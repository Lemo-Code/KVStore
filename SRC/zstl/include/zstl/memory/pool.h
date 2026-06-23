// zstl MultiSizeClassPool — lock-free memory pool with per-thread cache
//
// Architecture (fast path / slow path split):
//
// ┌─────────────────────────────────────────────────────┐
// │ FAST PATH (inline, zero atomics):                   │
// │   pool_malloc(n):                                   │
// │     1. Check n > kMaxPoolSize → fallback to ::new   │
// │     2. Compute size class idx                       │
// │     3. Pop from thread-local tcache[idx] (lock-free)│
// │     4. Return popped block                          │
// │                                                     │
// │   pool_free(ptr, n):                                │
// │     1. Check n > kMaxPoolSize → fallback to ::delete│
// │     2. Compute size class idx                       │
// │     3. Push to thread-local tcache[idx]             │
// │     4. If tcache full → slow path                   │
// └─────────────────────────────────────────────────────┘
//                        │ (cache miss)
//                        ▼
// ┌─────────────────────────────────────────────────────┐
// │ SLOW PATH (out-of-line in pool.cc):                 │
// │   allocateSlow(idx):                                │
// │     1. Try CAS pop from global freelist             │
// │     2. If empty → mmap new chunk, carve into blocks │
// │     3. Push batch to tcache, return one block       │
// │                                                     │
// │   deallocateSlow(ptr, idx):                         │
// │     1. Flush half tcache to global freelist         │
// │     2. Add freed block to tcache                    │
// └─────────────────────────────────────────────────────┘
//
// Key design decisions:
// - Thread-local caches eliminate contention for common ops
// - Global freelist uses CAS (lock-free) for thread safety
// - Batch refill/flush amortizes global freelist access cost
// - Page-aligned chunks from mmap for cache-line friendliness
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <new>
#include "zstl/memory/type_traits.h"

namespace zstl {

// ============================================================
// pool_stats — monitoring and diagnostics
// ============================================================

struct pool_stats {
    size_t total_allocated_bytes = 0;   // Total bytes requested from OS
    size_t total_in_use_bytes    = 0;   // Estimated bytes outstanding in app
    size_t global_free_count     = 0;   // Blocks in global freelists
    size_t tcache_free_count     = 0;   // Blocks cached in thread-local tcaches
    size_t chunk_count           = 0;   // Number of OS chunks currently held
    size_t trim_candidates       = 0;   // Chunks eligible for munmap
    size_t fallback_allocations  = 0;   // Allocations exceeding kMaxPoolSize

    // Per-size-class breakdown
    struct per_class {
        size_t block_size;
        size_t global_free;
        size_t capacity;
    };
    per_class classes[kNumSizeClasses];
};

// ============================================================
// Lock-free freelist node (intrusive linked list)
// ============================================================

struct FreeNode {
    FreeNode* next;
};

// ============================================================
// Chunk header — tracks metadata for each mmap'd chunk
// ============================================================

struct ChunkHeader {
    size_t block_size;       // Size of each block in this chunk
    size_t num_blocks;       // Total blocks in chunk
    size_t num_free;         // Approximate free count (for trim decisions)
    ChunkHeader* next;       // Intrusive linked list of chunks

    // Returns pointer to the first block (immediately after header)
    void* data() noexcept {
        return reinterpret_cast<char*>(this) + sizeof(ChunkHeader);
    }
};

// ============================================================
// Global freelist per size class — CAS-based lock-free
// singly-linked list with batched operations
// ============================================================

class LockFreeFreelist {
public:
    LockFreeFreelist() : head_(nullptr), count_(0) {}

    // Push a single node (thread-safe, lock-free)
    void push(FreeNode* node) noexcept {
        FreeNode* old_head = head_.load(std::memory_order_relaxed);
        do {
            node->next = old_head;
        } while (!head_.compare_exchange_weak(old_head, node,
                 std::memory_order_release, std::memory_order_relaxed));
        count_.fetch_add(1, std::memory_order_relaxed);
    }

