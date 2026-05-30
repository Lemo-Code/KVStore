#include "semaphore.h"

#include <ctime>
#include <stdexcept>

namespace net {

Semaphore::Semaphore(uint32_t count) {
  if (sem_init(&semaphore_, 0, count) != 0) {
    throw std::logic_error("sem_init error");
  }
}

Semaphore::~Semaphore() { sem_destroy(&semaphore_); }

void Semaphore::wait() {
  if (sem_wait(&semaphore_) != 0) {
    throw std::logic_error("sem_wait error");
  }
}

bool Semaphore::tryWait() {
  if (sem_trywait(&semaphore_) != 0) {
    return false;
  }
  return true;
}

bool Semaphore::timedWait(uint64_t timeout_ms) {
  if (timeout_ms == 0) {
    return tryWait();
  }
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    return false;
  }
  const uint64_t nsec = static_cast<uint64_t>(ts.tv_nsec) +
                        (timeout_ms % 1000) * 1000000ull;
  ts.tv_sec += static_cast<time_t>(timeout_ms / 1000 + nsec / 1000000000ull);
  ts.tv_nsec = static_cast<long>(nsec % 1000000000ull);
  if (sem_timedwait(&semaphore_, &ts) != 0) {
    return false;
  }
  return true;
}

void Semaphore::notify() {
  if (sem_post(&semaphore_) != 0) {
    throw std::logic_error("sem_post error");
  }
}

}  // namespace net
