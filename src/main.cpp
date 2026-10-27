// usbipd-tui
//
// 移植自 third_party/FTXUI/examples/component/button_animated.cpp
// (Copyright 2020 Arthur Sonzogni. All rights reserved. MIT license.)
//
// 这个程序做什么：
//   在控制台里跑一个 FTXUI 界面。窗口去掉系统标题栏，顶栏改由界面自己画（白色一行），
//   按住顶栏可以拖动窗口，点右边的 X 退出。
//
// 相对 FTXUI 示例的改动：
//   1. 用 App::Fullscreen()（alt screen）铺满终端，示例用的是 FitComponent()。
//   2. 进入 TUI 之前不向控制台输出任何内容。
//   3. 去掉系统标题栏，顶栏改由界面绘制，并提供拖动和关闭。
//   4. alt screen 装好、首帧绘制之前，把光标强制归位到左上角。
//
// 各函数分工：
//   MeasureTopBar()         量出顶栏一行的像素高度、关闭按钮的像素宽度
//   HitTestTopBar()         判定鼠标坐标落在顶栏的哪个部位
//   TopBarMouseProc()       鼠标钩子回调：拖动窗口，或请求退出
//   MouseHookThread()       钩子线程：装钩子，然后跑消息循环
//   InstallTopBarMouse()    量尺寸、起钩子线程，接管顶栏上的鼠标
//   RemoveNativeTitleBar()  去掉控制台窗口自带的系统标题栏
//   DisableQuickEdit()      关掉控制台的快速编辑，免得点一下就冻结输出
//   main()                  程序入口：准备控制台、搭界面、跑事件循环

#include <atomic>
#include <memory>
#include <string>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/loop.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
// FTXUI 头文件里有 min/max 宏检查，必须在 windows.h 之前关掉这两个宏。
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace ftxui;

// 顶栏固定占几行。布局和鼠标钩子的命中判定共用这一个值：
// 布局按它固定顶栏高度，钩子按「行数 x 行高」换算成像素。
constexpr int kTopBarRows = 1;

#if defined(_WIN32)
namespace {

// 关闭按钮占的列数，等于 " X " 三个字符的宽度。
constexpr int kCloseColumns = 3;

// 顶栏的拖动状态。只由钩子线程读写，主线程不碰。
struct TopBarState {
  bool dragging = false;  // 是否正在拖动
  POINT grab_cursor{};    // 按下时的光标位置，作为位移基准
  RECT grab_window{};     // 按下时的窗口位置，作为移动基准
  int bar_height = 20;    // 顶栏的像素高度，量不到时用这个默认值
  int close_width = 60;   // 关闭按钮的像素宽度，量不到时用这个默认值
};

TopBarState g_top_bar;
HHOOK g_mouse_hook = nullptr;      // 钩子句柄，只在钩子线程里用
std::atomic<App*> g_app{nullptr};  // 主循环指针，供钩子线程投递退出请求

// 量出顶栏的像素尺寸：
//   bar_height  = 一行文字的像素高度（布局里顶栏正好占一行）
//   close_width = 关闭按钮的像素宽度（3 个字符）
// 算法是「客户区尺寸 / 终端行列数」，这样 DPI 和字号变化时也跟着对。
// 量不到时保留结构体里的默认值。
void MeasureTopBar(HWND hwnd) {
  RECT client{};
  if (GetClientRect(hwnd, &client) == 0) {
    return;
  }
  CONSOLE_SCREEN_BUFFER_INFO info{};
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info) == 0) {
    return;
  }

  const int columns = info.srWindow.Right - info.srWindow.Left + 1;
  const int rows = info.srWindow.Bottom - info.srWindow.Top + 1;
  if (columns <= 0 || rows <= 0) {
    return;
  }

  const int width = client.right - client.left;
  const int height = client.bottom - client.top;
  const int row_height = height / rows;
  if (row_height > 0) {
    g_top_bar.bar_height = kTopBarRows * row_height;
  }
  if (width / columns > 0) {
    g_top_bar.close_width = kCloseColumns * (width / columns);
  }
}

// 判定鼠标屏幕坐标落在顶栏的哪一段：
//   0 = 不在顶栏（坐标被别的窗口盖住也算不在）；
//   1 = 拖动区；
//   2 = 关闭按钮。
int HitTestTopBar(HWND hwnd, POINT screen) {
  // 该坐标下必须就是控制台窗口本身。
  if (GetAncestor(WindowFromPoint(screen), GA_ROOT) != hwnd) {
    return 0;
  }

  POINT pt = screen;
  if (ScreenToClient(hwnd, &pt) == 0) {
    return 0;
  }
  RECT client{};
  if (GetClientRect(hwnd, &client) == 0) {
    return 0;
  }

  // 垂直方向落在顶栏那一行，水平方向落在客户区内。
  if (pt.y < 0 || pt.y >= g_top_bar.bar_height) {
    return 0;
  }
  if (pt.x < 0 || pt.x >= client.right) {
    return 0;
  }
  // 最右边 close_width 像素归关闭按钮，其余都是拖动区。
  return pt.x >= client.right - g_top_bar.close_width ? 2 : 1;
}

