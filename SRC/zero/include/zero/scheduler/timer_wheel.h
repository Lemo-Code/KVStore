// zero TimerWheel — 5-level hierarchical timer wheel
//
// Provides O(1) tick, O(1) add, O(1) cancel amortized timer management.
// Uses hierarchical timing wheels where each level has coarser granularity:
//
//   Level 0: 256 slots @ 1ms      = 0 - 255ms range
//   Level 1: 64 slots  @ 256ms    = 256ms - 16.384s
//   Level 2: 64 slots  @ 16384ms  = 16.384s - 1048.576s (~17.5 min)
//   Level 3: 64 slots  @ 1048.576s = ~17.5 min - ~1.16 hours
//   Level 4: 64 slots  @ 67108.864s = ~1.16 hours - ~49.7 days
//
// Total range: ~49.7 days with 1ms granularity.
//
// Timers cascade from higher levels to lower levels when time advances.
// When a level's cursor wraps around, timers in the next slot are
// "cascaded" (re-inserted into finer-granularity levels).
//
// Thread-safe for concurrent add/cancel from different fibers (internally
// uses SpinLock). However, tick() must be called from a single thread
// (the reactor thread).
#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <array>

#include "zero/base/noncopyable.h"
#include "zero/thread/spinlock.h"

namespace zero {

class TimerWheel : public Noncopyable {
public:
    using TimerCallback = std::function<void()>;

    static constexpr int kLevels = 5;

    // ============================================================
    // Timer entry
    // ============================================================

    struct Timer {
        uint64_t id;
        uint64_t deadline_ms;   // Absolute deadline in milliseconds
        TimerCallback callback;
        int level = 0;          // Current wheel level (0-4)
        int slot = 0;           // Current slot within the level
        Timer* next = nullptr;  // Next timer in the same slot (linked list)
        bool cancelled = false; // Marked for removal
    };

    // ============================================================
    // Wheel configuration
    // ============================================================

    // Number of slots per level
    static constexpr int kSlotsPerLevel[kLevels] = {256, 64, 64, 64, 64};
    // Resolution per slot in ms
    static constexpr int kResolutionMs[kLevels] = {
        1, 256, 16384, 1048576, 67108864
    };
    // Total range per level in ms (slots * resolution)
    static constexpr uint64_t kRangeMs[kLevels] = {
        256, 16384, 1048576, 67108864, 4294967296ULL  // ~49.7 days
    };

    // ============================================================
    // Construction
    // ============================================================

    TimerWheel();
    ~TimerWheel();

    // ============================================================
    // Operations
    // ============================================================

    // Add a timer that will fire after `delay_ms` milliseconds.
    // Returns a unique timer ID (non-zero), or 0 on failure.
    // The callback will be invoked from the reactor thread on expiry.
    uint64_t add_timer(uint64_t delay_ms, TimerCallback cb);

    // Cancel a timer by its ID.
    // Returns true if the timer was found and cancelled, false if
    // it already fired or was already cancelled.
    bool cancel_timer(uint64_t timer_id);

    // Advance time by one tick (1ms).
    // Called by the reactor from the event loop.
    // Fires all expired timers and returns the count of fired timers.
    int tick();

    // ============================================================
    // Observers
    // ============================================================

    // Get the next epoll timeout in milliseconds.
    // Always returns 1 (we tick at 1ms granularity).
    // In optimized implementations, this could return the time
    // to the next timer expiry.
    int next_timeout_ms() const noexcept { return 1; }

    // Current time in milliseconds since wheel creation.
    uint64_t now_ms() const noexcept {
        return current_time_ms_.load(std::memory_order_relaxed);
    }

    // Total timers currently registered (approximate)
    size_t active_timers() const noexcept;

    // Total timers ever fired
    uint64_t total_fired() const noexcept {
        return total_fired_.load(std::memory_order_relaxed);
    }

    // Total timers ever cancelled
    uint64_t total_cancelled() const noexcept {
        return total_cancelled_.load(std::memory_order_relaxed);
    }

private:
    // Insert a timer into the appropriate level + slot
    void insert_timer(Timer* timer);

    // Cascade timers from a level to lower levels when the level's
    // cursor wraps around. All timers in the next slot are removed
    // and re-inserted into finer-granularity levels.
    void cascade(int level);

    // Remove cancelled timers from a slot's linked list
    void clean_cancelled(int level, int slot);

    // Allocate a Timer from the pool
    Timer* alloc_timer();

    // Return a timer to the pool
    void free_timer(Timer* timer);

    // Linked-list heads: wheels_[level][slot] = head of linked list
    // Using fixed-size 256 for max slot count across all levels
    Timer* wheels_[kLevels][256] = {};

    // Current position (slot index) within each level
    int cursors_[kLevels] = {};

    std::atomic<uint64_t> current_time_ms_{0};

    // Timer storage: pre-allocated pool to avoid per-timer allocation
    std::vector<Timer*> timer_pool_;

    // Spinlock for add_timer/cancel_timer (tick is single-threaded)
    mutable SpinLock lock_;

    uint64_t next_timer_id_ = 1;

    // Statistics
    std::atomic<uint64_t> total_fired_{0};
    std::atomic<uint64_t> total_cancelled_{0};
};

} // namespace zero
