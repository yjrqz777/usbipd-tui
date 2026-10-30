#pragma once

#include <atomic>
#include <cstdint>
#include <stop_token>

namespace ftxui {
class App;
}

// 计数任务：按经过的毫秒数递增 value，约每 16ms 请求一次 TUI 刷新。
// 由 std::jthread 调用，结束时通过 stop_token 通知退出。
void ValueTicker(std::stop_token stop,
                 std::atomic<std::int64_t>& value,
                 ftxui::App& screen);