// 鼠标钩子回调，负责顶栏上的全部鼠标操作：
//   左键按在关闭按钮上：把退出请求投回主循环，吞掉这次点击；
//   左键按在拖动区：   记下光标和窗口位置，开始拖动，吞掉这次按下；
//   拖动中鼠标移动：   按真实光标位置异步移动窗口；
//   松开左键：         结束拖动。
LRESULT CALLBACK TopBarMouseProc(int code, WPARAM wparam, LPARAM lparam) {  // NOLINT
  if (code != HC_ACTION) {
    return CallNextHookEx(g_mouse_hook, code, wparam, lparam);
  }

  HWND hwnd = GetConsoleWindow();
  if (hwnd == nullptr) {
    return CallNextHookEx(g_mouse_hook, code, wparam, lparam);
  }

  const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lparam);  // NOLINT
  const bool dragging = g_top_bar.dragging;

  if (wparam == WM_LBUTTONDOWN) {
    const int hit = HitTestTopBar(hwnd, info->pt);
    if (hit == 2) {
      // 点在关闭按钮上：交给主循环退出。
      if (App* app = g_app.load()) {
        app->Post([app] { app->Exit(); });
      }
      return 1;
    }
    if (hit == 1) {
      // 点在拖动区：记下基准位置，进入拖动状态。
      POINT cursor{};
      GetCursorPos(&cursor);
      g_top_bar.dragging = true;
      g_top_bar.grab_cursor = cursor;
      GetWindowRect(hwnd, &g_top_bar.grab_window);
      // 只吞掉“按下”：TUI 收不到按下就不会进入文本选择模式。
      return 1;
    }
  } else if (wparam == WM_MOUSEMOVE) {
    if (dragging) {
      // 用系统真实光标位置算目标位置。这里不吞事件：吞掉“移动”会让系统不移动
      // 指针，窗口只能跟着 ±1px 的抖动走。
      POINT cursor{};
      GetCursorPos(&cursor);
      SetWindowPos(hwnd, nullptr,
                   g_top_bar.grab_window.left + cursor.x - g_top_bar.grab_cursor.x,
                   g_top_bar.grab_window.top + cursor.y - g_top_bar.grab_cursor.y, 0, 0,
                   SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
    }
  } else if (wparam == WM_LBUTTONUP) {
    if (dragging) {
      // 松开左键：结束拖动，并吞掉这次抬起。
      g_top_bar.dragging = false;
      return 1;
    }
  }

  return CallNextHookEx(g_mouse_hook, code, wparam, lparam);
}

// 钩子线程：低级鼠标钩子必须由带消息循环的线程安装和维持，
// 所以这里装上钩子后一直跑消息循环，直到进程结束。
DWORD WINAPI MouseHookThread(LPVOID /*unused*/) {
  g_mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, TopBarMouseProc,
                                   GetModuleHandleW(nullptr), 0);
  if (g_mouse_hook == nullptr) {
    return 0;
  }

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  UnhookWindowsHookEx(g_mouse_hook);
  g_mouse_hook = nullptr;
  return 0;
}

// 接管顶栏上的鼠标：先量好顶栏尺寸，再起一个线程装钩子。
// 必须在 Loop 构造之后调用，那时窗口尺寸才是最终的。
void InstallTopBarMouse(App* app) {
  HWND hwnd = GetConsoleWindow();
  if (hwnd == nullptr) {
    return;
  }

  g_app.store(app);
  MeasureTopBar(hwnd);

  HANDLE thread = CreateThread(nullptr, 0, MouseHookThread, nullptr, 0, nullptr);
  if (thread != nullptr) {
    CloseHandle(thread);
  }
}

// 去掉控制台窗口自带的系统标题栏：清掉标题、系统菜单、边框、最小化/最大化按钮的样式位，
// 窗口就只剩客户区，标题栏由 TUI 自己画。
//
// 只改样式，不做 ShowWindow(SW_MAXIMIZE)：窗口属于 conhost 进程，最大化会在
// 跨进程同步消息里让它重排控制台缓冲区，两边互等，窗口直接“无响应”。
// 这段只对传统 conhost 生效，Windows Terminal 那类伪终端没有可去掉的原生标题栏。
void RemoveNativeTitleBar() {
  HWND hwnd = GetConsoleWindow();
  if (hwnd == nullptr) {
    return;
  }

  LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
  style &= ~static_cast<LONG_PTR>(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
                                  WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
  SetWindowLongPtrW(hwnd, GWL_STYLE, style);

  // 让新样式立即生效，客户区才会铺满整个窗口。
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                   SWP_NOACTIVATE);
}

