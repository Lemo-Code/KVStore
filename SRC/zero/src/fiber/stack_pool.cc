/*
 * zero StackPool — fiber stack memory pool backed by mmap
 *
 * Each fiber stack is a contiguous virtual-memory region obtained via mmap.
 * A guard page (PROT_NONE) is placed at the low end to detect stack overflow
 * with a SIGSEGV rather than silent data corruption.
 *
 * Memory layout (stack grows downward from the returned pointer):
 *   ┌─────────────────┐  high address  ← stack_ (returned to caller)
 *   │                 │
 *   │   usable stack  │  size bytes rounded to page boundary
 *   │                 │  PROT_READ | PROT_WRITE
 *   ├─────────────────┤
 *   │   guard page    │  1 page, PROT_NONE
 *   └─────────────────┘  low address   ← mmap base
 *
 * Recycling policy:
 *   Deallocated stacks enter a free list for reuse.  When the free list
 *   exceeds kMaxFreeStacks (64) entries, the oldest is munmap'd to return
 *   virtual address space to the OS.  The free list uses best-fit search
 *   to minimize virtual-address-space fragmentation.
 *
 * Thread safety:
 *   All public methods are guarded by a SpinLock.  Contention is low
 *   because stack allocation/deallocation happens at fiber creation and
 *   destruction time, not in the hot path of scheduling.
 */

#include "zero/fiber/stack_pool.h"
#include "zero/thread/spinlock.h"

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace zero {

namespace {

// Cached system page size, initialized on first use.
inline size_t page_size() noexcept {
    static const size_t s_ps = []() {
        long sz = ::sysconf(_SC_PAGESIZE);
        return (sz > 0) ? static_cast<size_t>(sz) : 4096;
    }();
    return s_ps;
}

// Round up to the nearest page boundary.
inline size_t round_up_page(size_t size) noexcept {
    const size_t ps = page_size();
    return (size + ps - 1) & ~(ps - 1);
}

// Maximum free-list depth before we start unmapping excess entries.
constexpr size_t kMaxFreeStacks = 64;

} // anonymous namespace

/* =========================================================================
 * Singleton
 * ======================================================================== */

StackPool& StackPool::instance() {
    static StackPool pool;
    return pool;
}

/* =========================================================================
 * map_stack() — allocate a new stack region from the OS
 *
 * Allocates `size` bytes (already rounded to page size) plus one guard
 * page.  Returns a pointer to the TOP of the usable region.
 * ======================================================================== */

void* StackPool::map_stack(size_t size) {
    const size_t ps    = page_size();
    const size_t total = size + ps;  // usable + guard

    void* base = ::mmap(nullptr, total,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);
    if (base == MAP_FAILED) {
        return nullptr;
    }

    // Mark the lowest page as PROT_NONE (guard page)
    if (::mprotect(base, ps, PROT_NONE) != 0) {
        ::munmap(base, total);
        return nullptr;
    }

    // Return pointer to the top of the usable region
    return static_cast<char*>(base) + total;
}

/* =========================================================================
 * allocate() — obtain a stack of at least `size` bytes
 *
 * Tries the free list first using best-fit (smallest stack that satisfies
 * the request).  Falls back to mapping a new stack via mmap.
 * ======================================================================== */

void* StackPool::allocate(size_t size) {
    const size_t rounded = round_up_page(size);

    {
        ScopedSpinLock guard(lock_);

        // Best-fit search: find the smallest stack >= rounded
        ssize_t best_idx   = -1;
        size_t  best_delta = SIZE_MAX;

        for (size_t i = 0; i < free_stacks_.size(); ++i) {
            if (free_stacks_[i].second >= rounded) {
                size_t delta = free_stacks_[i].second - rounded;
                if (delta < best_delta) {
                    best_delta = delta;
                    best_idx   = static_cast<ssize_t>(i);
                    if (delta == 0) break;  // exact fit
                }
            }
        }

        if (best_idx >= 0) {
            size_t idx = static_cast<size_t>(best_idx);
            void*  stk = free_stacks_[idx].first;
            // Swap-and-pop for O(1) removal
            if (idx != free_stacks_.size() - 1) {
                free_stacks_[idx] = std::move(free_stacks_.back());
            }
            free_stacks_.pop_back();
            return stk;
        }
    }
    // Lock released — safe to mmap (which may take a while)

    void* stack = map_stack(rounded);
    if (stack) {
        ScopedSpinLock guard(lock_);
        total_allocated_ += rounded;
    }
    return stack;
}

/* =========================================================================
 * deallocate() — return a stack to the free list for reuse
 *
 * If the free list is at capacity (kMaxFreeStacks), the stack is unmapped
 * immediately to free virtual address space.
 * ======================================================================== */

void StackPool::deallocate(void* stack, size_t size) {
    if (!stack || size == 0) return;

    const size_t rounded = round_up_page(size);

    ScopedSpinLock guard(lock_);

    if (free_stacks_.size() >= kMaxFreeStacks) {
        // Too many cached stacks — release this one to the OS
        const size_t ps    = page_size();
        const size_t total = rounded + ps;
        void*        base  = static_cast<char*>(stack) - total;
        ::munmap(base, total);
        return;
    }

    free_stacks_.emplace_back(stack, rounded);
}

/* =========================================================================
 * preallocate() — populate the free list with N stacks
 *
 * Front-loads allocation costs to reduce latency jitter during operation.
 * Stops early if the free list reaches capacity or mmap fails.
 * ======================================================================== */

void StackPool::preallocate(size_t count, size_t stack_size) {
    if (count == 0) return;

    const size_t rounded = round_up_page(stack_size);

    ScopedSpinLock guard(lock_);

    for (size_t i = 0; i < count; ++i) {
        if (free_stacks_.size() >= kMaxFreeStacks) {
            break;
        }

        void* stack = map_stack(rounded);
        if (!stack) {
            break;  // out of virtual memory
        }

        total_allocated_ += rounded;
        free_stacks_.emplace_back(stack, rounded);
    }
}

StackPool::~StackPool() {
    trim();
}

void StackPool::trim() {
    ScopedSpinLock guard(lock_);
    const size_t ps = page_size();
    for (auto& [stack, size] : free_stacks_) {
        void* base = static_cast<char*>(stack) - (size + ps);
        ::munmap(base, size + ps);
    }
    free_stacks_.clear();
}

size_t StackPool::available() const noexcept {
    return free_stacks_.size();
}

size_t StackPool::total_allocated() const noexcept {
    return total_allocated_;
}

size_t StackPool::total_cached() const noexcept {
    size_t total = 0;
    for (const auto& [_, sz] : free_stacks_) total += sz;
    return total;
}

} // namespace zero
