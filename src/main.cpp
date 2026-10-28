// usbipd-tui
//
// FTXUI 负责界面及输入；传统 Windows conhost 的无边框窗口由
// platform::ConsoleWindow 管理。顶栏绘制与拖动事件见 ui/top_bar.cpp。

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/loop.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

#include "platform/console_window.hpp"
#include "value_ticker.hpp"
#include "ui/top_bar.hpp"

using namespace ftxui;

int main() {
  // 先修改 conhost 样式，再让 FTXUI 读取窗口尺寸。
  // 该对象最后析构，保证 Loop 恢复终端后再恢复宿主控制台样式。
  platform::ConsoleWindow console;

  auto screen = App::Fullscreen();
  std::atomic<std::int64_t> value{50};

  auto buttons = Container::Horizontal({
      Button("Decrease", [&] { value.fetch_sub(1, std::memory_order_relaxed); },
             ButtonOption::Animated(Color::Red)),
      Button("Reset", [&] { value.store(50, std::memory_order_relaxed); },
             ButtonOption::Animated(Color::Green)),
      Button("Increase", [&] { value.fetch_add(1, std::memory_order_relaxed); },
             ButtonOption::Animated(Color::Blue)),
  });

  auto content = Renderer(buttons, [&] {
    const auto current = value.load(std::memory_order_relaxed);
    return vbox({
        vbox({
            text("value = " + std::to_string(current)),
            separator(),
            gauge(current * 0.01f),
        }) | border,
        buttons->Render(),
    });
  });

  auto title_bar = ui::CreateTopBar(screen);

  // 默认聚焦内容区，防止启动后直接按回车关闭窗口。
  int focus = 1;
  auto layout = Renderer(Container::Vertical({title_bar, content}, &focus), [&] {
    return vbox({
        title_bar->Render() | size(HEIGHT, EQUAL, ui::kTopBarRows),
        separator(),
        content->Render() | flex,
    });
  });

  auto root = ui::WithTitleBarDrag(layout);
  Loop loop(&screen, root);

  // FTXUI 切入 alt screen 后，再隐藏 conhost 的系统滚动条。
  platform::HideConsoleScrollBars();

  // alt screen 已安装，首帧绘制尚未开始。
  platform::ResetCursor();

  // 计数线程由独立函数实现，jthread 析构时自动请求停止并等待退出。
  std::jthread ticker(ValueTicker, std::ref(value), std::ref(screen));
  loop.Run();
  return 0;
}
