/*
 * zero FiberPool — pre-constructed Fiber cache for the hot path
 *
 * Motivation:
 *   Creating a Fiber is expensive — it requires mmap + mprotect for the
 *   stack (and eventual munmap on destruction).  In a steady-state server,
 *   fibers are created and destroyed at high frequency (one per request,
 *   one per connection, etc.).  Reusing fiber objects eliminates this
 *   overhead — the stack is already mapped, and only the Context struct
 *   needs to be re-initialized.
 *
 * Lifecycle:
 *   preallocate(N)   —  create N idle fibers, push to pool
 *   get(cb)          —  pop an idle fiber (or allocate new), set callback
 *   recycle(fiber)   —  clear callback, push back to pool (if not full)
 *
 * Pool cap:
 *   The pool is bounded at kMaxPoolSize (256).  When full, recycled fibers
 *   are released — the shared_ptr drops, ~Fiber() runs, and the stack is
 *   returned to StackPool (where it may be cached or unmapped).
 *
 * Thread safety:
 *   All public methods acquire the SpinLock.  In get(), the lock is
 *   released before falling through to new-Fiber creation (which itself
 *   acquires StackPool's lock) to avoid holding two spinlocks at once.
 */

#include "zero/fiber/fiber_pool.h"
#include "zero/thread/spinlock.h"

#include <utility>

namespace zero {

namespace {

// Maximum number of idle fibers to cache.  Beyond this, recycled fibers
// are dropped so their stacks can be freed.
constexpr size_t kMaxPoolSize = 256;

} // anonymous namespace

/* =========================================================================
 * Singleton
 * ======================================================================== */

FiberPool& FiberPool::instance() {
    static FiberPool pool;
    return pool;
}

/* =========================================================================
 * get() — obtain a ready-to-run Fiber with the given callback
 *
 * Pops from the pool when possible; otherwise creates a brand-new Fiber.
 * The lock is released before the fallback creation path to avoid holding
 * two spinlocks simultaneously (Fiber constructor → StackPool::allocate).
 * ======================================================================== */

Fiber::Ptr FiberPool::get(Fiber::Callback cb, size_t stack_size) {
    // Try the cache first, under the lock
    {
        ScopedSpinLock guard(lock_);

        if (!pool_.empty()) {
            Fiber::Ptr fiber = std::move(pool_.back());
            pool_.pop_back();

            // Re-initialize: swap in the new callback, reset state to INIT,
            // and re-initialize the execution context so MainFunc starts
            // with a fresh stack frame on the first swap-in.
            //
            // FiberPool is a friend of Fiber — direct member access.
            fiber->cb_     = std::move(cb);
            fiber->state_  = Fiber::State::INIT;

            // stack_ is the top of the stack.  Context::init() sets
            // rsp=stack_top and rip=&Fiber::MainFunc.
            fiber->ctx_.init(&Fiber::MainFunc, fiber->stack_);

            return fiber;
        }
    }
    // Lock released — safe to create a new fiber (which calls into
    // StackPool::instance().allocate(), itself spinlock-protected).

    return std::make_shared<Fiber>(std::move(cb), stack_size);
}

/* =========================================================================
 * recycle() — return a terminated fiber to the pool
 *
 * Resets the fiber to a clean state (null callback, TERM state) and
 * pushes it back for reuse.  If the pool is at capacity the fiber is
 * simply dropped — ~Fiber() returns the stack to StackPool.
 * ======================================================================== */

void FiberPool::recycle(Fiber::Ptr fiber) {
    if (!fiber) return;

    // Reset to a clean terminal state.  At this point the fiber is
    // guaranteed not to be running on any thread.
    fiber->cb_    = nullptr;
    fiber->state_ = Fiber::State::TERM;

    ScopedSpinLock guard(lock_);

    if (pool_.size() < kMaxPoolSize) {
        pool_.push_back(std::move(fiber));
    }
    // else: fiber goes out of scope → refcount → 0 → ~Fiber()
    //       → StackPool::deallocate(stack_, stack_size_)
}

/* =========================================================================
 * preallocate() — bulk-create N idle fibers for the pool
 *
 * Fibers are created with an empty callback and marked TERM (they have
 * never run).  On the first get(), the callback is replaced and the
 * context re-initialized.
 *
 * Stops early if the pool reaches kMaxPoolSize or a Fiber allocation
 * fails (out of memory / virtual address space exhausted).
 * ======================================================================== */

void FiberPool::preallocate(size_t count, size_t stack_size) {
    if (count == 0) return;

    ScopedSpinLock guard(lock_);

    for (size_t i = 0; i < count; ++i) {
        if (pool_.size() >= kMaxPoolSize) {
            break;
        }

        // Create with an empty callback — the stack is allocated but
        // the fiber has never been run.
        auto fiber = std::make_shared<Fiber>(Fiber::Callback{}, stack_size);

        // If stack allocation failed, the fiber is in EXCEPT state.
        // Don't add broken fibers to the pool.
        if (fiber->getState() == Fiber::State::EXCEPT) {
            break;
        }

        fiber->state_ = Fiber::State::TERM;
        pool_.push_back(std::move(fiber));
    }
}

void FiberPool::reserve(size_t count) {
    ScopedSpinLock guard(lock_);
    pool_.reserve(count);
}

size_t FiberPool::available() const noexcept {
    ScopedSpinLock guard(lock_);
    return pool_.size();
}

size_t FiberPool::total_allocated() const noexcept {
    return total_allocated_;
}

size_t FiberPool::total_recycled() const noexcept {
    return total_recycled_;
}

void FiberPool::clear() {
    ScopedSpinLock guard(lock_);
    pool_.clear();
}

} // namespace zero
