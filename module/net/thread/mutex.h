#ifndef NET_THREAD_MUTEX_H
#define NET_THREAD_MUTEX_H

#include "noncopyable.h"

#include <atomic>
#include <pthread.h>

namespace net {

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

class Mutex : Noncopyable {
 public:
  typedef ScopedLockImpl<Mutex> Lock;

  Mutex() { pthread_mutex_init(&lock_, nullptr); }
  ~Mutex() { pthread_mutex_destroy(&lock_); }

  void lock() { pthread_mutex_lock(&lock_); }
  void unlock() { pthread_mutex_unlock(&lock_); }

 private:
  pthread_mutex_t lock_;
};

class RWMutex : Noncopyable {
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

class NullRWMutex : Noncopyable {
 public:
  typedef ReadScopedLockImpl<NullRWMutex> ReadLock;
  typedef WriteScopedLockImpl<NullRWMutex> WriteLock;

  void rdlock() {}
  void wrlock() {}
  void unlock() {}
};

class NullMutex : Noncopyable {
 public:
  typedef ScopedLockImpl<NullMutex> Lock;

  void lock() {}
  void unlock() {}
};

class Spinlock : Noncopyable {
 public:
  typedef ScopedLockImpl<Spinlock> Lock;

  Spinlock() { pthread_spin_init(&mutex_, 0); }
  ~Spinlock() { pthread_spin_destroy(&mutex_); }

  void lock() { pthread_spin_lock(&mutex_); }
  void unlock() { pthread_spin_unlock(&mutex_); }

 private:
  pthread_spinlock_t mutex_;
};

class CASLock : Noncopyable {
 public:
  typedef ScopedLockImpl<CASLock> Lock;

  CASLock() { mutex_.clear(); }

  void lock() {
    while (std::atomic_flag_test_and_set_explicit(&mutex_,
                                                  std::memory_order_acquire)) {
    }
  }

  void unlock() {
    std::atomic_flag_clear_explicit(&mutex_, std::memory_order_release);
  }

 private:
  std::atomic_flag mutex_ = ATOMIC_FLAG_INIT;
};

}  // namespace net

#endif  // NET_THREAD_MUTEX_H
