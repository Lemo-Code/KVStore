#ifndef NET_DESIGN_RUNTIME_SCHEDULER_H
#define NET_DESIGN_RUNTIME_SCHEDULER_H

#include "runtime/fiber.h"
#include "runtime/timer.h"

#include <functional>
#include <memory>
#include <string>

namespace net {

/**
 * @brief Go 风格 GMP 调度器（仅负责协程任务分发，不含 epoll）。
 */
class Scheduler {
 public:
  using ptr = std::shared_ptr<Scheduler>;

  Scheduler(size_t threads = 1, bool use_caller = true,
            const std::string& name = "");
  virtual ~Scheduler();

  void start();
  void stop();

  template <class FiberOrCb>
  void schedule(FiberOrCb fc, int pin_thread = -1);

  TimerWheel& timer() { return timer_; }
  const TimerWheel& timer() const { return timer_; }

  static Scheduler* GetThis();
  static size_t GetProcessorId();

 protected:
  virtual void idle();
  virtual void tickle();
  virtual bool stopping();

 private:
  TimerWheel timer_;
  std::string name_;
};

}  // namespace net

#endif  // NET_DESIGN_RUNTIME_SCHEDULER_H
