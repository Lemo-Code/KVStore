#pragma once

#include <pthread.h>
#include <atomic>
#include <chrono>
#include <thread>

namespace zero {

// ============ SpinLock (自旋锁) ============
// 用于极短临界区 (几行代码)。用户态自旋，不进入内核。
class SpinLock {
public:
    SpinLock() : flag_(ATOMIC_FLAG_INIT) {}

    void lock() {
        // 指数退避: 先快速自旋，然后逐步让出 CPU
        int backoff = 0;
        while (flag_.test_and_set(std::memory_order_acquire)) {
            if (backoff < 64) {
                // 短自旋 (pause 指令降低功耗)
                #if defined(__x86_64__) || defined(__i386__)
                    __builtin_ia32_pause();
                #endif
                ++backoff;
            } else {
                // 长自旋 → 让出时间片
                std::this_thread::yield();
            }
        }
    }

    bool try_lock() {
        return !flag_.test_and_set(std::memory_order_acquire);
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag_;
};

// ============ Mutex (互斥锁) ============
// 基于 pthread_mutex，适合中等临界区
class Mutex {
public:
    Mutex() {
        pthread_mutex_init(&mutex_, nullptr);
    }
    ~Mutex() {
        pthread_mutex_destroy(&mutex_);
    }

    void lock()   { pthread_mutex_lock(&mutex_); }
    void unlock() { pthread_mutex_unlock(&mutex_); }
    bool try_lock() { return pthread_mutex_trylock(&mutex_) == 0; }

    pthread_mutex_t* native() { return &mutex_; }

    // 支持 std::lock_guard
    class Lock {
    public:
        explicit Lock(Mutex& m) : m_(m) { m_.lock(); }
        ~Lock() { m_.unlock(); }
    private:
        Mutex& m_;
    };

private:
    pthread_mutex_t mutex_;
};

// ============ RWMutex (读写锁) ============
// 基于 pthread_rwlock。读多写少场景 (配置读取)
class RWMutex {
public:
    RWMutex() {
        pthread_rwlock_init(&rwlock_, nullptr);
    }
    ~RWMutex() {
        pthread_rwlock_destroy(&rwlock_);
    }

    void read_lock()    { pthread_rwlock_rdlock(&rwlock_); }
    void write_lock()   { pthread_rwlock_wrlock(&rwlock_); }
    void read_unlock()  { pthread_rwlock_unlock(&rwlock_); }
    void write_unlock() { pthread_rwlock_unlock(&rwlock_); }

    class ReadLock {
    public:
        explicit ReadLock(RWMutex& m) : m_(m) { m_.read_lock(); }
        ~ReadLock() { m_.read_unlock(); }
    private:
        RWMutex& m_;
    };

    class WriteLock {
    public:
        explicit WriteLock(RWMutex& m) : m_(m) { m_.write_lock(); }
        ~WriteLock() { m_.write_unlock(); }
    private:
        RWMutex& m_;
    };

private:
    pthread_rwlock_t rwlock_;
};

} // namespace zero
