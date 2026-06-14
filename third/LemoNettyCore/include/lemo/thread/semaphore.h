#pragma once

#include "lemo/utils/noncopyable.h"

#include <cstdint>
#include <semaphore.h>

namespace lemo {
namespace thread {

class Semaphore : public utils::NonCopyable {
 public:
  explicit Semaphore(uint32_t count = 0);
  ~Semaphore();

  void wait();
  bool tryWait();
  bool timedWait(uint64_t timeout_ms);
  void notify();

 private:
  sem_t semaphore_;
};

}  // namespace thread
}  // namespace lemo
