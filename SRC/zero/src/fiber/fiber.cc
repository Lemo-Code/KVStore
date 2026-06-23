/*
 * zero Fiber — stackful asymmetric coroutine implementation
 *
 * Each Fiber represents an independent call stack.  A fiber is either the
 * "main" scheduler fiber (which runs the epoll event loop and has no
 * separate stack) or a user-created fiber with a mmap'd stack and a
 * user-supplied callback.
 *
 * State machine (6 states):
 *   INIT  → RUNNING  (first resume / swapIn)
 *   RUNNING → READY  (yield without blocking)
 *   RUNNING → HOLD   (yield while waiting for I/O / channel / mutex)
 *   RUNNING → TERM   (callback completed normally)
 *   RUNNING → EXCEPT (callback threw an exception)
 *   READY  → RUNNING (rescheduled by scheduler)
 *   HOLD   → READY   (I/O event arrived, timer fired, etc.) → RUNNING
 *
 * Context switching:
 *   Fiber switching uses the asymmetric model: every user fiber always
 *   yields back to the scheduler fiber.  The scheduler fiber never yields
 *   — it returns from swapIn() when the target fiber yields.
 *
 *   swapIn():  scheduler → user fiber     (called by scheduler)
 *   yield():   user fiber → scheduler     (called by user fiber)
 *
 * Thread safety:
 *   - s_fiber_id is atomic (monotonically increasing global counter)
 *   - t_fiber and t_main_fiber are thread-local (no locking needed)
 *   - A fiber is only ever resumed/yielded by the thread that owns it
 */

#include "zero/fiber/fiber.h"
#include "zero/fiber/stack_pool.h"
#include "zero/scheduler/scheduler.h"

#include <cassert>
#include <cstring>
#include <exception>
#include <new>

