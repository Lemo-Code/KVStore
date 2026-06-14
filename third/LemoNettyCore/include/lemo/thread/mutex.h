#pragma once

#include "lemo/utils/noncopyable.h"

#include <atomic>
#include <pthread.h>

namespace lemo {
namespace thread {

template <class T>
struct ScopedLockImpl {
 public:
  explicit ScopedLockImpl(T& mutex) : mutex_(mutex) {
    mutex_.lock();
    locked_ = true;
  }

  ~ScopedLockImpl() { unlock(); }

  void lock() {
    if (!locked_) {
      mutex_.lock();
      locked_ = true;
    }
  }

  void unlock() {
    if (locked_) {
      mutex_.unlock();
      locked_ = false;
    }
  }

 private:
  T& mutex_;
  bool locked_ = false;
};

template <class T>
struct ReadScopedLockImpl {
 public:
  explicit ReadScopedLockImpl(T& mutex) : mutex_(mutex) {
    mutex_.rdlock();
    locked_ = true;
  }

  ~ReadScopedLockImpl() { unlock(); }

  void lock() {
    if (!locked_) {
      mutex_.rdlock();
      locked_ = true;
    }
  }

  void unlock() {
    if (locked_) {
      mutex_.unlock();
      locked_ = false;
    }
  }

 private:
  T& mutex_;
  bool locked_ = false;
};

template <class T>
struct WriteScopedLockImpl {
 public:
  explicit WriteScopedLockImpl(T& mutex) : mutex_(mutex) {
    mutex_.wrlock();
    locked_ = true;
  }

  ~WriteScopedLockImpl() { unlock(); }

  void lock() {
    if (!locked_) {
      mutex_.wrlock();
      locked_ = true;
    }
  }

  void unlock() {
    if (locked_) {
      mutex_.unlock();
      locked_ = false;
    }
  }

 private:
  T& mutex_;
  bool locked_ = false;
};

class Mutex : public utils::NonCopyable {
 public:
  typedef ScopedLockImpl<Mutex> Lock;

  Mutex() { pthread_mutex_init(&lock_, nullptr); }
  ~Mutex() { pthread_mutex_destroy(&lock_); }

  void lock() { pthread_mutex_lock(&lock_); }
  void unlock() { pthread_mutex_unlock(&lock_); }

 private:
  pthread_mutex_t lock_;
};

/**
 * @brief 自旋锁，适用于极短临界区（runq、fd 事件槽）。
 *
 * 临界区较长或可能阻塞时请用 Mutex。
 */
class Spinlock : public utils::NonCopyable {
 public:
  typedef ScopedLockImpl<Spinlock> Lock;

  Spinlock() { pthread_spin_init(&lock_, PTHREAD_PROCESS_PRIVATE); }
  ~Spinlock() { pthread_spin_destroy(&lock_); }

  void lock() { pthread_spin_lock(&lock_); }
  void unlock() { pthread_spin_unlock(&lock_); }

 private:
  pthread_spinlock_t lock_;
};

class RWMutex : public utils::NonCopyable {
 public:
  typedef ReadScopedLockImpl<RWMutex> ReadLock;
  typedef WriteScopedLockImpl<RWMutex> WriteLock;

  RWMutex() { pthread_rwlock_init(&lock_, nullptr); }
  ~RWMutex() { pthread_rwlock_destroy(&lock_); }

  void rdlock() { pthread_rwlock_rdlock(&lock_); }
  void wrlock() { pthread_rwlock_wrlock(&lock_); }
  void unlock() { pthread_rwlock_unlock(&lock_); }

 private:
  pthread_rwlock_t lock_;
};

}  // namespace thread
}  // namespace lemo
