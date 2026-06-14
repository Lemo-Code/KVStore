#include "lemo/fiber/fiber_pool.h"

#include <vector>

namespace lemo {
namespace fiber {

namespace {

thread_local std::vector<Fiber::ptr> t_fiber_pool;

}  // namespace

Fiber::ptr FiberPool::acquire(std::function<void()> cb) {
  Fiber::ptr fiber;
  if (!t_fiber_pool.empty()) {
    fiber = std::move(t_fiber_pool.back());
    t_fiber_pool.pop_back();
    fiber->reset(std::move(cb));
  } else {
    fiber.reset(new Fiber(std::move(cb)));
  }
  return fiber;
}

void FiberPool::release(Fiber::ptr& fiber) {
  if (!fiber) {
    return;
  }
  const Fiber::State st = fiber->getState();
  if (st == Fiber::EXCEPT) {
    fiber.reset();
    return;
  }
  if (st != Fiber::TERM && st != Fiber::INIT) {
    return;
  }
  fiber->reset(nullptr);
  if (t_fiber_pool.size() < kDefaultMaxSize) {
    t_fiber_pool.push_back(std::move(fiber));
  } else {
    fiber.reset();
  }
}

size_t FiberPool::cachedCount() { return t_fiber_pool.size(); }

}  // namespace fiber
}  // namespace lemo
