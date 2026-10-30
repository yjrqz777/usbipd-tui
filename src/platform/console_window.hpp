#pragma once

#include <cstdint>

namespace platform {

// 保存并恢复宿主控制台状态。必须在 FTXUI Loop 之前创建，之后销毁。
class ConsoleWindow {
 public:
  ConsoleWindow();
  ~ConsoleWindow();

  ConsoleWindow(const ConsoleWindow&) = delete;
  ConsoleWindow& operator=(const ConsoleWindow&) = delete;

 private:
#ifdef _WIN32
  void* hwnd_ = nullptr;
  std::intptr_t original_style_ = 0;
  unsigned long original_input_mode_ = 0;
  bool style_changed_ = false;
  bool input_mode_changed_ = false;
#endif
};

// 鼠标按在自绘顶栏时，让传统 conhost 进入 Windows 系统拖动模式。
// right_reserved_columns 保留给右侧关闭按钮；Windows Terminal 不处理。
bool BeginTitleBarDrag(int mouse_column, int right_reserved_columns);

// 隐藏传统 conhost 的滚动条，不改变屏幕缓冲区尺寸。
// 在 FTXUI 初始化后调用一次，避免初始化过程重新显示滚动条。
void HideConsoleScrollBars();

// alt screen 安装完成后，在首次绘制前将光标归位。
void ResetCursor();

}  // namespace platform
