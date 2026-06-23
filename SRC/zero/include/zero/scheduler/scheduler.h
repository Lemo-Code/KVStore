// zero Scheduler — M:N fiber scheduler
//
// Multiplexes M fibers onto N OS threads (N = CPU core count by default).
// Each thread runs an event loop (Reactor + TimerWheel) on a "main fiber".
// Work-stealing queues distribute fibers across threads for load balancing.
//
// Usage:
//   Scheduler sched;          // Create with N=CPU cores
//   sched.start();            // Launch worker threads (non-blocking)
//   sched.schedule(my_fiber); // Enqueue a fiber to run
//   sched.stop();             // Graceful shutdown, drains remaining fibers
//
// The scheduler is designed to be process-global (one instance per process).
// For simple apps, call InitZero() which creates a default scheduler.
#pragma once

#include <memory>
#include <vector>
#include <atomic>
#include <cstdint>

#include "zero/fiber/fiber.h"
#include "zero/thread/thread.h"
#include "zero/thread/mutex.h"

namespace zero {

class Reactor;
class WorkStealingQueue;
class TimerWheel;

class Scheduler {
public:
    // ============================================================
    // Construction
    // ============================================================

    // Create a scheduler with `num_threads` worker threads.
    // Pass 0 to auto-detect (uses all available CPU cores).
    // The scheduler does NOT start until start() is called.
    explicit Scheduler(size_t num_threads = 0);
    ~Scheduler();

    // Disable copy/move
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    // ============================================================
    // Lifecycle
    // ============================================================

    // Start the scheduler — launches worker threads.
    // Returns immediately; worker threads begin processing fibers.
    void start();

    // Stop the scheduler gracefully.
    // 1. Sets the stopping flag.
    // 2. Wakes all worker threads.
    // 3. Waits for all threads to finish (joins them).
    void stop();

    // ============================================================
    // Scheduling
    // ============================================================

    // Schedule a fiber to run. The fiber is enqueued on the calling
    // thread's local queue (or another thread's queue for load balancing).
    void schedule(Fiber::Ptr fiber);

    // Convenience: create a fiber from a callback and schedule it.
    void schedule(Fiber::Callback cb);

    // Schedule a fiber on a specific thread's queue.
    void schedule_on(int thread_id, Fiber::Ptr fiber);

    // ============================================================
    // Thread-local accessors
    // ============================================================

    // Get the current thread's main scheduler fiber (the event-loop fiber)
    static Fiber* GetSchedulerFiber();

    // Get the current thread's Scheduler instance
    static Scheduler* GetThis();

    // Get the current thread's reactor
    static Reactor* GetReactor();

    // Get the current thread's timer wheel
    static TimerWheel* GetTimerWheel();

    // ============================================================
    // Observers
    // ============================================================

    size_t thread_count() const noexcept { return threads_.size(); }
    bool is_stopping() const noexcept {
        return stopping_.load(std::memory_order_relaxed);
    }

    // ============================================================
    // Statistics
    // ============================================================

    // Total fibers scheduled since start
    uint64_t total_fibers_scheduled() const noexcept {
        return total_fibers_scheduled_.load(std::memory_order_relaxed);
    }

    // Total fibers completed (TERM + EXCEPT)
    uint64_t total_fibers_completed() const noexcept {
        return total_fibers_completed_.load(std::memory_order_relaxed);
    }

    // Current number of fibers in the system (approximate)
    size_t active_fiber_count() const noexcept;

    // Total idle ticks across all workers (when no fibers or events)
    uint64_t total_idle_ticks() const noexcept {
        return total_idle_ticks_.load(std::memory_order_relaxed);
    }

    // ============================================================
    // Per-thread state
    // ============================================================

    struct PerThread {
        int thread_id;
        Fiber::Ptr scheduler_fiber;        // Main fiber running reactor loop
        Fiber* current_fiber = nullptr;    // Currently executing fiber
        Reactor* reactor = nullptr;        // Per-thread epoll reactor
        WorkStealingQueue* local_queue = nullptr;  // Local fiber queue
        Scheduler* scheduler = nullptr;    // Owning scheduler
        bool is_idle = true;               // Currently idle (no fibers)
        uint64_t idle_ticks = 0;           // Total idle ticks
        uint64_t fibers_processed = 0;     // Fibers executed
    };

    // Thread-local pointer to current thread's PerThread state
    static thread_local PerThread* t_per_thread;

private:
    // Worker thread entry point
    void run(int thread_id);

    // Initialize per-thread data for a worker
    void init_per_thread(int thread_id);

    // Steal work from another thread's queue
    Fiber::Ptr steal_work(int thief_id);

    std::vector<std::unique_ptr<Thread>> threads_;
    std::vector<PerThread*> per_threads_;
    std::atomic<bool> stopping_{false};
    size_t num_threads_;

    // Global fallback queue (used when local + stealing fail)
    Mutex global_mutex_;
    std::vector<Fiber::Ptr> global_queue_;

    // Statistics
    std::atomic<uint64_t> total_fibers_scheduled_{0};
    std::atomic<uint64_t> total_fibers_completed_{0};
    std::atomic<uint64_t> total_idle_ticks_{0};
};

} // namespace zero
