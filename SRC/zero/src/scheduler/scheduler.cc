// zero Scheduler implementation — M:N fiber scheduler
//
// The Scheduler is the heart of the zero framework. It multiplexes M user
// fibers onto N OS threads (typically one per CPU core) using cooperative
// scheduling. Each worker thread runs an event loop:
//
//   1. Poll I/O readiness via the per-thread Reactor (epoll)
//   2. Drain the local Chase-Lev work-stealing queue (LIFO pops)
//   3. Tick the timer wheel to fire expired timers
//   4. If idle, steal work from a random sibling thread
//   5. If still idle, nanosleep briefly to avoid busy-waiting
//
// Fibers are cooperative — they yield voluntarily when they would otherwise
// block (I/O, timers, locks, channels). The scheduler never preempts a
// running fiber. This cooperative model eliminates most concurrency bugs
// (no data races between fibers on the same thread) while still providing
// the performance benefits of asynchronous I/O.
//
// Key design decisions:
//
//   - One Reactor per thread: eliminates synchronization on the epoll
//     instance. Each thread manages its own set of file descriptors.
//     FDs are assigned to threads at connection-accept time.
//
//   - Work-stealing: the Chase-Lev deque allows the owner thread to push
//     and pop without atomic CAS (in the common case), while thieves use
//     CAS to steal from the opposite end. This provides near-optimal
//     load balancing with minimal contention.
//
//   - Timer wheel per thread: each reactor has its own 5-level timer
//     wheel. This eliminates locking on timer operations and keeps
//     timer callbacks thread-local.
//
//   - FiberPool: finished fibers are recycled to a global pool rather
//     than being freed. This avoids the cost of stack allocation (mmap/
//     munmap) on the hot path.
//
//   - CPU pinning: each worker thread is pinned to a dedicated CPU core
//     to maximize cache affinity and minimize migration overhead.
//
// The "main scheduler fiber" on each thread is a special fiber with no
// stack and no callback. It acts as the anchor for context_swap: when a
// user fiber yields, control returns to this fiber, which continues the
// event loop. When the event loop resumes a user fiber, it context_swaps
// from the scheduler fiber to the user fiber.

#include "zero/scheduler/scheduler.h"
#include "zero/scheduler/reactor.h"
#include "zero/scheduler/work_stealing_queue.h"
#include "zero/scheduler/timer_wheel.h"
#include "zero/thread/cpu_affinity.h"
#include "zero/fiber/fiber.h"
#include "zero/fiber/fiber_pool.h"
#include "zero/base/macro.h"

#include <unistd.h>
#include <time.h>
#include <algorithm>