    // Pop a single node (thread-safe, lock-free).
    // Returns nullptr if empty.
    FreeNode* pop() noexcept {
        FreeNode* old_head = head_.load(std::memory_order_acquire);
        while (old_head) {
            if (head_.compare_exchange_weak(old_head, old_head->next,
                    std::memory_order_release, std::memory_order_acquire)) {
                count_.fetch_sub(1, std::memory_order_relaxed);
                return old_head;
            }
        }
        return nullptr;
    }

    // Batch pop: remove up to n nodes from the freelist.
    // Returns the head of a null-terminated list of popped nodes.
    // The caller receives a linked list of up to n nodes.
    FreeNode* popBatch(size_t n) noexcept {
        if (n == 0) return nullptr;

        FreeNode* old_head = head_.load(std::memory_order_acquire);
        while (old_head) {
            // Walk the list to find the nth node
            FreeNode* cur = old_head;
            size_t found = 1;
            while (found < n && cur->next) {
                cur = cur->next;
                ++found;
            }
            FreeNode* new_head = cur->next;
            cur->next = nullptr;  // Terminate the popped list

            if (head_.compare_exchange_weak(old_head, new_head,
                    std::memory_order_release, std::memory_order_acquire)) {
                count_.fetch_sub(found, std::memory_order_relaxed);
                return old_head;
            }
            // CAS failed: old_head is now reloaded; cur->next was clobbered;
            // need to fix it up. The popped list in cur is invalid now.
            // For correctness, we must retry from scratch.
            // The cur->next modification is harmless since we retry.
        }
        return nullptr;
    }

    // Push a null-terminated list of nodes (thread-safe).
    // The caller provides both head and tail for O(1) insertion.
    void pushList(FreeNode* list_head, FreeNode* list_tail,
                  size_t list_count) noexcept
    {
        if (!list_head) return;
        FreeNode* old_head = head_.load(std::memory_order_relaxed);
        do {
            list_tail->next = old_head;
        } while (!head_.compare_exchange_weak(old_head, list_head,
                 std::memory_order_release, std::memory_order_relaxed));
        count_.fetch_add(list_count, std::memory_order_relaxed);
    }

    // Remove all nodes (for trim operation).
    // Returns the head of the entire list, or nullptr if empty.
    FreeNode* popAll() noexcept {
        FreeNode* old_head = head_.exchange(nullptr, std::memory_order_acquire);
        if (old_head) {
            count_.store(0, std::memory_order_relaxed);
        }
        return old_head;
    }

    // Check if empty (approximate — may be stale immediately)
    bool empty() const noexcept {
        return head_.load(std::memory_order_relaxed) == nullptr;
    }

    // Approximate count (relaxed atomic — may be stale)
    size_t count() const noexcept {
        return count_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<FreeNode*> head_;
    std::atomic<size_t>    count_;
};

// ============================================================
// Per-thread cache bin for one size class
// ============================================================

struct TCacheBin {
    FreeNode* head  = nullptr;
    size_t    count = 0;
    size_t    low_watermark = 0;  // Refill when count drops below this
};

// ============================================================
// MultiSizeClassPool — the singleton memory pool
// ============================================================


class MultiSizeClassPool {
public:
    static MultiSizeClassPool& instance() noexcept;

    // --- Slow-path allocation (called when tcache is exhausted) ---
    void* allocateSlow(size_t idx);

    // --- Slow-path deallocation (called when tcache is full) ---
    void deallocateSlow(void* ptr, size_t idx);

    // --- Batch operations for tcache management ---
    void refillBatch(TCacheBin& bin, size_t idx);
    void flushBatch(TCacheBin& bin, size_t idx);

    // --- Pool maintenance ---

    // Return fully-free chunks to the OS (munmap).
    // Returns number of bytes returned.
    size_t pool_trim();

    // Aggregate statistics across all size classes.
    void pool_stats(pool_stats& out) const;

    // Total bytes allocated from OS (approximate, relaxed atomic)
    size_t total_allocated() const noexcept {
        return total_allocated_.load(std::memory_order_relaxed);
    }

    // Get global freelist for a size class (for testing/trim)
    LockFreeFreelist& global_freelist(size_t idx) noexcept {
        return global_freelists_[idx];
    }

private:
    MultiSizeClassPool();
    ~MultiSizeClassPool();
    MultiSizeClassPool(const MultiSizeClassPool&) = delete;
    MultiSizeClassPool& operator=(const MultiSizeClassPool&) = delete;

