#ifndef NET_DESIGN_RUNTIME_FIBER_H
#define NET_DESIGN_RUNTIME_FIBER_H

#include "runtime/context.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace net {

class Scheduler;

class Fiber : public std::enable_shared_from_this<Fiber> {
 public:
  using ptr = std::shared_ptr<Fiber>;

  enum class State {
    INIT,
    HOLD,
    EXEC,
    TERM,
    READY,
    EXCEPT,
  };

  static constexpr uint32_t kDefaultStackSize = 128 * 1024;

  Fiber();
  explicit Fiber(std::function<void()> cb, size_t stack_size = 0,
                 bool use_caller = false);
  ~Fiber();

  void reset(std::function<void()> cb);

  void swapIn();
  void swapOut();

  uint64_t id() const { return id_; }
  State state() const { return state_; }
  void setState(State s) { state_ = s; }

  static Fiber::ptr GetThis();
  static void YieldToHold();
  static void YieldToReady();
  static void SleepMs(uint64_t ms);
  static uint32_t GetFiberId();

 private:
  Context ctx_;
  void* stack_ = nullptr;
  uint64_t id_ = 0;
  uint32_t stack_size_ = 0;
  State state_ = State::INIT;
  std::function<void()> cb_;
};

}  // namespace net

#endif  // NET_DESIGN_RUNTIME_FIBER_H
