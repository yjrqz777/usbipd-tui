// usbipd-tui
//
// FTXUI 负责界面及输入；传统 Windows conhost 的无边框窗口由
// platform::ConsoleWindow 管理。顶栏绘制与拖动事件见 ui/top_bar.cpp。

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include "ftxui/component/event.hpp"

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/loop.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

#include "platform/console_window.hpp"
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

  // 每经过 1ms，value 增加 1；计数使用单调时钟补偿 Windows 定时器抖动。
  // 终端只需要约 60Hz 的刷新，不应以 1kHz 重绘。
  std::jthread ticker([&](std::stop_token stop) {
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    const auto start = Clock::now();
    auto counted_ms = std::int64_t{0};
    auto last_redraw = start;

    while (!stop.stop_requested()) {
      std::this_thread::sleep_for(1ms);
      if (stop.stop_requested()) {
        break;
      }

      const auto now = Clock::now();
      const auto elapsed_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
      if (elapsed_ms > counted_ms) {
        value.fetch_add(elapsed_ms - counted_ms, std::memory_order_relaxed);
        counted_ms = elapsed_ms;
      }

      if (now - last_redraw >= 16ms) {
        last_redraw = now;
        screen.PostEvent(Event::Custom);
      }
    }
  });

  loop.Run();
  ticker.request_stop();
  ticker.join();
  return 0;
}
