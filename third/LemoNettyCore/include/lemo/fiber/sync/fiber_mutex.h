#pragma once

#include "lemo/fiber/fiber.h"
#include "lemo/fiber/scheduler.h"
#include "lemo/nettycore_export.h"
#include "lemo/thread/mutex.h"
#include "lemo/utils/noncopyable.h"

#include <cstdint>
#include <list>
#include <utility>

namespace lemo {
namespace fiber {
namespace sync {

/**
 * @brief 协程互斥锁：阻塞时 yield，唤醒时 schedule(READY)。
 */
class LEMO_NETTYCORE_API FiberMutex : public utils::NonCopyable {
 public:
  FiberMutex() = default;
  ~FiberMutex();

  void lock();
  void unlock();
  bool try_lock();

 private:
  typedef thread::Mutex MutexType;

  MutexType mutex_;
  bool locked_ = false;
  std::list<std::pair<Scheduler*, Fiber::ptr>> waiters_;
};

}  // namespace sync
}  // namespace fiber
}  // namespace lemo
