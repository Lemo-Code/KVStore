#include "lemo/fiber/sync/fiber_mutex.h"

#include <cassert>

namespace lemo {
namespace fiber {
namespace sync {

FiberMutex::~FiberMutex() { assert(waiters_.empty()); }

bool FiberMutex::try_lock() {
  MutexType::Lock lock(mutex_);
  if (!locked_) {
    locked_ = true;
    return true;
  }
  return false;
}

void FiberMutex::lock() {
  {
    MutexType::Lock lock(mutex_);
    if (!locked_) {
      locked_ = true;
      return;
    }
    Scheduler* sched = Scheduler::GetThis();
    Fiber::ptr fiber = Fiber::GetThis();
    assert(sched != nullptr && fiber != nullptr);
    waiters_.emplace_back(sched, fiber);
  }
  Fiber::YieldToHold();
}

void FiberMutex::unlock() {
  MutexType::Lock lock(mutex_);
  if (!waiters_.empty()) {
    std::pair<Scheduler*, Fiber::ptr> next = waiters_.front();
    waiters_.pop_front();
    lock.unlock();
    next.first->schedule(next.second);
    return;
  }
  locked_ = false;
}

}  // namespace sync
}  // namespace fiber
}  // namespace lemo
