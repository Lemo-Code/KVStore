#ifndef NET_DESIGN_RUNTIME_TIMER_H
#define NET_DESIGN_RUNTIME_TIMER_H

#include <cstdint>
#include <functional>
#include <memory>

namespace net {

using TimerId = uint64_t;

class Timer {
 public:
  using ptr = std::shared_ptr<Timer>;
  virtual ~Timer() = default;
  virtual void cancel() = 0;
};

/**
 * @brief 分层时间轮定时器（组合入 Scheduler，非继承）。
 */
class TimerWheel {
 public:
  TimerId addTimer(uint64_t delay_ms, std::function<void()> cb);
  Timer::ptr addTimerPtr(uint64_t delay_ms, std::function<void()> cb,
                         bool recurring = false);
  bool cancel(TimerId id);

  /** 距最近到期毫秒数；无定时器时返回 UINT64_MAX */
  uint64_t nextTimeoutMs() const;

  void tick(uint64_t now_ms);
};

}  // namespace net

#endif  // NET_DESIGN_RUNTIME_TIMER_H
