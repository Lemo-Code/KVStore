// zero WorkStealingQueue implementation — Chase-Lev lock-free work-stealing deque
//
// Based on the classic Chase-Lev dynamic circular work-stealing deque:
//   "Dynamic Circular Work-Stealing Deque" by Chase & Lev, SPAA 2005.
//   Also described in: https://www.di.ens.fr/~zappa/readings/ppopp13.pdf
//
// Design overview:
//
//   The deque is a bounded circular buffer with two indices:
//     - bottom_: owned exclusively by the owner thread. The owner pushes
//       and pops at the bottom end (LIFO ordering for cache-locality).
//     - top_: shared between the owner (pop) and thieves (steal). Thieves
//       steal from the top end (FIFO ordering for fairness and to reduce
//       contention — the owner and thieves operate on opposite ends).
//
//   The key invariant: the buffer is non-empty when bottom_ > top_.
//   The owner decrements bottom_ BEFORE reading the element (optimistic),
//   then uses a CAS on top_ to resolve races on the last element.
//
// Memory ordering rationale:
//
//   This is one of the most subtle aspects of lock-free programming.
//   The following ordering constraints are essential for correctness:
//
//   1. push(): After storing the fiber in buffer_[b & mask_], we need a
//      release fence before updating bottom_. This ensures that the stored
//      fiber is visible to any thread that subsequently reads bottom_ and
//      then reads from the buffer. Without this, a thief could see the
//      updated bottom_ but a stale (null) buffer entry.
//
//   2. pop(): After decrementing bottom_, we need a seq_cst fence before
//      reading top_. This is the critical synchronization point with
//      steal(). Both pop() and steal() can attempt to claim the last
//      element, and seq_cst ensures a total order among all such attempts.
//      The CAS on top_ then resolves the race: exactly one of pop() or
//      steal() succeeds, and the other sees the CAS fail and backs off.
//
//   3. steal(): Uses acquire on top_ (to see the owner's writes), then a
//      seq_cst fence (to establish total order with pop()'s CAS), then
//      acquire on bottom_ (to see the owner's pushes). If t < b, the
//      thief attempts a CAS on top_ to claim the element. If the CAS
//      succeeds, the thief owns the element; if it fails, another thief
//      or the owner claimed it first.
//
//   Note: The seq_cst fences are more expensive than acquire/release, but
//   they are necessary for correctness of the CAS race between pop() and
//   steal(). Replacing seq_cst with acquire/release would introduce a
//   subtle ABA-like problem where both pop() and steal() could "succeed"
//   on the same element.
//
// Cache-line alignment:
//
//   bottom_ and top_ are placed on separate 64-byte cache lines to prevent
//   false sharing. When the owner writes bottom_ (every push/pop), and a
//   thief reads top_ (every steal), they would otherwise invalidate each
//   other's cache lines, causing a significant performance penalty on
//   multi-socket systems. The alignas(kCacheLineSize) ensures each atomic
//   variable starts at the beginning of its own cache line.

#include "zero/scheduler/work_stealing_queue.h"
#include "zero/base/macro.h"

#include <algorithm>
#include <cmath>

