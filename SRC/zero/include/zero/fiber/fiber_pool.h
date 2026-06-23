// zero FiberPool — pre-allocate and reuse Fiber objects
//
// Creating and destroying Fiber objects requires stack allocation
// (mmap/munmap) which is expensive. FiberPool maintains a free list
// of recycled fibers so that hot-path fiber creation is O(1) with
// no syscall overhead.
//
// Thread-safe for concurrent access across scheduler threads.
// Usage:
//   auto fiber = FiberPool::instance().get(callback);
//   scheduler->schedule(fiber);
//   // After fiber terminates, FiberPool::recycle() is called automatically.
#pragma once

#include <vector>
#include <cstddef>

#include "zero/fiber/fiber.h"
#include "zero/thread/spinlock.h"

namespace zero {

class FiberPool {
public:
    // Singleton accessor
    static FiberPool& instance();

    // Get a fiber from the pool, or create a new one if the pool is empty.
    // The callback and stack size are forwarded to Fiber constructor.
    // Thread-safe.
    Fiber::Ptr get(Fiber::Callback cb, size_t stack_size = Fiber::kDefaultStackSize);

    // Return a finished fiber to the pool for reuse.
    // The fiber's callback is cleared and its state is reset to INIT.
    // Thread-safe.
    void recycle(Fiber::Ptr fiber);

    // Pre-allocate a number of fibers to warm up the pool.
    // Useful during server startup to avoid allocation in the hot path.
    void preallocate(size_t count, size_t stack_size = Fiber::kDefaultStackSize);

    // Reserve capacity in the pool (does not allocate fibers, just
    // pre-allocates vector storage).
    void reserve(size_t count);

    // Statistics
    size_t available() const noexcept;
    size_t total_allocated() const noexcept;
    size_t total_recycled() const noexcept;

    // Clear all cached fibers (use before shutdown to free memory)
    void clear();

private:
    FiberPool() = default;
    ~FiberPool() = default;

    mutable SpinLock lock_;
    std::vector<Fiber::Ptr> pool_;
    size_t total_allocated_ = 0;
    size_t total_recycled_ = 0;
};

} // namespace zero
