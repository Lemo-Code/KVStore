// zero Fiber — stackful asymmetric coroutine with 6-state FSM
//
// Fibers are lightweight user-space threads that cooperatively yield
// to each other. Each fiber has its own stack (mmap'd, guard page
// protected). The scheduler multiplexes many fibers onto a few OS
// threads using work-stealing queues.
//
// States:
//   INIT   - Just created, never executed
//   READY  - Ready to run (in scheduler queue)
//   RUNNING - Currently executing on a thread
//   HOLD   - Blocked (waiting for I/O, timer, channel, lock, etc.)
//   TERM   - Finished normally
//   EXCEPT - Terminated by uncaught exception
//
// Usage:
//   auto f = Fiber::Create([]() { doWork(); });
//   scheduler->schedule(f);
//
// The main scheduler loop runs on a "main fiber" that is created with
// no callback and no stack — it acts as the anchor for context swaps.
#pragma once

#include <memory>
#include <functional>
#include <atomic>
#include <exception>
#include <string>

#include "zero/fiber/context.h"

namespace zero {

class Scheduler;
class FiberPool;

class Fiber : public std::enable_shared_from_this<Fiber> {
public:
    // ============================================================
    // Types
    // ============================================================

    enum class State : uint8_t {
        INIT = 0,    // Just created, never run
        READY = 1,   // Ready to run (enqueued)
        RUNNING = 2, // Currently executing
        HOLD = 3,    // Suspended (waiting for I/O or explicitly yielded)
        TERM = 4,    // Finished execution
        EXCEPT = 5   // Terminated with exception
    };

    using Callback = std::function<void()>;
    using Ptr = std::shared_ptr<Fiber>;

    static constexpr size_t kDefaultStackSize = 131072;  // 128 KB

    // ============================================================
    // Construction
    // ============================================================

    // Create a fiber with a callback and optional stack size.
    // The fiber is in INIT state; must be scheduled to run.
    Fiber(Callback cb, size_t stack_size = kDefaultStackSize);

    // Create the main scheduler fiber (no stack, no callback).
    // This constructor is only called by the Scheduler.
    Fiber();

    ~Fiber();

    // Factory: allocate from FiberPool (prefer this for hot paths)
    static Ptr Create(Callback cb, size_t stack_size = kDefaultStackSize);

    // ============================================================
    // Scheduling operations (called by Scheduler)
    // ============================================================

    // Resume execution of this fiber. Saves the current fiber's context
    // and restores this fiber's context.
    // Precondition: this fiber is in READY state.
    void resume();

    // Yield execution back to the scheduler.
    // Saves this fiber's context and restores the scheduler's main fiber.
    // Called by the fiber itself when it needs to block.
    void yield();

    // Low-level context switch in/out (used by context_swap)
    void swapIn();
    void swapOut();

    // ============================================================
    // Observers
    // ============================================================

    State getState() const noexcept { return state_; }
    void setState(State s) noexcept { state_ = s; }

    uint64_t id() const noexcept { return id_; }

    // Human-readable state name for debugging
    static const char* stateName(State s) noexcept;

    // Whether this fiber is the main scheduler fiber
    bool isMain() const noexcept { return is_main_; }

    // Get the current stack bottom (low address) and size
    void* stackBottom() const noexcept { return stack_; }
    size_t stackSize() const noexcept { return stack_size_; }

    // ============================================================
    // Thread-local accessors
    // ============================================================

    // Get the currently executing fiber on this thread.
    // Returns nullptr if no fiber is running.
    static Fiber* GetThis();

    // Get the current fiber's unique ID.
    // Returns 0 if no fiber is running.
    static uint64_t GetFiberId();

    // Get the main scheduler fiber for this thread
    static Ptr GetMainFiber();

    // Set the current fiber for this thread (called by Scheduler)
    static void SetThis(Fiber* fiber);

private:
    // Main fiber constructor (no stack, no callback)
    Fiber(bool is_main);

    // Static entry point for new fibers — calls cb_() and handles exceptions
    static void MainFunc();

    // Capture any uncaught exception from the fiber callback
    void captureException();

    uint64_t id_;
    State state_ = State::INIT;
    Context ctx_{};
    Callback cb_;
    void* stack_ = nullptr;
    size_t stack_size_ = 0;
    bool is_main_ = false;
    std::exception_ptr exception_;  // Captured exception for EXCEPT state

    // Thread-local state
    static thread_local Fiber* t_fiber;
    static thread_local Fiber::Ptr t_main_fiber;
    static std::atomic<uint64_t> s_fiber_id_counter;

    friend class Scheduler;
    friend class FiberPool;
};

// Convert state enum to string for logging/debugging
inline const char* Fiber::stateName(State s) noexcept {
    switch (s) {
        case State::INIT:    return "INIT";
        case State::READY:   return "READY";
        case State::RUNNING: return "RUNNING";
        case State::HOLD:    return "HOLD";
        case State::TERM:    return "TERM";
        case State::EXCEPT:  return "EXCEPT";
        default:             return "UNKNOWN";
    }
}

} // namespace zero
