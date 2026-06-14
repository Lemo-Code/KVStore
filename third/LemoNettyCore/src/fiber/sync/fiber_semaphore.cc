#include "lemo/fiber/sync/fiber_semaphore.h"

#include <cassert>

namespace lemo {
namespace fiber {
namespace sync {

FiberSemaphore::FiberSemaphore(size_t initial_count) : count_(initial_count) {}

FiberSemaphore::~FiberSemaphore() { assert(waiters_.empty()); }

size_t FiberSemaphore::count() const {
  MutexType::Lock lock(mutex_);
  return count_;
}

bool FiberSemaphore::tryWait() {
  MutexType::Lock lock(mutex_);
  if (count_ > 0) {
    --count_;
    return true;
  }
  return false;
}

void FiberSemaphore::wait() {
  {
    MutexType::Lock lock(mutex_);
    if (count_ > 0) {
      --count_;
      return;
    }
    Scheduler* sched = Scheduler::GetThis();
    Fiber::ptr fiber = Fiber::GetThis();
    assert(sched != nullptr && fiber != nullptr);
    waiters_.emplace_back(sched, fiber);
  }
  Fiber::YieldToHold();
}

void FiberSemaphore::notify() {
  MutexType::Lock lock(mutex_);
  if (!waiters_.empty()) {
    std::pair<Scheduler*, Fiber::ptr> next = waiters_.front();
    waiters_.pop_front();
    lock.unlock();
    next.first->schedule(next.second);
    return;
  }
  ++count_;
}

void FiberSemaphore::notifyN(size_t n) {
  for (size_t i = 0; i < n; ++i) {
    notify();
  }
}

}  // namespace sync
}  // namespace fiber
}  // namespace lemo
