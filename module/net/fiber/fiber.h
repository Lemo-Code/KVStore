#ifndef NET_FIBER_FIBER_H
#define NET_FIBER_FIBER_H

#include "thread/noncopyable.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <ucontext.h>

namespace net {

class Scheduler;

/**
 * @brief 基于 ucontext 的用户态协程（非对称模型）。
 *
 * 每个 pthread 线程拥有一个主协程（无独立栈），可创建多个子协程（独立栈）。
 * 子协程通过 swapIn/swapOut 与调度器主协程或线程主协程切换。
 */
class Fiber : public std::enable_shared_from_this<Fiber> {
 public:
  friend class Scheduler;

  typedef std::shared_ptr<Fiber> ptr;

  enum State {
    INIT = 0,
    HOLD = 1,
    EXEC = 2,
    TERM = 3,
    READY = 4,
    EXCEPT = 5,
  };

  /** 默认子协程栈大小（字节） */
  static constexpr uint32_t kDefaultStackSize = 128 * 1024;

  Fiber();
  Fiber(std::function<void()> cb, size_t stacksize = 0, bool use_caller = false);
  ~Fiber();

  void reset(std::function<void()> cb);

  void swapOut();
  void swapIn();
  void call();
  void back();

  uint64_t getId() const { return id_; }
  State getState() const { return state_; }
  void setState(State state) { state_ = state; }

  static void MainFunc();
  static void CallerMainFunc();
  static Fiber::ptr GetThis();
  static void SetThis(Fiber* fiber);
  static void YieldToHold();
  static void YieldToReady();
  static void SleepMs(uint64_t ms);
  static uint64_t GetTotalCount();
  static uint32_t GetFiberId();

 private:
  static Fiber* GetSwapMainFiber();

  ucontext_t ctx_;
  void* stack_ = nullptr;

  uint64_t id_ = 0;
  uint32_t stack_size_ = 0;
  State state_ = INIT;
  std::function<void()> cb_;
};

}  // namespace net

#endif  // NET_FIBER_FIBER_H
