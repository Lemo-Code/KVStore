// zero Channel<T> — fiber-aware CSP-style channel
//
// Channels are the communication primitive for fibers — they provide
// safe, synchronized data transfer between fibers without blocking
// OS threads. When a fiber blocks on send() or recv(), it yields
// execution to the scheduler rather than blocking the thread.
//
// Supports:
//   - Buffered channels (capacity > 0): buffer up to N items
//   - Unbuffered channels (capacity == 0): synchronous rendezvous
//   - trySend/tryRecv: non-blocking operations
//   - close(): graceful shutdown, wakes all blocked fibers
namespace zero { class Scheduler; }
//
// Thread-safe for send/recv from different fibers (internally uses SpinLock).
// Not safe for concurrent send/recv from different OS threads without
// additional synchronization.
//
// Usage:
//   auto ch = std::make_shared<Channel<int>>(10);  // Buffered, capacity 10
//   ch->send(42);       // Fiber yields if full
//   int val;
//   ch->recv(val);      // Fiber yields if empty
#pragma once

#include <queue>
#include <memory>
#include <utility>

#include "zero/fiber/fiber.h"
#include "zero/scheduler/scheduler.h"
#include "zero/thread/spinlock.h"

namespace zero {

class Scheduler;

template <typename T>
class Channel {
public:
    using Ptr = std::shared_ptr<Channel<T>>;

    // Create a channel with optional buffer capacity.
    // capacity=0 means synchronous (rendezvous): send blocks until recv.
    // capacity=N means buffered: up to N values can be buffered.
    explicit Channel(size_t capacity = 0)
        : capacity_(capacity) {}

    ~Channel() {
        // Wake any blocked fibers to prevent leaks
        close();
    }

    // ============================================================
    // Send operations
    // ============================================================

    // Send a value into the channel (copy).
    // Blocks the calling fiber if the buffer is full (unbuffered: blocks
    // until a receiver is ready). Returns false if the channel is closed.
    bool send(const T& value) {
        return sendImpl(T(value));
    }

    // Send a value into the channel (move).
    bool send(T&& value) {
        return sendImpl(std::move(value));
    }

    // Try to send without blocking.
    // Returns true if the value was accepted.
    bool try_send(const T& value) {
        return trySendImpl(value);
    }

    // ============================================================
    // Receive operations
    // ============================================================

    // Receive a value from the channel.
    // Blocks the calling fiber if the buffer is empty.
    // Returns false if the channel is closed and empty.
    bool recv(T& out) {
        return recvImpl(out);
    }

    // Try to receive without blocking.
    // Returns true if a value was received.
    bool try_recv(T& out) {
        return tryRecvImpl(out);
    }

    // ============================================================
    // Channel lifecycle
    // ============================================================

    // Close the channel. All blocked fibers are woken and will return
    // false from their send/recv operations.
    // Subsequent sends/recvs return false immediately.
    void close() {
        spinlock_.lock();
        if (closed_) {
            spinlock_.unlock();
            return;
        }
        closed_ = true;

        // Drain blocked fibers
        auto recvers = std::move(recv_waiters_);
        auto senders = std::move(send_waiters_);
        spinlock_.unlock();

        // Schedule all blocked fibers for wakeup
        schedule_all(std::move(recvers));
        schedule_all(std::move(senders));
    }

    // Whether the channel has been closed
    bool is_closed() const noexcept {
        // Relaxed: close() is a one-way transition; once true, never
        // goes back to false.
        return closed_;
    }

    // ============================================================
    // Capacity / Size
    // ============================================================

    // Number of items currently in the channel
    size_t size() const {
        LockGuard lg(spinlock_);
        return buffer_.size();
    }

    // Maximum number of items the channel can hold
    size_t capacity() const noexcept { return capacity_; }

    // Whether the channel is empty
    bool empty() const {
        LockGuard lg(spinlock_);
        return buffer_.empty();
    }