// 关掉控制台的快速编辑。快速编辑开着时，鼠标一点窗口就进入选择文本模式，
// 控制台输出被挂起，整个界面看起来像卡死。
void DisableQuickEdit() {
  HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode = 0;
  if (GetConsoleMode(input, &mode) == 0) {
    return;
  }
  mode &= ~static_cast<DWORD>(ENABLE_QUICK_EDIT_MODE);
  mode |= static_cast<DWORD>(ENABLE_EXTENDED_FLAGS);
  SetConsoleMode(input, mode);
}

}  // namespace
#endif

// 程序入口。顺序是：准备控制台 -> 搭界面 -> 进 TUI 跑循环。
int main() {
  // ---- 1. 控制台准备：都要在创建 App 之前做完 ----
#if defined(_WIN32)
  // 控制台默认用本地代码页，切成 UTF-8 才能正常显示中文和边框字符。
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  SetConsoleTitleW(L"test 笨蛋");

  // 先改窗口样式，因为 App::Fullscreen() 会读取终端尺寸。
  RemoveNativeTitleBar();
  DisableQuickEdit();
#endif

  // ---- 2. 搭界面 ----
  auto screen = App::Fullscreen();

  int value = 50;

  // 三个动画按钮：左右方向键切换焦点，回车触发。
  auto buttons = Container::Horizontal({
      Button(
          "Decrease", [&] { value--; }, ButtonOption::Animated(Color::Red)),
      Button(
          "Reset", [&] { value = 50; }, ButtonOption::Animated(Color::Green)),
      Button(
          "Increase", [&] { value++; }, ButtonOption::Animated(Color::Blue)),
  });

  // 内容区：value 文本、分隔线、进度条，外面套一圈边框。
  auto content = Renderer(buttons, [&] {
    return vbox({
        vbox({
            text("value = " + std::to_string(value)),
            separator(),
            gauge(value * 0.01f),
        }) | border,
        buttons->Render(),
    });
  });

  // 关闭按钮的样式：红色加粗的 " X "，聚焦时反色。
  // 特意做成一行高（不用带 borderEmpty 的 Animated 样式），顶栏才正好占
  // kTopBarRows 行，和钩子里的命中判定对齐。
  ButtonOption close_option;
  close_option.transform = [](const EntryState& state) {
    auto element = text(state.label) | color(Color::Red) | bold;
    if (state.focused) {
      element |= inverted;
    }
    return element;
  };
  auto close_button = Button(" X ", [&] { screen.Exit(); }, close_option);

  // TUI 自绘的顶栏：白色底，左边标题，右边关闭按钮；这一行可以按住拖动窗口。
  auto title_bar = Renderer(close_button, [&] {
    return hbox({
               text(" usbipd-tui ") | color(Color::Black) | bold,
               filler(),
               close_button->Render(),
           }) |
           bgcolor(Color::White);
  });

  // 焦点默认落在内容区（索引 1），免得启动后按回车直接把窗口关掉。
  int focus = 1;

  // 整体布局：顶栏（固定 kTopBarRows 行）+ 分隔线 + 内容区。
  auto layout = Renderer(Container::Vertical({title_bar, content}, &focus), [&] {
    return vbox({
        title_bar->Render() | size(HEIGHT, EQUAL, kTopBarRows),
        separator(),
        content->Render() | flex,
    });
  });

  // ---- 3. 进 TUI：装 alt screen，然后跑事件循环 ----

  // Loop 的构造函数会执行 PreMain()：装 alt screen、切控制台模式。
  // 析构时才恢复终端状态，所以它必须活到循环结束。
  Loop loop(&screen, layout);

#if defined(_WIN32)
  // 此时 alt screen 已经装好、首帧还没画，把光标按到左上角，
  // 免得首帧从换行之后的位置开始绘制。
  SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), COORD{0, 0});

  // 无边框窗口没有系统标题栏，顶栏的拖动和关闭交给鼠标钩子。
  InstallTopBarMouse(&screen);
#endif

  loop.Run();

#if defined(_WIN32)
  // 循环结束后钩子线程可能还在，清掉指针，避免往已销毁的 App 投任务。
  g_app.store(nullptr);
#endif
  return 0;
}