namespace zero {

// ============================================================
// Thread-local pointer to the current thread's PerThread state
// ============================================================
//
// This is the central lookup structure for all scheduler operations
// on a given thread. It is set by run() before the event loop starts
// and cleared when the thread exits. All fiber operations on this
// thread access it to find the scheduler, reactor, queues, etc.
//
// Declared in the header as: static thread_local PerThread* t_per_thread;
thread_local Scheduler::PerThread* Scheduler::t_per_thread = nullptr;

// ============================================================
// Free function: GetSchedulerFiber()
// ============================================================
//
// Returns a pointer to the scheduler fiber (the main event-loop fiber)
// for the calling thread. Returns nullptr if called from a non-scheduler
// thread. Used by fiber.cc to implement Fiber::GetMainFiber() and
// Fiber::yield() — when a fiber yields, it must know which scheduler
// fiber to swap back to.
//
// The scheduler fiber is a special Fiber with no stack and no callback.
// It exists purely as a context-switch target; when swapped in, execution
// continues at whatever point the event loop left off.

Fiber* GetSchedulerFiber() {
    auto* pt = Scheduler::t_per_thread;
    if (pt != nullptr && pt->scheduler_fiber) {
        return pt->scheduler_fiber.get();
    }
    return nullptr;
}

// ============================================================
// Static method: Scheduler::GetSchedulerFiber()
// ============================================================
//
// Member-function version of the free function above. Identical behavior;
// provided for consistency with GetThis().

Fiber* Scheduler::GetSchedulerFiber() {
    auto* pt = t_per_thread;
    if (pt != nullptr && pt->scheduler_fiber) {
        return pt->scheduler_fiber.get();
    }
    return nullptr;
}

// ============================================================
// Static method: Scheduler::GetThis()
// ============================================================
//
// Returns a pointer to the Scheduler instance that owns the calling
// thread. Returns nullptr if called from a non-scheduler thread.
// Used by fibers to access the scheduler for operations like
// scheduling new fibers, accessing the reactor, etc.

Scheduler* Scheduler::GetThis() {
    auto* pt = t_per_thread;
    if (pt != nullptr) {
        return pt->scheduler;
    }
    return nullptr;
}

// ============================================================
// Constructor
// ============================================================

Scheduler::Scheduler(size_t num_threads) {
    // When num_threads is 0 (the default), auto-detect the number of
    // logical CPU cores on this machine. This is typically the right
    // choice for I/O-bound workloads where one thread per core maximizes
    // throughput without excessive context switching.
    //
    // For CPU-bound workloads, using fewer threads than cores can reduce
    // contention. For purely I/O-bound workloads (e.g., proxy servers),
    // using more threads than cores can improve latency by keeping the
    // CPU fed while some threads block in the kernel. The caller can
    // override the default by passing an explicit count.
    if (num_threads == 0) {
        num_threads_ = static_cast<size_t>(get_cpu_count());
    } else {
        num_threads_ = num_threads;
    }

    // At minimum, we need one worker thread. Zero threads would mean
    // no event loop runs and no fibers execute.
    if (num_threads_ < 1) {
        num_threads_ = 1;
    }

    // Pre-allocate the per-thread state vectors. This avoids reallocation
    // in start() which would be problematic if start() were called from
    // multiple threads (though it shouldn't be).
    per_threads_.reserve(num_threads_);

    // No threads are created until start() is called.
}

// ============================================================
// Destructor
// ============================================================

Scheduler::~Scheduler() {
    // Ensure all threads are stopped. stop() is idempotent — calling it
    // multiple times is safe. If start() was never called, stop() is a
    // no-op because per_threads_ is empty.
    stop();

    // Clean up per-thread state. At this point all threads have joined,
    // so no thread can be accessing these structures.
    //
    // Cleanup order: local_queue first (may reference fibers), then
    // reactor (may reference the timer wheel and epoll FDs), then the
    // PerThread structure itself.
    for (auto* pt : per_threads_) {
        if (pt != nullptr) {
            // Delete the work-stealing queue. Any fibers remaining in
            // the queue at shutdown are abandoned — their destructors
            // will run when the shared_ptr refcount reaches zero.
            delete pt->local_queue;
            pt->local_queue = nullptr;

            // Delete the reactor. This closes the epoll fd, the wakeup
            // eventfd, and destroys the timer wheel.
            delete pt->reactor;
            pt->reactor = nullptr;

            // The scheduler_fiber shared_ptr is cleared automatically
            // by the PerThread destructor. The Fiber destructor will
            // free the (non-existent) stack.

            delete pt;
        }
    }
    per_threads_.clear();
}

// ============================================================
// start() — launch all worker threads
// ============================================================

void Scheduler::start() {
    // Reset the stop flag in case we are restarting after a previous
    // start/stop cycle.
    stopping_.store(false, std::memory_order_relaxed);

    // Create PerThread structures and Thread objects for each worker.
    // We create all PerThreads first (under no lock, since start() is
    // not thread-safe by design), then start all threads.
    for (size_t i = 0; i < num_threads_; ++i) {
        int thread_id = static_cast<int>(i);

        // Allocate and initialize the per-thread state. This struct
        // is owned by the scheduler and deleted in stop()/~Scheduler().
        auto* pt = new PerThread();
        pt->thread_id      = thread_id;
        pt->scheduler      = this;
        pt->reactor        = new Reactor();
        pt->local_queue    = new WorkStealingQueue(1024);
        // scheduler_fiber and current_fiber are initialized in run().
        per_threads_.push_back(pt);

        // Create the OS thread. The lambda captures this, thread_id,
        // and pt (raw pointer — pt outlives the thread because we join
        // all threads before deleting pt).
        std::string name = "zero-wk-" + std::to_string(thread_id);
        auto thread = std::make_unique<Thread>(
            [this, pt]() {
                this->run(pt->thread_id);
            },
            name
        );

        // Start the thread. Thread::start() blocks until the thread
        // has actually begun execution (internal semaphore), ensuring
        // that the thread is ready before we proceed.
        if (!thread->start()) {
            ZERO_ASSERT_MSG(false, "Scheduler: failed to start worker thread");
        }

        threads_.push_back(std::move(thread));

        // Pin the thread to a dedicated CPU core for cache affinity.
        // We distribute workers across cores, starting from core 0.
        // For systems with hyperthreading, the OS scheduler will
        // typically pair logical cores on the same physical core, but
        // we do not attempt to detect SMT topology here.
        if (threads_.back() && num_threads_ > 1) {
            int cpu_count = get_cpu_count();
            int core = thread_id;
            if (cpu_count > 0) {
                core = core % cpu_count;
            }
            set_cpu_affinity(threads_.back()->native_handle(), core);
        }
    }
}

// ============================================================
// stop() — graceful shutdown
// ============================================================

void Scheduler::stop() {
    // Signal all threads to stop. Using memory_order_release ensures
    // that all prior writes (e.g., to shared state) are visible to
    // threads that subsequently observe stopping_ == true with acquire
    // ordering.
    //
    // The compare_exchange prevents multiple concurrent stop() calls
    // from double-waking or double-cleaning.
    bool expected = false;
    stopping_.compare_exchange_strong(expected, true,
                                       std::memory_order_release,
                                       std::memory_order_relaxed);

    // Wake up each reactor so that any thread blocked in epoll_wait
    // returns immediately. The reactor's poll() method checks the
    // stopping_ flag after wakeup and returns.
    for (auto* pt : per_threads_) {
        if (pt != nullptr && pt->reactor != nullptr) {
            pt->reactor->wakeup();
        }
    }

    // Join all worker threads. Each thread will exit its event loop
    // upon observing stopping_ == true, drain its local queue, and
    // return from run(). join() blocks until each thread has fully
    // terminated.
    for (auto& t : threads_) {
        if (t != nullptr) {
            t->join();
        }
    }

    // Clear the thread vector after joining. The Thread objects are
    // destroyed (which detaches if still running, though they shouldn't
    // be at this point).
    threads_.clear();
}

// ============================================================
// schedule(Fiber::Ptr) — enqueue a ready fiber
// ============================================================

void Scheduler::schedule(Fiber::Ptr fiber) {
    if (!fiber) {
        return;  // Null fibers are silently ignored
    }

    // The fiber enters the READY state. The event loop checks for
    // READY fibers and resumes them. Fibers that are already READY
    // (double-scheduling) are harmless — they just get an extra turn.
    fiber->setState(Fiber::State::READY);

    auto* pt = t_per_thread;
    if (pt != nullptr && pt->local_queue != nullptr) {
        // Fast path: called from within a scheduler thread.
        // Push to the local queue — this fiber will be executed on
        // the current thread, maximizing cache locality.
        pt->local_queue->push(std::move(fiber));
    } else {
        // Slow path: called from an external thread (e.g., accept thread,
        // signal handler, or before the scheduler is started).
        // Distribute the fiber to a worker thread's queue.
        //
        // We use the fiber ID modulo num_threads_ as a simple hash to
        // pick a target thread. This provides a consistent mapping so
        // that related fibers (e.g., from the same connection) tend to
        // land on the same thread, improving cache locality.
        size_t idx = fiber->id() % num_threads_;
        if (idx < per_threads_.size() && per_threads_[idx] != nullptr &&
            per_threads_[idx]->local_queue != nullptr) {
            per_threads_[idx]->local_queue->push(std::move(fiber));
        }
        // If the target thread isn't ready (racing with startup/shutdown),
        // the fiber is dropped. This is acceptable because scheduling
        // from outside the scheduler is inherently best-effort.
    }
}

// ============================================================
// schedule(Callback) — create a fiber from a callback and schedule it
// ============================================================

void Scheduler::schedule(Fiber::Callback cb) {
    // Allocate a fiber from the global fiber pool. This avoids the
    // expensive stack allocation (mmap) on every fiber creation.
    // The pool recycles fibers that have finished execution.
    auto fiber = FiberPool::instance().get(std::move(cb));
    schedule(std::move(fiber));
}

// ============================================================
// run(thread_id) — the worker thread's event loop
// ============================================================
//
// This is the core of the scheduler. Each worker thread executes this
// function, which runs an infinite loop (until stopping_) that:
//
//   1. Polls the reactor for I/O events (with timer-driven timeout)
//   2. Drains and executes fibers from the local work queue
//   3. Processes expired timers (done by reactor::poll internally)
//   4. Attempts work-stealing from a sibling thread if idle
//   5. Brief nanosleep if completely idle to avoid CPU spin
//
// The event loop is designed to never block for long: the reactor poll
// timeout is at most 1ms, and the idle sleep is at most 1ms. This keeps
// the scheduler responsive to new work arriving from other threads.

void Scheduler::run(int thread_id) {
    // ==================================================================
    // Initialization
    // ==================================================================

    // Retrieve our per-thread state (allocated in start()).
    PerThread* pt = per_threads_[static_cast<size_t>(thread_id)];
    ZERO_ASSERT(pt != nullptr);

    // Install this thread's state into the thread-local pointer.
    // From this point on, t_per_thread is valid for this thread.
    t_per_thread = pt;

    // Create the scheduler fiber (main loop fiber).
    // This is a special fiber with an empty stack (nullptr, 0 size) and
    // no callback. It is never "resumed" in the normal sense — when
    // context_swap is called with this fiber as the target, execution
    // continues at the point after the context_swap call that saved
    // this fiber's state (which is inside fiber::yield() or fiber::resume()).
    pt->scheduler_fiber = std::make_shared<Fiber>();

    // Mark the scheduler fiber as the currently-executing fiber.
    // User fibers use Fiber::GetThis() to find themselves; the scheduler
    // fiber must also be findable so that yield() knows where to return.
    Fiber::SetThis(pt->scheduler_fiber.get());

    // Set a readable thread name for debugging with top, htop, gdb, etc.
    std::string thread_name = "zero-fiber-" + std::to_string(thread_id);
    Thread::set_current_name(thread_name);

    // Pre-allocate a small number of fibers in the global pool.
    // This ensures that the first few schedule(callback) calls don't
    // need to mmap fiber stacks, reducing latency jitter.
    FiberPool::instance().preallocate(32);

    // Aliases for hot-path access. Local references avoid repeated
    // pointer dereferencing through pt->.
    Reactor&           reactor = *pt->reactor;
    WorkStealingQueue& local_q = *pt->local_queue;
    FiberPool&         fpool   = FiberPool::instance();

    // ==================================================================
    // Main event loop
    // ==================================================================

    while (ZERO_LIKELY(!stopping_.load(std::memory_order_relaxed))) {
        // ----------------------------------------------------------
        // Step 1: Poll I/O readiness via the Reactor
        // ----------------------------------------------------------
        //
        // The reactor performs epoll_wait with a short timeout (1ms).
        // Even with no I/O events, this returns quickly so that we can
        // check for fibers, timers, and the stop flag.
        //
        // The timer wheel is ticked inside reactor::poll() after
        // processing I/O events. Expired timer callbacks may schedule
        // new fibers, which will appear in the local queue.
        int num_events = reactor.poll(1);
        ZERO_UNUSED(num_events);  // Suppress warning; used for debugging

        // ----------------------------------------------------------
        // Step 2: Drain the local work queue
        // ----------------------------------------------------------
        //
        // Pop fibers from the back of the deque (LIFO). LIFO order
        // provides better cache locality because the most recently
        // scheduled fiber's data is likely still in CPU cache.
        //
        // Each fiber is resumed by context_swap from the scheduler
        // fiber to the user fiber. When the user fiber yields (or
        // terminates), control returns here.
        while (!stopping_.load(std::memory_order_relaxed)) {
            Fiber::Ptr fiber = local_q.pop();
            if (!fiber) {
                break;  // Queue is empty
            }

            // Skip fibers that are already terminated. This can happen
            // if a fiber was scheduled but then canceled (e.g., via
            // a timeout) before being executed.
            Fiber::State state = fiber->getState();
            if (state == Fiber::State::TERM ||
                state == Fiber::State::EXCEPT) {
                fpool.recycle(std::move(fiber));
                continue;
            }

            // Resume the fiber. When resume() returns, the fiber has
            // either yielded (HOLD), terminated (TERM), finished (EXCEPT),
            // or explicitly set itself to READY (re-enqueue).
            pt->current_fiber = fiber.get();
            fiber->resume();
            pt->current_fiber = nullptr;

            // Post-resume state machine:
            state = fiber->getState();
            switch (state) {
            case Fiber::State::TERM:
            case Fiber::State::EXCEPT:
                // Fiber finished — recycle to the pool.
                fpool.recycle(std::move(fiber));
                break;

            case Fiber::State::READY:
                // Fiber yielded voluntarily (e.g., sched_yield equivalent).
                // Re-enqueue it at the back of the local queue.
                local_q.push(std::move(fiber));
                break;

            case Fiber::State::HOLD:
                // Fiber is blocked on I/O, a timer, a lock, or a channel.
                // It will be re-scheduled by the wakeup mechanism when the
                // blocking condition is resolved.
                break;

            default:
                // RUNNING, INIT: should not be possible post-resume.
                break;
            }
        }

        // ----------------------------------------------------------
        // Step 3: Work stealing
        // ----------------------------------------------------------
        //
        // If we have no local work, try to steal from a sibling thread.
        // We pick a victim pseudo-randomly based on the thread ID and
        // the scheduler fiber's ID. The Chase-Lev steal() operation
        // takes from the top (FIFO) end, which typically contains older
        // fibers that are less cache-hot — stealing them doesn't hurt
        // the victim's cache locality much.
        if (local_q.empty() && per_threads_.size() > 1) {
            // Select a victim: (self_id + hash) % num_threads, but not self.
            size_t victim = static_cast<size_t>(
                (pt->scheduler_fiber->id() +
                 static_cast<uint64_t>(thread_id)) % per_threads_.size()
            );
            if (victim == static_cast<size_t>(thread_id)) {
                victim = (victim + 1) % per_threads_.size();
            }

            PerThread* victim_pt = per_threads_[victim];
            if (victim_pt && victim_pt->local_queue &&
                !victim_pt->local_queue->empty()) {
                Fiber::Ptr stolen = victim_pt->local_queue->steal();
                if (stolen) {
                    // Execute the stolen fiber immediately, same as local.
                    Fiber::State state = stolen->getState();
                    if (state != Fiber::State::TERM &&
                        state != Fiber::State::EXCEPT) {
                        pt->current_fiber = stolen.get();
                        stolen->resume();
                        pt->current_fiber = nullptr;

                        state = stolen->getState();
                        if (state == Fiber::State::TERM ||
                            state == Fiber::State::EXCEPT) {
                            fpool.recycle(std::move(stolen));
                        } else if (state == Fiber::State::READY) {
                            local_q.push(std::move(stolen));
                        }
                    } else {
                        fpool.recycle(std::move(stolen));
                    }
                }
            }
        }

        // ----------------------------------------------------------
        // Step 4: Idle handling
        // ----------------------------------------------------------
        //
        // If there was no I/O activity and no fibers to run (either
        // locally or stolen), we nanosleep briefly. This prevents the
        // scheduler from busy-waiting at 100% CPU when idle, while
        // keeping wakeup latency low (~1ms in the worst case).
        //
        // The reactor's eventfd wakeup mechanism ensures that if another
        // thread schedules work for us, we wake up immediately (the
        // nanosleep is interrupted by the eventfd write making epoll_fd
        // readable, which causes the next reactor::poll to return).
        //
        // We check ALL queues because without I/O events, we might
        // still have work queued in other threads.
        if (num_events == 0 && local_q.empty()) {
            bool any_work = false;
            for (auto* other : per_threads_) {
                if (other && other != pt && other->local_queue &&
                    !other->local_queue->empty()) {
                    any_work = true;
                    break;
                }
            }
            if (!any_work) {
                struct timespec idle = {0, 1000000}; // 1 ms
                nanosleep(&idle, nullptr);
            }
        }
    }

    // ==================================================================
    // Graceful shutdown
    // ==================================================================
    //
    // Drain any remaining fibers in the local queue. These fibers were
    // scheduled before the stop flag was set. We execute them to
    // completion so that their cleanup logic (destructors, RAII guards)
    // runs properly.
    while (true) {
        Fiber::Ptr fiber = local_q.pop();
        if (!fiber) {
            break;
        }

        Fiber::State state = fiber->getState();
        if (state == Fiber::State::TERM ||
            state == Fiber::State::EXCEPT) {
            fpool.recycle(std::move(fiber));
            continue;
        }

        pt->current_fiber = fiber.get();
        fiber->resume();
        pt->current_fiber = nullptr;

        state = fiber->getState();
        if (state == Fiber::State::TERM ||
            state == Fiber::State::EXCEPT) {
            fpool.recycle(std::move(fiber));
        }
    }

    // Clear the current fiber pointer and thread-local state.
    // This signals to any code that checks t_per_thread that this
    // thread is no longer participating in the scheduler.
    pt->current_fiber = nullptr;
    pt->scheduler_fiber.reset();
    Fiber::SetThis(nullptr);
    t_per_thread = nullptr;
}

} // namespace zero
