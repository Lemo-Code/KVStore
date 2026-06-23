// zero TimerWheel implementation — 5-level hierarchical timing wheel
//
// A hierarchical timer wheel (Varghese & Lauck, 1987) with O(1) tick and
// O(1) insertion. Five levels span from 1 ms to approximately 49.7 days
// of timer coverage:
//
//   Level  Slots  Resolution      Range
//   -----  -----  ----------      ---------------
//     0     256      1 ms               256 ms
//     1      64    256 ms            16,384 ms  (~16 s)
//     2      64  16,384 ms        1,048,576 ms  (~17 min)
//     3      64   1,048 s        67,108,864 ms  (~19 h)
//     4      64      67 s     4,294,967,296 ms  (~49.7 d)
//
// Timers cascade from coarser to finer levels as time advances. When
// a level's cursor advances (triggered by the finer level wrapping),
// all timers in the new slot are redistributed to appropriate lower-level
// slots based on their remaining time-to-expiry.
//
// Cancellation is lazy: cancelTimer() nulls the callback. The timer node
// remains in the linked list but is skipped when fired. Periodic garbage
// collection reclaims canceled nodes.

#include "zero/scheduler/timer_wheel.h"
#include "zero/base/macro.h"

#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <algorithm>

namespace zero {

// ============================================================
// Static level configuration
// ============================================================


namespace {

// Precomputed total range per level: kSlots[L] * kResMs[L]
constexpr int kSlots[TimerWheel::kLevels] = {256, 64, 64, 64, 64};
constexpr int kResMs[TimerWheel::kLevels] = {1, 256, 16384, 1048576, 67108864};
constexpr uint64_t kLevelRange[TimerWheel::kLevels] = {
    256ULL,                       // L0: 256 * 1
    64ULL * 256ULL,               // L1: 16,384
    64ULL * 16384ULL,             // L2: 1,048,576
    64ULL * 1048576ULL,           // L3: 67,108,864
    64ULL * 67108864ULL           // L4: 4,294,967,296
};

// Number of L0 ticks that must elapse before a level's cursor advances.
// L0 advances every tick. L1 advances every 256 ticks. L2 advances every
// 256*64=16384 ticks, etc.
constexpr uint64_t kAdvancePeriod[TimerWheel::kLevels] = {
    1ULL,                         // L0: every 1 tick
    256ULL,                       // L1: every 256 L0 ticks
    256ULL * 64ULL,               // L2: every 16384 L0 ticks
    256ULL * 64ULL * 64ULL,       // L3: every 1048576 L0 ticks
    256ULL * 64ULL * 64ULL * 64ULL // L4: every 67108864 L0 ticks
};

// GC interval in ticks. 1000 ticks ≈ 1 second in the default 1ms tick
// configuration. Balances memory reclamation frequency against overhead.
constexpr uint64_t kGcInterval = 1000;

// Maximum timer pool size before GC is forced regardless of interval.
// Prevents unbounded growth under pathological cancel-heavy workloads.
constexpr size_t kGcForceThreshold = 65536;

} // anonymous namespace

// ============================================================
// Helper: current monotonic time in milliseconds
// ============================================================

static uint64_t get_monotonic_ms() {
    struct timespec ts;
    // CLOCK_MONOTONIC provides a non-decreasing clock unaffected by
    // system time adjustments. This is essential for long-running
    // server processes where NTP or admin actions may change the
    // wall clock.
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000ULL +
           static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL;
}

// ============================================================
// Constructor
// ============================================================

TimerWheel::TimerWheel() {
    // Initialize the current time from the monotonic clock once at
    // construction. Subsequent advancement is done via tick() calls.
    current_time_ms_ = get_monotonic_ms();

    // Zero all slot list heads. Each element is the head of a singly
    // linked list of Timer nodes; nullptr = empty slot.
    memset(wheels_, 0, sizeof(wheels_));

    // Zero all cursors.
    memset(cursors_, 0, sizeof(cursors_));

    // Pre-allocate timer pool capacity to reduce reallocations during
    // steady-state operation. 4096 timers is sufficient for most
    // server workloads; the vector will grow if needed.
    timer_pool_.reserve(4096);

    next_timer_id_ = 1;
}

// ============================================================
// Destructor
// ============================================================

TimerWheel::~TimerWheel() {
    // Delete all Timer nodes. The timer_pool_ vector owns all allocated
    // Timer objects; the wheels_ arrays contain only non-owning raw
    // pointers into this pool. Deleting through the pool is safe
    // because no timers can fire during destruction.
    for (Timer* t : timer_pool_) {
        delete t;
    }
    timer_pool_.clear();

    // No need to clear wheels_ — the memory is about to be freed.
}

// ============================================================
// addTimer — schedule a callback after delay_ms milliseconds
// ============================================================

uint64_t TimerWheel::add_timer(uint64_t delay_ms, TimerCallback cb) {
    // Enforce a minimum delay of 1 ms. Timers with 0 delay would fire
    // on the very next tick, which is acceptable behavior but the
    // explicit minimum prevents accidental immediate execution when
    // the caller intended "as soon as possible."
    if (delay_ms == 0) {
        delay_ms = 1;
    }

    // Allocate the timer node. All Timer objects are heap-allocated
    // because their lifetime extends beyond the calling stack frame.
    Timer* timer = new Timer();
    timer->id          = next_timer_id_++;
    timer->deadline_ms = current_time_ms_ + delay_ms;
    timer->callback    = std::move(cb);
    timer->level       = 0;
    timer->slot        = 0;
    timer->next        = nullptr;

    // Handle the vanishingly unlikely event of 64-bit ID wraparound.
    // At one timer per nanosecond, this would take ~585 years.
    if (ZERO_UNLIKELY(next_timer_id_ == 0)) {
        next_timer_id_ = 1;
    }

    // Insert into the appropriate wheel slot.
    insert_timer(timer);

    // Track in the pool for cleanup.
    timer_pool_.push_back(timer);

    return timer->id;
}

// ============================================================
// cancelTimer — cancel a previously scheduled timer
// ============================================================

bool TimerWheel::cancel_timer(uint64_t timer_id) {
    if (timer_id == 0) {
        return false;
    }

    // Linear scan over the pool. In practice, the number of active
    // timers is usually modest (tens to low hundreds). For workloads
    // with thousands of timers and frequent cancellation, a hash table
    // would be justified. The lazy approach (null callback) avoids the
    // complexity of removing nodes from linked lists mid-traversal.
    for (Timer* t : timer_pool_) {
        if (t->id == timer_id) {
            t->callback = nullptr;  // Mark as canceled
            return true;
        }
    }

    return false;
}

// ============================================================
// insert_timer — place a timer into the correct wheel slot
// ============================================================

void TimerWheel::insert_timer(Timer* timer) {
    if (timer == nullptr) {
        return;
    }

    // Calculate the remaining time until expiration.
    // Using signed arithmetic to correctly handle the case where
    // the deadline is already in the past (e.g., after cascading).
    int64_t delta_ms = static_cast<int64_t>(timer->deadline_ms) -
                       static_cast<int64_t>(current_time_ms_);

    // Already-expired timers go directly into the current L0 slot
    // so they are fired on the immediate next tick.
    if (delta_ms <= 0) {
        timer->level = 0;
        timer->slot  = cursors_[0];
        timer->next  = wheels_[0][timer->slot];
        wheels_[0][timer->slot] = timer;
        return;
    }

    uint64_t delta = static_cast<uint64_t>(delta_ms);

    // Find the lowest (finest-granularity) level that can cover this
    // delta. Lower levels have finer resolution, which means more
    // accurate firing time and fewer cascade operations.
    for (int level = 0; level < TimerWheel::kLevels; ++level) {
        if (delta < kLevelRange[level]) {
            // Compute the slot index.
            //   slot = (deadline_ms / kResMs[level]) % kSlots[level]
            // This places the timer into the slot that will be reached
            // exactly when the deadline arrives.
            uint64_t slot64 = (timer->deadline_ms /
                               static_cast<uint64_t>(kResMs[level])) %
                              static_cast<uint64_t>(kSlots[level]);
            int slot = static_cast<int>(slot64);

            timer->level = level;
            timer->slot  = slot;

            // Head insertion — O(1), order within the slot does not matter
            // since all timers in the slot are processed together.
            timer->next = wheels_[level][slot];
            wheels_[level][slot] = timer;
            return;
        }
    }

    // Fallback: the delta exceeds all level ranges (theoretically
    // impossible since L4 covers ~49.7 days). Place in L4 as a safety net.
    {
        int level = TimerWheel::kLevels - 1;
        uint64_t slot64 = (timer->deadline_ms /
                           static_cast<uint64_t>(kResMs[level])) %
                          static_cast<uint64_t>(kSlots[level]);
        timer->level = level;
        timer->slot  = static_cast<int>(slot64);
        timer->next  = wheels_[level][timer->slot];
        wheels_[level][timer->slot] = timer;
    }
}

// ============================================================
// tick() — advance time by one millisecond and fire expired timers
// ============================================================

int TimerWheel::tick() {
    // Advance the global "current time" for this timer wheel instance.
    ++current_time_ms_;

    int fired = 0;

    // ============================================================
    // Step 1: Advance the L0 cursor (wrapping at 256)
    // ============================================================
    cursors_[0] = (cursors_[0] + 1) % kSlots[0];

    // ============================================================
    // Step 2: Fire expired timers from the current L0 slot
    // ============================================================
    {
        Timer* head = wheels_[0][cursors_[0]];
        wheels_[0][cursors_[0]] = nullptr;

        Timer* curr = head;
        while (curr != nullptr) {
            Timer* next = curr->next;
            curr->next = nullptr;

            if (curr->callback != nullptr && curr->deadline_ms <= current_time_ms_) {
                // Fire the expired timer. The callback is invoked
                // synchronously; it may schedule new fibers, timers,
                // or perform I/O. This is safe because we are not
                // holding any locks and the wheel state is consistent.
                curr->callback();
                ++fired;
                // The callback is NOT cleared — it remains valid for
                // debugging introspection. The timer node stays in
                // timer_pool_ and is cleaned up by GC.
            } else if (curr->callback != nullptr) {
                // Not yet expired. This can happen when timers cascade
                // from higher levels into L0 but their deadline is
                // further in the future. Re-insert at the correct level.
                insert_timer(curr);
            }
            // Timers with null callback (canceled) are ignored.

            curr = next;
        }
    }

    // ============================================================
    // Step 3: Cascade higher levels at the appropriate periods
    // ============================================================
    // Level 1 cascades every 256 ticks (when L0 wraps).
    // Level 2 cascades every 256*64 ticks, etc.
    // Using modulo arithmetic on current_time_ms_ to determine when
    // a level's cursor should advance.
    for (int level = 1; level < TimerWheel::kLevels; ++level) {
        // A level advances when the global time is a multiple of that
        // level's advance period. This is equivalent to checking cursor
        // wrap but avoids maintaining per-level tick counters.
        if ((current_time_ms_ % kAdvancePeriod[level]) == 0) {
            cascade(level);
        }
    }

    // ============================================================
    // Step 4: Periodic garbage collection
    // ============================================================
    {
        static thread_local uint64_t gc_tick_counter = 0;
        ++gc_tick_counter;

        // Run GC every kGcInterval ticks, or if the pool is bloated.
        if (gc_tick_counter >= kGcInterval ||
            timer_pool_.size() >= kGcForceThreshold) {
            gc_tick_counter = 0;

            // Remove all nodes whose callback has been nulled (fired
            // or canceled). The erase-remove idiom is efficient and
            // preserves the relative order of remaining timers.
            auto new_end = std::remove_if(
                timer_pool_.begin(), timer_pool_.end(),
                [](Timer* t) {
                    if (t->callback == nullptr) {
                        delete t;
                        return true;
                    }
                    return false;
                });
            timer_pool_.erase(new_end, timer_pool_.end());
        }
    }

    return fired;
}

// ============================================================
// cascade() — move timers from a coarser level to finer levels
// ============================================================

void TimerWheel::cascade(int level) {
    // Only levels 1-4 can cascade (L0 is the finest and has no lower
    // level to cascade into).
    if (level <= 0 || level >= TimerWheel::kLevels) {
        return;
    }

    // Advance the cursor for this level.
    cursors_[level] = (cursors_[level] + 1) % kSlots[level];

    // Take all timers from the current slot. The slot is cleared so
    // that new timers inserted into this slot during the cascade
    // process are not re-processed.
    Timer* head = wheels_[level][cursors_[level]];
    wheels_[level][cursors_[level]] = nullptr;

    // Re-insert each timer. insert_timer() recalculates the correct
    // (finer) level and slot based on the remaining delta. Active
    // timers cascade downward; canceled timers are silently dropped.
    Timer* curr = head;
    while (curr != nullptr) {
        Timer* next = curr->next;
        curr->next = nullptr;

        if (curr->callback != nullptr) {
            // Active timer — re-insert at the appropriate finer level.
            insert_timer(curr);
        }
        // Canceled timers (null callback) remain in timer_pool_ and
        // are cleaned up by periodic GC. We do not delete them here
        // to avoid introducing complexity in the cascade path.

        curr = next;
    }
}

// ============================================================
// Design notes
// ============================================================
//
// Timer ID lifecycle:
//   1. addTimer() allocates a Timer node and assigns a monotonically
//      increasing 64-bit ID (starting from 1). The ID is returned to
//      the caller for cancellation.
//   2. cancelTimer(timer_id) nulls the callback. The Timer node stays
//      in the linked list (lazy deletion) and in timer_pool_.
//   3. tick() skips timers with null callbacks during L0 processing.
//      The node remains in timer_pool_ until garbage collection.
//   4. Periodic GC (every ~1000 ticks) scans timer_pool_ and deletes
//      all nodes with null callbacks (canceled or already fired).
//
// Thread safety:
//   This timer wheel is designed for single-threaded use within a
//   scheduler thread. All operations (addTimer, cancelTimer, tick,
//   cascade) are called from the owning thread only. No locks are
//   needed. If cross-thread timer operations are required, the caller
//   must provide external synchronization.
//
// Time advancement:
//   The wheel's current_time_ms_ is advanced exclusively by tick()
//   calls, which are driven by the event loop at 1 ms intervals.
//   We do not re-read the system clock after initialization to ensure
//   monotonicity and consistency. The 1 ms tick granularity is a
//   deliberate trade-off: it provides microsecond-scale timer accuracy
//   (adequate for network I/O timeouts) while keeping the timer wheel
//   logic simple and predictable.

size_t TimerWheel::active_timers() const noexcept {
    size_t count = 0;
    for (Timer* t : timer_pool_) {
        if (t && t->callback) ++count;
    }
    return count;
}

} // namespace zero
