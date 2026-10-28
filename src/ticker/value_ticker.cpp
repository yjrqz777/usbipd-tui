#include "value_ticker.hpp"

#include <chrono>
#include <thread>

#include "ftxui/component/app.hpp"
#include "ftxui/component/event.hpp"

void ValueTicker(std::stop_token stop,
                 std::atomic<std::int64_t>& value,
                 ftxui::App& screen) {
  using Clock = std::chrono::steady_clock;
  using namespace std::chrono_literals;

  const auto start = Clock::now();
  std::int64_t counted_ms = 0;
  auto last_redraw = start;

  while (!stop.stop_requested()) {
    std::this_thread::sleep_for(1ms);
    if (stop.stop_requested()) {
      break;
    }

    // 根据实际经过的时间补偿调度误差，不依赖 sleep_for 精确唤醒。
    const auto now = Clock::now();
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
    if (elapsed_ms > counted_ms) {
      value.fetch_add(elapsed_ms - counted_ms, std::memory_order_relaxed);
      counted_ms = elapsed_ms;
    }

    // 计数是 1ms 节拍，但终端无需每毫秒重绘。
    if (now - last_redraw >= 16ms) {
      last_redraw = now;
      screen.PostEvent(ftxui::Event::Custom);
    }
  }
}