    // Allocate a new chunk from the OS.
    // Returns pointer to the first block, with the chunk tracked internally.
    void* allocateChunk(size_t block_size, size_t num_blocks);

    LockFreeFreelist global_freelists_[kNumSizeClasses];

    // Chunk tracking (intrusive linked list for trim support)
    ChunkHeader* chunk_list_ = nullptr;
    std::atomic<size_t> chunk_count_{0};

    std::atomic<size_t> total_allocated_{0};
    std::atomic<size_t> fallback_count_{0};
};

// ============================================================
// Thread-local tcache — one bin per size class per thread
// ============================================================

extern thread_local TCacheBin t_tcache[kNumSizeClasses];

// ============================================================
// FAST PATH: pool_malloc (inline, zero atomics for common case)
// ============================================================

inline void* pool_malloc(size_t n) {
    // Oversized allocations bypass the pool entirely
    if (__builtin_expect(n > kMaxPoolSize, 0)) {
        return ::operator new(n);
    }

    size_t idx = size_class_index(n);
    TCacheBin& bin = t_tcache[idx];

    // Happy path: tcache has available blocks
    if (__builtin_expect(bin.head != nullptr, 1)) {
        FreeNode* node = bin.head;
        bin.head = node->next;
        --bin.count;
        return static_cast<void*>(node);
    }

    // Cache miss: fall through to slow path
    return MultiSizeClassPool::instance().allocateSlow(idx);
}

// ============================================================
// FAST PATH: pool_free (inline, zero atomics for common case)
// ============================================================

inline void pool_free(void* ptr, size_t n) {
    if (!ptr) return;

    if (__builtin_expect(n > kMaxPoolSize, 0)) {
        ::operator delete(ptr);
        return;
    }

    size_t idx = size_class_index(n);
    TCacheBin& bin = t_tcache[idx];

    // Happy path: tcache has room
    if (__builtin_expect(bin.count < tcache_capacity(idx), 1)) {
        FreeNode* node = static_cast<FreeNode*>(ptr);
        node->next = bin.head;
        bin.head = node;
        ++bin.count;
        return;
    }

    // Cache full: fall through to slow path (flush + add)
    MultiSizeClassPool::instance().deallocateSlow(ptr, idx);
}

// ============================================================
// EVEN FASTER: pool_malloc / pool_free for known size class
// (used when the size class is already known, e.g., from an
// allocator that remembers the size class)
// ============================================================

inline void* pool_malloc_class(size_t idx) {
    TCacheBin& bin = t_tcache[idx];
    if (__builtin_expect(bin.head != nullptr, 1)) {
        FreeNode* node = bin.head;
        bin.head = node->next;
        --bin.count;
        return static_cast<void*>(node);
    }
    return MultiSizeClassPool::instance().allocateSlow(idx);
}

inline void pool_free_class(void* ptr, size_t idx) {
    if (!ptr) return;
    TCacheBin& bin = t_tcache[idx];
    if (__builtin_expect(bin.count < tcache_capacity(idx), 1)) {
        FreeNode* node = static_cast<FreeNode*>(ptr);
        node->next = bin.head;
        bin.head = node;
        ++bin.count;
        return;
    }
    MultiSizeClassPool::instance().deallocateSlow(ptr, idx);
}

// ============================================================
// Realloc support: pool_realloc
// ============================================================

inline void* pool_realloc(void* ptr, size_t old_size, size_t new_size) {
    // If both sizes fit in the same size class, no copy needed
    if (old_size <= kMaxPoolSize && new_size <= kMaxPoolSize &&
        size_class_index(old_size) == size_class_index(new_size))
    {
        return ptr;
    }

    void* new_ptr = pool_malloc(new_size);
    if (ptr && new_ptr) {
        size_t copy_size = old_size < new_size ? old_size : new_size;
        __builtin_memcpy(new_ptr, ptr, copy_size);
    }
    pool_free(ptr, old_size);
    return new_ptr;
}

} // namespace zstl
