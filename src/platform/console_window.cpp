#include "platform/console_window.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace platform {

ConsoleWindow::ConsoleWindow() {
#if defined(_WIN32)
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  SetConsoleTitleW(L"test 笨蛋");

  HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode = 0;
  if (GetConsoleMode(input, &mode) != 0) {
    const DWORD new_mode =
        (mode | ENABLE_EXTENDED_FLAGS) & ~static_cast<DWORD>(ENABLE_QUICK_EDIT_MODE);
    if (SetConsoleMode(input, new_mode) != 0) {
      original_input_mode_ = mode;
      input_mode_changed_ = true;
    }
  }

  // GetConsoleWindow() 在 Windows Terminal 中不代表可见的宿主窗口。
  HWND hwnd = GetConsoleWindow();
  if (hwnd == nullptr || IsWindowVisible(hwnd) == 0) {
    return;
  }

  SetLastError(0);
  const LONG_PTR original = GetWindowLongPtrW(hwnd, GWL_STYLE);
  if (original == 0 && GetLastError() != ERROR_SUCCESS) {
    return;
  }

  const LONG_PTR style =
      original & ~static_cast<LONG_PTR>(WS_CAPTION | WS_SYSMENU |
                                        WS_THICKFRAME | WS_MINIMIZEBOX |
                                        WS_MAXIMIZEBOX);
  if (style == original) {
    return;
  }

  SetLastError(0);
  const LONG_PTR previous = SetWindowLongPtrW(hwnd, GWL_STYLE, style);
  if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
    return;
  }

  hwnd_ = hwnd;
  original_style_ = static_cast<std::intptr_t>(original);
  style_changed_ = true;
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                   SWP_NOZORDER | SWP_NOACTIVATE);
#endif
}

ConsoleWindow::~ConsoleWindow() {
#if defined(_WIN32)
  // conhost 可能是用户原本打开的 CMD 窗口，程序退出时必须恢复样式。
  HWND hwnd = static_cast<HWND>(hwnd_);
  if (style_changed_ && IsWindow(hwnd) != 0) {
    SetWindowLongPtrW(hwnd, GWL_STYLE,
                      static_cast<LONG_PTR>(original_style_));
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                     SWP_NOZORDER | SWP_NOACTIVATE);
  }

  if (input_mode_changed_) {
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),
                   static_cast<DWORD>(original_input_mode_));
  }
#endif
}

bool BeginTitleBarDrag(int mouse_column, int right_reserved_columns) {
#if defined(_WIN32)
  if (mouse_column < 0 || right_reserved_columns < 0) {
    return false;
  }

  HWND hwnd = GetConsoleWindow();
  if (hwnd == nullptr || IsWindowVisible(hwnd) == 0) {
    return false;
  }

  CONSOLE_SCREEN_BUFFER_INFO info{};
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info) == 0) {
    return false;
  }
  const int columns = info.srWindow.Right - info.srWindow.Left + 1;
  if (columns <= right_reserved_columns ||
      mouse_column >= columns - right_reserved_columns) {
    return false;
  }

  // 交给系统完成后续拖动，不再安装 WH_MOUSE_LL 或自行计算位移。
  // 这是对 conhost 的实验性方案，实际拖动效果需要 Windows 实机验证。
  return PostMessageW(hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0) != 0;
#else
  (void)mouse_column;
  (void)right_reserved_columns;
  return false;
#endif
}

void ResetCursor() {
#if defined(_WIN32)
  SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), COORD{0, 0});
#endif
}

}  // namespace platform
