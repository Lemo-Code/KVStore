#pragma once

#include "lemo/fiber/fiber.h"

#include <functional>

namespace lemo {
namespace fiber {

/**
 * @brief 线程本地协程池：复用栈与 ucontext，避免 IO 往返后反复分配。
 */
class FiberPool {
 public:
  static constexpr size_t kDefaultMaxSize = 256;

  static Fiber::ptr acquire(std::function<void()> cb);
  static void release(Fiber::ptr& fiber);
  static size_t cachedCount();
};

}  // namespace fiber
}  // namespace lemo
