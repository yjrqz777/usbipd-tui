// usbipd-tui
//
// FTXUI 负责界面及输入；传统 Windows conhost 的无边框窗口由
// platform::ConsoleWindow 管理。顶栏绘制与拖动事件见 ui/top_bar.cpp。

#include <atomic>      /* std::atomic：跨线程共享的原子变量 */
#include <cstdint>     /* std::int64_t：固定宽度 64 位整数 */
#include <functional>  /* std::ref：把对象引用传给线程函数 */
#include <string>      /* std::to_string：把数字转换成字符串 */
#include <thread>      /* std::jthread：析构时自动请求停止并 join */
#include <filesystem>  /* std::filesystem::path：日志路径与目录创建 */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/loop.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

#include <spdlog/sinks/rotating_file_sink.h> /* 滚动文件 sink */
#include <spdlog/spdlog.h>                   /* spdlog 日志接口 */

#include "platform/console_window.hpp"
#include "ticker/value_ticker.hpp"
#include "ui/top_bar.hpp"

using namespace ftxui;

int main()
{
    /* 日志目录需要自己创建，spdlog 不会自动创建目录。 */
  const std::filesystem::path log_dir = "./";      /* 日志目录：当前工作目录 */
  std::filesystem::create_directories(log_dir);    /* 确保目录存在；"./" 通常已存在 */

  /* 5 MB 一个文件，最多保留 3 个；_mt 表示线程安全。 */
  auto logger = spdlog::rotating_logger_mt(
      "usbipd-tui",                          /* logger 名称 */
      (log_dir / "usbipd-tui.log").string(), /* 日志文件路径 */
      5 * 1024 * 1024,                       /* 单文件上限：5 MB */
      3);                                    /* 最多保留 3 个历史文件 */

  logger->set_level(spdlog::level::debug);  /* 记录 debug 及以上级别 */
  logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v"); /* 时间、级别、线程号、正文 */
  logger->flush_on(spdlog::level::warn);    /* warning 及以上立即写盘 */

  /* 让 spdlog::info()/error() 默认走这个文件 logger。 */
  spdlog::set_default_logger(logger);

  spdlog::info("usbipd-tui started");       /* 记录程序启动 */



  const auto result = platform::RunCommand(L"usbipd.exe list");

  if (!result.launched) {
    spdlog::error("failed to run command, error={}", result.error);
  } else {
    spdlog::info("exit_code={}", result.exit_code);
    spdlog::info("output=\n{}", result.output);
  }
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