    // Whether the channel is full (buffered only; unbuffered is always
    // "full" because it has 0 capacity)
    bool full() const {
        LockGuard lg(spinlock_);
        return capacity_ > 0 && buffer_.size() >= capacity_;
    }

private:
    template <typename U>
    bool sendImpl(U&& value) {
        Fiber* self = Fiber::GetThis();
        if (!self) return false;  // Not in a fiber context

        while (true) {
            spinlock_.lock();

            if (ZERO_UNLIKELY(closed_)) {
                spinlock_.unlock();
                return false;
            }

            // Fast path: wake a waiting receiver directly
            if (!recv_waiters_.empty()) {
                auto recver = std::move(recv_waiters_.front());
                recv_waiters_.pop();
                direct_value_ = std::forward<U>(value);
                has_direct_ = true;
                spinlock_.unlock();

                schedule_one(std::move(recver));
                return true;
            }

            // Buffered path: enqueue if space
            if (capacity_ > 0 && buffer_.size() < capacity_) {
                buffer_.push(std::forward<U>(value));
                spinlock_.unlock();
                return true;
            }

            // No space — block this fiber
            send_waiters_.push(self->shared_from_this());
            spinlock_.unlock();

            self->yield();
            // After being woken, loop and retry
        }
    }

    bool recvImpl(T& out) {
        Fiber* self = Fiber::GetThis();
        if (!self) return false;

        while (true) {
            spinlock_.lock();

            // Fast path: direct value from a sender
            if (has_direct_) {
                out = std::move(direct_value_);
                has_direct_ = false;
                spinlock_.unlock();
                return true;
            }

            // Buffered path: dequeue
            if (!buffer_.empty()) {
                out = std::move(buffer_.front());
                buffer_.pop();

                // Wake a blocked sender if there is one
                if (!send_waiters_.empty()) {
                    auto snd = std::move(send_waiters_.front());
                    send_waiters_.pop();
                    spinlock_.unlock();
                    schedule_one(std::move(snd));
                } else {
                    spinlock_.unlock();
                }
                return true;
            }

            // Closed and empty -> done
            if (ZERO_UNLIKELY(closed_)) {
                spinlock_.unlock();
                return false;
            }

            // Nothing available — block
            recv_waiters_.push(self->shared_from_this());
            spinlock_.unlock();

            self->yield();
        }
    }

    bool trySendImpl(const T& value) {
        spinlock_.lock();

        if (closed_) {
            spinlock_.unlock();
            return false;
        }

        // Wake a receiver if one is waiting
        if (!recv_waiters_.empty()) {
            auto recver = std::move(recv_waiters_.front());
            recv_waiters_.pop();
            direct_value_ = value;
            has_direct_ = true;
            spinlock_.unlock();
            schedule_one(std::move(recver));
            return true;
        }

        // Buffer if there's space
        if (capacity_ > 0 && buffer_.size() < capacity_) {
            buffer_.push(value);
            spinlock_.unlock();
            return true;
        }

        spinlock_.unlock();
        return false;
    }

    bool tryRecvImpl(T& out) {
        spinlock_.lock();

        if (has_direct_) {
            out = std::move(direct_value_);
            has_direct_ = false;
            spinlock_.unlock();
            return true;
        }

        if (!buffer_.empty()) {
            out = std::move(buffer_.front());
            buffer_.pop();

            // Wake a blocked sender if there is one
            if (!send_waiters_.empty()) {
                auto snd = std::move(send_waiters_.front());
                send_waiters_.pop();
                spinlock_.unlock();
                schedule_one(std::move(snd));
            } else {
                spinlock_.unlock();
            }
            return true;
        }

        spinlock_.unlock();
        return false;
    }

    void schedule_one(Fiber::Ptr fiber) {
        if (!fiber) return;
        fiber->setState(Fiber::State::READY);
        auto* sched = Scheduler::GetThis();
        if (sched) sched->schedule(std::move(fiber));
    }

    void schedule_all(std::queue<Fiber::Ptr> waiters) {
        while (!waiters.empty()) {
            schedule_one(std::move(waiters.front()));
            waiters.pop();
        }
    }

    // ---- Member variables ----
    mutable SpinLock spinlock_;
    bool closed_ = false;
    bool has_direct_ = false;
    T direct_value_{};
    size_t capacity_;
    std::queue<T> buffer_;
    std::queue<Fiber::Ptr> recv_waiters_;
    std::queue<Fiber::Ptr> send_waiters_;
};

} // namespace zero