namespace zero {

/* =========================================================================
 * Static member definitions
 * ======================================================================== */

thread_local Fiber*       Fiber::t_fiber       = nullptr;
thread_local Fiber::Ptr   Fiber::t_main_fiber  = nullptr;
std::atomic<uint64_t>     Fiber::s_fiber_id_counter{1}; // 0 reserved for "no fiber"

/* =========================================================================
 * Scheduler-fiber accessor
 *
 * Scheduler::GetSchedulerFiber() returns the per-thread scheduler fiber.
 * We provide a short-cut helper in an anonymous namespace so that the
 * yield/swap paths can call it without qualifying Scheduler:: each time.
 * ======================================================================== */

namespace {

inline Fiber* GetSchedulerFiber() {
    auto* pt = Scheduler::t_per_thread;
    if (pt && pt->scheduler_fiber) {
        return pt->scheduler_fiber.get();
    }
    return nullptr;
}

} // anonymous namespace

/* =========================================================================
 * Constructor: main / scheduler fiber (no stack, no callback)
 *
 * The scheduler creates one of these per worker thread.  It runs the
 * epoll event loop and never yields — its Context is only used as the
 * *from* side of context_swap (save path), so rsp/rip don't need to
 * point to anything meaningful.
 * ======================================================================== */

Fiber::Fiber()
    : id_(s_fiber_id_counter.fetch_add(1, std::memory_order_relaxed))
    , state_(State::RUNNING)
    , stack_(nullptr)
    , stack_size_(0)
{
    ctx_ = Context{};
    t_fiber = this;
    // t_main_fiber is set by Scheduler::run() after the shared_ptr<Fiber>
    // is constructed, because shared_from_this() is not yet available here.
}

/* =========================================================================
 * Constructor: private main-fiber constructor
 *
 * Provided for friend classes (Scheduler) that want to make the role
 * explicit.  Delegates entirely to Fiber().
 * ======================================================================== */

Fiber::Fiber(bool /*is_main*/) : Fiber() {}

/* =========================================================================
 * Constructor: user fiber (allocates stack, initializes context)
 *
 * On failure to allocate a stack, the fiber transitions to EXCEPT state
 * immediately.  Callers should check getState() after construction.
 * ======================================================================== */

Fiber::Fiber(Callback cb, size_t stack_size)
    : id_(s_fiber_id_counter.fetch_add(1, std::memory_order_relaxed))
    , state_(State::INIT)
    , cb_(std::move(cb))
    , stack_size_(stack_size)
{
    if (stack_size_ == 0) {
        stack_size_ = 131072; // 128 KB default
    }

    try {
        stack_ = StackPool::instance().allocate(stack_size_);
        if (!stack_) {
            state_ = State::EXCEPT;
            return;
        }

        // stack_ points to the HIGH end of the mapped region (stack grows
        // downward).  On first swap-in the CPU will execute &Fiber::MainFunc
        // with %rsp set to this address.
        ctx_.init(&Fiber::MainFunc, stack_);
    } catch (const std::bad_alloc&) {
        state_ = State::EXCEPT;
        stack_ = nullptr;
    } catch (...) {
        state_ = State::EXCEPT;
        stack_ = nullptr;
    }
}

/* =========================================================================
 * Destructor — return the stack to StackPool if we own one
 * ======================================================================== */

Fiber::~Fiber() {
    if (stack_ && stack_size_ > 0) {
        StackPool::instance().deallocate(stack_, stack_size_);
        stack_ = nullptr;
    }
}

/* =========================================================================
 * resume() — called by scheduler to make this fiber run
 *
 * Precondition: caller is the scheduler fiber (t_fiber points to it).
 * Saves the current fiber's state and swaps to this fiber.
 * Returns after the target fiber yields back.
 * ======================================================================== */

void Fiber::resume() {
    Fiber* cur = GetThis();
    assert(cur != nullptr && "resume() requires a running scheduler fiber");
    assert(cur != this && "cannot resume self");
    assert(state_ == State::INIT || state_ == State::READY ||
           state_ == State::HOLD);

    // The scheduler fiber returns to READY so it can be re-entered
    if (cur->state_ == State::RUNNING) {
        cur->state_ = State::READY;
    }

    state_ = State::RUNNING;
    swapIn();
}

/* =========================================================================
 * yield() — called by this fiber to give control back to the scheduler
 *
 * Preserves TERM/EXCEPT states (a completed fiber should not become
 * READY again).  Swaps to the per-thread scheduler fiber.
 * ======================================================================== */

void Fiber::yield() {
    // A terminal fiber stays terminal — don't overwrite TERM or EXCEPT
    if (state_ != State::TERM && state_ != State::EXCEPT) {
        state_ = State::READY;
    }

    Fiber* scheduler = GetSchedulerFiber();
    if (!scheduler) {
        state_ = State::EXCEPT;
        return;
    }

    context_swap(&ctx_, &scheduler->ctx_);
    // Execution resumes here when this fiber is swapped in again later.
}

/* =========================================================================
 * swapIn() — low-level switch from scheduler to this fiber
 *
 * Sets t_fiber = this so GetThis() returns the correct value inside the
 * target fiber.  After the target yields and we return here, t_fiber is
 * restored to the scheduler.
 * ======================================================================== */

void Fiber::swapIn() {
    Fiber* scheduler = GetSchedulerFiber();
    assert(scheduler != nullptr);

    t_fiber = this;
    context_swap(&scheduler->ctx_, &ctx_);
    // Arrive here when `this` fiber calls yield() / swapOut().
    // Restore t_fiber so the scheduler loop sees itself.
    t_fiber = scheduler;
}

/* =========================================================================
 * swapOut() — low-level switch from this fiber back to scheduler
 * ======================================================================== */

void Fiber::swapOut() {
    Fiber* scheduler = GetSchedulerFiber();
    if (!scheduler) {
        state_ = State::EXCEPT;
        return;
    }
    context_swap(&ctx_, &scheduler->ctx_);
    // Execution resumes here when this fiber is swapped in again.
}

/* =========================================================================
 * MainFunc() — static entry point for every new user fiber
 *
 * Invokes the user's callback inside a try-catch so that exceptions
 * never escape through the fiber boundary.  After the callback returns
 * (or throws), the fiber transitions to TERM or EXCEPT and performs a
 * final yield.  This yield never returns — the scheduler recycles or
 * destroys the fiber.
 * ======================================================================== */

void Fiber::MainFunc() {
    Fiber* self = GetThis();
    if (!self) {
        return; // Should never happen
    }

    try {
        if (self->cb_) {
            self->cb_();
        }
        self->state_ = State::TERM;
    } catch (const std::exception&) {
        self->state_ = State::EXCEPT;
    } catch (...) {
        self->state_ = State::EXCEPT;
    }

    // Final yield — the scheduler sees TERM/EXCEPT and will not reschedule
    // this fiber.  Execution never returns from this call.
    self->yield();

    // If we somehow reach here (scheduler bug), hard-abort to prevent
    // stack unwind through the initial context frame.
    std::terminate();
}

/* =========================================================================
 * GetThis() — returns the currently executing fiber on this thread
 * ======================================================================== */

Fiber* Fiber::GetThis() {
    return t_fiber;
}

/* =========================================================================
 * GetFiberId() — returns the current fiber's ID, or 0 if none
 * ======================================================================== */

uint64_t Fiber::GetFiberId() {
    Fiber* cur = t_fiber;
    return cur ? cur->id() : 0;
}

} // namespace zero
