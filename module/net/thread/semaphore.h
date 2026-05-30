#ifndef NET_THREAD_SEMAPHORE_H
#define NET_THREAD_SEMAPHORE_H

#include "noncopyable.h"

#include <cstdint>
#include <semaphore.h>

namespace net {

/** POSIX 计数信号量，用于线程启动同步等场景。 */
class Semaphore : Noncopyable {
 public:
  explicit Semaphore(uint32_t count = 0);
  ~Semaphore();

  void wait();
  /** @return true 表示成功减计数，false 表示当前为 0 */
  bool tryWait();
  /** @return true 表示在超时前被唤醒 */
  bool timedWait(uint64_t timeout_ms);
  void notify();

 private:
  sem_t semaphore_;
};

}  // namespace net

#endif  // NET_THREAD_SEMAPHORE_H
