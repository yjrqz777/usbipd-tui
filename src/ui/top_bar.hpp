#pragma once

#include "ftxui/component/component.hpp"

namespace ftxui {
class App;
}

namespace ui {

inline constexpr int kTopBarRows = 1;

// 绘制单行顶部栏，右侧保留三个字符宽度用于关闭按钮。
ftxui::Component CreateTopBar(ftxui::App& app);

// 在整个应用布局上监听鼠标按下，避免内容区焦点影响顶栏拖动。
ftxui::Component WithTitleBarDrag(ftxui::Component layout);

}  // namespace ui
