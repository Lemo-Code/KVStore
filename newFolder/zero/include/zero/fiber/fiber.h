/**
 * @file    fiber.h
 * @brief   Stackful fiber (coroutine) with cooperative scheduling.
 *
 * Each fiber has its own stack and execution context. Fibers yield
 * voluntarily — there is no preemption. When a fiber blocks on I/O,
 * it yields to the scheduler which switches to another runnable fiber.
 *
 * Lifecycle:  READY → RUNNING ↔ HOLD → TERM
 *
 * Fibers are NOT thread-safe; each thread has its own fiber set.
 *
 * @ingroup fiber
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <atomic>
#include <string>

#include "zero/fiber/context.h"
#include "zero/base/noncopyable.h"

namespace zero {

class Fiber : public noncopyable {
public:
    /// Fiber states
    enum State {
        READY = 0,   ///< Ready to run.
        RUNNING = 1, ///< Currently executing.
        HOLD = 2,    ///< Blocked (waiting for I/O or timer).
        TERM = 3     ///< Finished execution.
    };

    /// Default stack size: 128KB
    static const size_t kDefaultStackSize = 128 * 1024;

    /// Callback type
    using Callback = std::function<void()>;

    /**
     * @brief  Create a fiber with callback and optional stack size.
     * @param cb         Function to execute.
     * @param stack_size Stack size (0 = default 128KB).
     * @param run_in_scheduler If true, fiber auto-schedules itself.
     */
    explicit Fiber(Callback cb, size_t stack_size = 0,
                   bool run_in_scheduler = true);

    /**
     * @brief  Create the main fiber (representing the thread's main context).
     *         This constructor is used internally.
     */
    explicit Fiber();

    ~Fiber();

    /// Resume execution of this fiber.
    void resume();

    /// Yield execution back to the scheduler.
    void yield();

    // ---- Accessors ----
    uint64_t id() const { return id_; }
    State state() const { return state_; }
    void set_state(State s) { state_ = s; }
    bool is_main() const { return is_main_; }

    /// Get/set the currently executing fiber on this thread.
    static Fiber* GetCurrent();
    static void SetCurrent(Fiber* f);

    /// Get the main fiber for this thread.
    static Fiber* GetMainFiber();

private:
    /// Entry point wrapper — calls callback then returns to scheduler.
    static void MainFunc(void* arg);

    uint64_t    id_;            ///< Unique fiber ID.
    State       state_;         ///< Current state.
    bool        is_main_;       ///< True if this is the main (thread) fiber.
    size_t      stack_size_;    ///< Stack size in bytes.
    void*       stack_;         ///< Stack memory (mmap'd).
    FiberContext ctx_;          ///< Saved execution context.
    Callback    cb_;            ///< User callback.

    // Fiber that yielded to this one (saved for yield back)
    Fiber* back_;

    static std::atomic<uint64_t> s_id_counter;
    static thread_local Fiber* t_current_fiber;
    static thread_local Fiber* t_main_fiber;
};

} // namespace zero
