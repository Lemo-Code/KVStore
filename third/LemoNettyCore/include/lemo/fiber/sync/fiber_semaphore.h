#pragma once

#include "lemo/fiber/fiber.h"
#include "lemo/fiber/scheduler.h"
#include "lemo/nettycore_export.h"
#include "lemo/thread/mutex.h"
#include "lemo/utils/noncopyable.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <utility>

namespace lemo {
namespace fiber {
namespace sync {

/**
 * @brief 协程信号量：P(wait) 阻塞 yield，V(notify) 唤醒等待协程。
 */
class LEMO_NETTYCORE_API FiberSemaphore : public utils::NonCopyable {
 public:
  explicit FiberSemaphore(size_t initial_count = 0);
  ~FiberSemaphore();

  bool tryWait();
  void wait();
  void notify();
  void notifyN(size_t n);

  size_t count() const;

 private:
  typedef thread::Mutex MutexType;

  mutable MutexType mutex_;
  size_t count_ = 0;
  std::list<std::pair<Scheduler*, Fiber::ptr>> waiters_;
};

}  // namespace sync
}  // namespace fiber
}  // namespace lemo
