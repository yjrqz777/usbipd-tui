#include "ui/top_bar.hpp"

#include <utility>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/mouse.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "platform/console_window.hpp"

namespace ui {
namespace {

constexpr int kCloseColumns = 3;  // " X " 的字符宽度

}  // namespace

ftxui::Component CreateTopBar(ftxui::App& app) {
  using namespace ftxui;

  ButtonOption option;
  option.transform = [](const EntryState& state) {
    auto element = text(state.label) | color(Color::Red) | bold;
    if (state.focused) {
      element |= inverted;
    }
    return element;
  };

  auto close_button = Button(" X ", [&app] { app.Exit(); }, option);

  return Renderer(close_button, [close_button] {
    return hbox({
               text(" usbipd-tui ") | color(Color::Black) | bold,
               filler(),
               close_button->Render(),
           }) |
           bgcolor(Color::White);
  });
}

ftxui::Component WithTitleBarDrag(ftxui::Component layout) {
  using namespace ftxui;

  return CatchEvent(std::move(layout), [](Event event) {
    if (!event.is_mouse()) {
      return false;
    }
    const auto& mouse = event.mouse();
    if (mouse.button != Mouse::Left || mouse.motion != Mouse::Pressed ||
        mouse.y < 0 || mouse.y >= kTopBarRows) {
      return false;
    }

    // 右侧关闭按钮不拦截，继续由 FTXUI Button 处理。
    return platform::BeginTitleBarDrag(mouse.x, kCloseColumns);
  });
}

}  // namespace ui