namespace zero {

// ============================================================
// Construction
// ============================================================

WorkStealingQueue::WorkStealingQueue(size_t capacity) {
    // The capacity must be a power of two so that (index & mask_) maps
    // an index into the circular buffer without expensive modulo
    // operations. We round up to the next power of two.
    //
    // Upper bound: we cap at 2^30 (about 1 billion entries) to avoid
    // overflow in size calculations and to keep buffer allocation
    // within reasonable memory bounds.
    if (capacity == 0) {
        capacity = 1;
    }

    capacity_ = 1;
    while (capacity_ < capacity && capacity_ < (static_cast<size_t>(1) << 30)) {
        capacity_ <<= 1;
    }

    mask_ = capacity_ - 1;

    // Allocate the circular buffer. Using new[] instead of std::vector
    // because we need precise control over the memory layout (the
    // buffer pointer is used directly in size and empty calculations).
    buffer_ = new Fiber::Ptr[capacity_];

    // Both indices start at 0. The deque is initially empty.
    bottom_.store(0, std::memory_order_relaxed);
    top_.store(0, std::memory_order_relaxed);
}

// ============================================================
// Destruction
// ============================================================

WorkStealingQueue::~WorkStealingQueue() {
    // Release all remaining Fiber::Ptr references in the buffer.
    // This is safe because at destruction time no other thread should
    // be accessing the queue. The owner thread must have stopped and
    // no thieves can be running.
    size_t b = bottom_.load(std::memory_order_relaxed);
    size_t t = top_.load(std::memory_order_relaxed);

    for (size_t i = t; i < b; ++i) {
        buffer_[i & mask_].reset();
    }

    delete[] buffer_;
    buffer_ = nullptr;
}

// ============================================================
// push() — owner adds element at the bottom (LIFO end)
// ============================================================

void WorkStealingQueue::push(Fiber::Ptr fiber) {
    if (!fiber) {
        return;  // Silently ignore null fibers
    }

    // Read bottom_ with relaxed ordering — the owner is the only writer.
    size_t b = bottom_.load(std::memory_order_relaxed);

    // Read top_ with acquire ordering to see any steals that may have
    // occurred since our last push. acquire ensures we see the most
    // recent value of top_ written by a thief's CAS.
    size_t t = top_.load(std::memory_order_acquire);

    // Full queue check: (b - t) is the number of elements in the buffer.
    // When b - t >= capacity_, every slot is occupied.
    if (b - t >= capacity_) {
        // Queue is full. In a production system, we could grow the buffer
        // by allocating a larger one and copying elements. However, this
        // is complex under concurrent access (thieves may be reading).
        // For now, we drop the fiber — with a 1024-entry queue per thread
        // and work-stealing, this should be exceedingly rare.
        return;
    }

    // Store the fiber at position (b & mask_). This is the bottom-1
    // element after the write to bottom_ completes.
    buffer_[b & mask_] = std::move(fiber);

    // Release fence: publishes the stored fiber to any thread that
    // subsequently reads bottom_ with acquire ordering. This ensures
    // that a thief seeing the updated bottom_ will also see the stored
    // fiber in buffer_.
    //
    // Without this fence, the CPU or compiler could reorder the buffer
    // store after the bottom_ store, causing a thief to read a null
    // pointer from the buffer.
    std::atomic_thread_fence(std::memory_order_release);

    // Update bottom_ with release ordering. The release ensures that
    // the fence and all prior stores (including the buffer store) are
    // visible to threads that acquire-read bottom_.
    bottom_.store(b + 1, std::memory_order_release);
}

// ============================================================
// pop() — owner removes element from the bottom (LIFO end)
// ============================================================

Fiber::Ptr WorkStealingQueue::pop() {
    // Step 1: Decrement bottom_ optimistically.
    // We decrement before checking if the queue is non-empty. If we
    // later find the queue is empty, we restore bottom_.
    size_t b = bottom_.load(std::memory_order_relaxed);
    if (b == 0) {
        return nullptr;  // Queue is definitely empty
    }

    b = b - 1;
    bottom_.store(b, std::memory_order_relaxed);

    // Step 2: Full sequential-consistency fence.
    // This is the critical synchronization point between pop() and
    // steal(). Without seq_cst, the following scenario is possible:
    //   1. Owner decrements bottom_ (now b == t, the last element)
    //   2. Owner reads top_ and sees t
    //   3. Owner reads buffer_[b & mask_] — gets the fiber
    //   4. Thief reads top_ and sees t
    //   5. Thief reads bottom_ and sees b (t < b from thief's view)
    //   6. Thief CAS top_ from t to t+1 — succeeds
    //   7. Owner CAS top_ from t to t+1 — ALSO succeeds because top_
    //      was t in both cases (the infamous ABA problem variant)
    //      Result: both owner and thief think they own the element.
    //
    // The seq_cst fence prevents this by establishing a total order:
    // either pop()'s CAS happens-before steal()'s CAS, or vice versa.
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // Step 3: Read top_ to check if the queue is non-empty.
    size_t t = top_.load(std::memory_order_relaxed);

    Fiber::Ptr fiber;

    if (t < b) {
        // Queue has at least 2 elements — no race possible with thieves
        // because thieves operate at index t, and we're at index b > t.
        fiber = std::move(buffer_[b & mask_]);
        // No CAS needed — we own this element exclusively.
    } else if (t == b) {
        // Queue has exactly 1 element (the one we just claimed).
        // We must use CAS to resolve the race with potential thieves.
        fiber = std::move(buffer_[b & mask_]);

        // Attempt to advance top_. If this CAS succeeds, we claimed the
        // last element before any thief could. If it fails, a thief got
        // there first, so we discard the fiber (the thief owns it now).
        if (!top_.compare_exchange_strong(
                t, t + 1,
                std::memory_order_release,
                std::memory_order_relaxed)) {
            // Lost the race — thief claimed the element.
            fiber.reset();
        }

        // Regardless of CAS outcome, advance bottom_ to match top_.
        // After the CAS: top_ == t+1 (either we set it or thief did),
        // so bottom_ should also be t+1 for the queue to be consistent.
        bottom_.store(t + 1, std::memory_order_release);
    } else {
        // t > b: Queue is actually empty (thief stole the last element
        // between our decrement of bottom_ and our read of top_).
        // Restore bottom_ to top_.
        bottom_.store(t, std::memory_order_release);
    }

    return fiber;
}

// ============================================================
// steal() — thief removes element from the top (FIFO end)
// ============================================================

Fiber::Ptr WorkStealingQueue::steal() {
    // Step 1: Read top_ with acquire ordering.
    // acquire ensures we see all stores that happened before the
    // release-store that published the current value of top_.
    size_t t = top_.load(std::memory_order_acquire);

    // Step 2: Full seq_cst fence to establish total order with pop().
    // See pop() for the detailed rationale — this prevents the scenario
    // where both pop() and steal() claim the same last element.
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // Step 3: Read bottom_ with acquire ordering.
    // acquire ensures we see all fiber stores that the owner published
    // before the release-store to bottom_.
    size_t b = bottom_.load(std::memory_order_acquire);

    // Step 4: Check if the queue has elements from our perspective.
    if (t < b) {
        // Non-empty — attempt to steal the element at index t.
        Fiber::Ptr fiber = std::move(buffer_[t & mask_]);

        // CAS to claim the element. If this succeeds, we have exclusive
        // ownership. If it fails, another thief (or the owner via pop())
        // claimed it first, and we return nullptr.
        //
        // We use release on success to publish our steal to other threads.
        // We use relaxed on failure because we don't publish anything.
        if (!top_.compare_exchange_strong(
                t, t + 1,
                std::memory_order_release,
                std::memory_order_relaxed)) {
            // Lost the race — another thief or the owner got it.
            return nullptr;
        }

        return fiber;
    }

    // Queue empty from our perspective.
    return nullptr;
}

// ============================================================
// size() — approximate number of elements in the queue
// ============================================================


// ============================================================
// empty() — convenience wrapper around size()
// ============================================================


} // namespace zero
