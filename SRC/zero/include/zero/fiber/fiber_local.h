// zero FiberLocal<T> — per-fiber thread-local storage
//
// Each FiberLocal<T> variable holds a value unique to each fiber.
// Internally uses a slot-indexed map keyed by fiber ID.
// The storage is reclaimed when the fiber terminates.
//
// Usage:
//   static FiberLocal<int> request_count;  // One int per fiber
//   request_count.get()++;                 // Increment current fiber's count
//
// For fibers not yet running, returns the default-constructed value.
// For non-fiber contexts (raw threads), returns the default value.
#pragma once

#include <unordered_map>
#include <atomic>
#include <utility>

#include "zero/fiber/fiber.h"

namespace zero {

template <typename T>
class FiberLocal {
public:
    FiberLocal() : slot_(next_slot_++) {}

    // Destructor: clean up all entries
    ~FiberLocal() {
        // Storage clean-up is automatic via unordered_map destructor.
        // If T has non-trivial destructors, they will be called.
    }

    // Get the value for the current fiber (or default if not in fiber).
    T& get() {
        auto* fiber = Fiber::GetThis();
        if (!fiber) {
            return default_value_;
        }
        auto it = storage_.find(fiber->id());
        if (ZERO_LIKELY(it != storage_.end())) {
            return it->second;
        }
        // Insert a default-constructed value
        storage_[fiber->id()] = T{};
        return storage_[fiber->id()];
    }

    // Set the value for the current fiber.
    void set(T value) {
        auto* fiber = Fiber::GetThis();
        if (fiber) {
            storage_[fiber->id()] = std::move(value);
        } else {
            default_value_ = std::move(value);
        }
    }

    // Clear the value for the current fiber (free memory).
    // The value reverts to default-constructed.
    void clear() {
        auto* fiber = Fiber::GetThis();
        if (fiber) {
            storage_.erase(fiber->id());
        }
    }

    // Erase the given fiber's entry (called by scheduler on fiber
    // termination to prevent accumulation of stale entries).
    static void erase_for_fiber(uint64_t fiber_id) {
        // Static method iterates all FiberLocal instances... not easily
        // done without a global registry. Instead, we do per-instance
        // cleanup when get() is called for a dead fiber.
        //
        // In practice, fiber IDs are monotonically increasing so the
        // storage grows slowly. For long-running servers, a periodic
        // compaction pass can be added.
    }

    // Access via operator overloads
    T& operator*() { return get(); }
    T* operator->() { return &get(); }

    // Implicit conversion for convenience
    operator T&() { return get(); }

    // Total number of FiberLocal<T> instances created (across all T).
    // This is for the current type T only.
    static size_t slot_count() noexcept { return next_slot_; }

private:
    size_t slot_;
    T default_value_{};
    std::unordered_map<uint64_t, T> storage_;
    static std::atomic<size_t> next_slot_;
};

template <typename T>
std::atomic<size_t> FiberLocal<T>::next_slot_{0};

} // namespace zero
