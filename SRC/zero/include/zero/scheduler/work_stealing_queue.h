// zero WorkStealingQueue — Chase-Lev lock-free MPMC bounded deque
//
// Implements the Chase-Lev work-stealing algorithm:
//   - Owner thread pushes/pops from the bottom (LIFO, cache-friendly)
//   - Thief threads steal from the top (FIFO, minimizes contention)
//
// This is the core data structure for scheduler load balancing.
// Each worker thread owns one queue. When a worker's local queue is
// empty, it randomly steals work from another worker's queue.
//
// Key properties:
//   - push() and pop() are lock-free (owner-only, no CAS needed for
//     the common case)
//   - steal() uses CAS on top, minimal contention with owner
//   - Bottom and top are on separate cache lines to prevent false sharing
//   - Bounded capacity (power of 2 for efficient masking)
//
// Memory ordering:
//   - push: release on buffer write, release on bottom increment
//   - pop:  relaxed on bottom decrement, acquire on top read
//   - steal: acquire on top read, acquire on buffer read
#pragma once

#include <atomic>
#include <memory>
#include <cstddef>
#include <cassert>

#include "zero/fiber/fiber.h"

namespace zero {

class WorkStealingQueue {
public:
    // Must be power of 2 for efficient masking
    explicit WorkStealingQueue(size_t capacity);
    ~WorkStealingQueue();

    WorkStealingQueue(const WorkStealingQueue&) = delete;
    WorkStealingQueue& operator=(const WorkStealingQueue&) = delete;

    // ============================================================
    // Owner-only operations (no lock needed)
    // ============================================================

    // Push a fiber onto the bottom of the deque (LIFO for owner).
    // The owner thread is the only one that calls push/pop.
    void push(Fiber::Ptr fiber);

    // Pop a fiber from the bottom of the deque (LIFO for owner).
    // Returns nullptr if the deque is empty.
    Fiber::Ptr pop();

    // ============================================================
    // Thief operations (lock-free CAS)
    // ============================================================

    // Steal a fiber from the top of the deque (FIFO for thieves).
    // Can be called by any thread.
    // Returns nullptr if the deque is empty or contention fails.
    Fiber::Ptr steal();

    // ============================================================
    // Observers
    // ============================================================

    // Approximate number of fibers in the queue (racy but safe)
    size_t size() const noexcept;

    // Whether the queue is empty (racy but safe)
    bool empty() const noexcept;

    // Maximum capacity
    size_t capacity() const noexcept { return capacity_; }

private:
    static constexpr size_t kCacheLineSize = 64;

    // bottom_: owner writes to tracks the bottom index.
    // top_: owner reads, thieves CAS to track the top index.
    // Separated by cache lines to prevent false sharing between
    // owner (bottom) and thieves (top).
    alignas(kCacheLineSize) std::atomic<size_t> bottom_{0};
    alignas(kCacheLineSize) std::atomic<size_t> top_{0};

    size_t capacity_;
    size_t mask_;           // capacity_ - 1 (power of 2)
    Fiber::Ptr* buffer_;    // Circular buffer of fibers
};

// Inline implementations of size/empty for hot-path access
inline size_t WorkStealingQueue::size() const noexcept {
    int64_t b = static_cast<int64_t>(
        bottom_.load(std::memory_order_relaxed));
    int64_t t = static_cast<int64_t>(
        top_.load(std::memory_order_relaxed));
    int64_t sz = b - t;
    return sz > 0 ? static_cast<size_t>(sz) : 0;
}

inline bool WorkStealingQueue::empty() const noexcept {
    return bottom_.load(std::memory_order_relaxed) <=
           top_.load(std::memory_order_relaxed);
}

} // namespace zero
