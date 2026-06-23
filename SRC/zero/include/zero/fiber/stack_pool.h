// zero StackPool — fiber stack memory pool
//
// Each fiber needs its own stack (typically 128 KB). The stack must be
// page-aligned and have a guard page at the low end to detect stack
// overflow (SIGSEGV triggers debug info).
//
// StackPool allocates stacks via mmap(MAP_ANONYMOUS | MAP_PRIVATE) with
// PROT_NONE on the first page (guard page). Freed stacks are returned
// to a free list for reuse rather than munmap'd, avoiding kernel overhead.
// Stacks are never returned to the OS unless the pool is explicitly
// trimmed or destroyed.
//
// Thread-safe via internal spinlock.
#pragma once

#include <cstddef>
#include <vector>
#include <utility>

#include "zero/thread/spinlock.h"

namespace zero {

class StackPool {
public:
    // Singleton accessor
    static StackPool& instance();

    // Allocate a stack region of the given size.
    // The returned pointer points to the HIGH end of the usable stack
    // (stack grows downward from this address).
    // The size is rounded up to a multiple of the system page size.
    void* allocate(size_t size);

    // Return a stack region to the pool for reuse.
    // The pointer must have been obtained from allocate() with the same
    // size (rounded up).
    void deallocate(void* stack, size_t size);

    // Pre-allocate N stacks to warm the pool.
    void preallocate(size_t count, size_t stack_size = 131072);

    // Free all cached stacks back to the OS.
    // Thread-safe, but should not be called while fibers are active.
    void trim();

    // Configure the guard page size (default 1 page = 4096 bytes).
    // Zero means no guard page.
    void set_guard_pages(size_t pages) noexcept { guard_pages_ = pages; }
    size_t guard_pages() const noexcept { return guard_pages_; }

    // Statistics
    size_t available() const noexcept;
    size_t total_allocated() const noexcept;   // Currently live mmap'd bytes
    size_t total_cached() const noexcept;       // Bytes in free list

private:
    StackPool() = default;
    ~StackPool();

    // Map a new stack from the OS. Allocates `size` + guard page.
    // Returns pointer to the top (high address) of the usable region.
    static void* map_stack(size_t size);

    mutable SpinLock lock_;
    std::vector<std::pair<void*, size_t>> free_stacks_;  // {top_ptr, size}
    size_t total_allocated_ = 0;   // Total mmap'd bytes
    size_t guard_pages_ = 1;       // 1 guard page by default
};

} // namespace zero
