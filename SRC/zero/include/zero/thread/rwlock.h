// zero RWLock — read-write lock with RAII read/write guards
// Wraps pthread_rwlock_t for multiple-reader / single-writer semantics.
// Provides ReadLockGuard (shared access) and WriteLockGuard (exclusive
// access).
#pragma once

#include <pthread.h>
#include <utility>

#include "zero/base/noncopyable.h"

namespace zero {

class RWLock : public Noncopyable {
public:
    RWLock() {
        pthread_rwlockattr_t attr;
        pthread_rwlockattr_init(&attr);
        // Prefer writer by default (avoids write starvation)
        pthread_rwlockattr_setkind_np(
            &attr,
            PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
        pthread_rwlock_init(&rwlock_, &attr);
        pthread_rwlockattr_destroy(&attr);
    }

    ~RWLock() {
        pthread_rwlock_destroy(&rwlock_);
    }

    // Acquire shared (read) lock
    void lock_shared() {
        pthread_rwlock_rdlock(&rwlock_);
    }

    // Release shared (read) lock
    void unlock_shared() {
        pthread_rwlock_unlock(&rwlock_);
    }

    // Acquire exclusive (write) lock
    void lock() {
        pthread_rwlock_wrlock(&rwlock_);
    }

    // Release exclusive (write) lock
    void unlock() {
        pthread_rwlock_unlock(&rwlock_);
    }

    // Try to acquire shared (read) lock without blocking
    bool try_lock_shared() {
        return pthread_rwlock_tryrdlock(&rwlock_) == 0;
    }

    // Try to acquire exclusive (write) lock without blocking
    bool try_lock() {
        return pthread_rwlock_trywrlock(&rwlock_) == 0;
    }

    // Get the native handle
    pthread_rwlock_t* native_handle() noexcept { return &rwlock_; }
    const pthread_rwlock_t* native_handle() const noexcept { return &rwlock_; }

private:
    pthread_rwlock_t rwlock_;
};

// RAII guard for shared (read) ownership
class ReadLockGuard {
public:
    explicit ReadLockGuard(RWLock& lock) : lock_(&lock) {
        lock_->lock_shared();
    }

    ~ReadLockGuard() {
        lock_->unlock_shared();
    }

    ReadLockGuard(const ReadLockGuard&) = delete;
    ReadLockGuard& operator=(const ReadLockGuard&) = delete;

private:
    RWLock* lock_;
};

// RAII guard for exclusive (write) ownership
class WriteLockGuard {
public:
    explicit WriteLockGuard(RWLock& lock) : lock_(&lock) {
        lock_->lock();
    }

    ~WriteLockGuard() {
        lock_->unlock();
    }

    WriteLockGuard(const WriteLockGuard&) = delete;
    WriteLockGuard& operator=(const WriteLockGuard&) = delete;

private:
    RWLock* lock_;
};

// Upgradeable read lock — can be atomically upgraded to write lock.
// Not natively supported by POSIX, so this is a simple wrapper that
// unlocks the read lock, acquires the write lock, then downgrades back.
class UpgradeLockGuard {
public:
    explicit UpgradeLockGuard(RWLock& lock)
        : lock_(&lock), write_locked_(false) {
        lock_->lock_shared();
    }

    ~UpgradeLockGuard() {
        if (write_locked_) {
            lock_->unlock();
        } else {
            lock_->unlock_shared();
        }
    }

    // Upgrade from read to write. Warning: other readers may have
    // released/acquired in between.
    void upgrade() {
        if (!write_locked_) {
            lock_->unlock_shared();
            lock_->lock();
            write_locked_ = true;
        }
    }

    // Downgrade from write back to read.
    void downgrade() {
        if (write_locked_) {
            lock_->unlock();
            lock_->lock_shared();
            write_locked_ = false;
        }
    }

    bool is_write_locked() const noexcept { return write_locked_; }

    UpgradeLockGuard(const UpgradeLockGuard&) = delete;
    UpgradeLockGuard& operator=(const UpgradeLockGuard&) = delete;

private:
    RWLock* lock_;
    bool write_locked_;
};

} // namespace zero
