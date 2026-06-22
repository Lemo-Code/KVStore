/**
 * @file    pool.h
 * @brief   Multi-size-class memory pool implementation.
 * @author  lstl team
 * @date    2025
 *
 * Provides a high-performance memory pool inspired by jemalloc design
 * principles. The pool uses size classes to group allocation requests
 * into buckets, each with its own freelist for O(1) allocate/deallocate.
 *
 * Key features:
 * - 28 size classes covering 8 to 7168 bytes.
 * - Per-size-class freelist for lock-free allocation in the common case.
 * - Large allocations (> kMaxPoolSize = 8192) bypass the pool entirely.
 * - Thread-safe via a single global pool_impl singleton.
 * - Single-threaded pool_single variant for fiber-local or
 *   single-threaded contexts with zero locking overhead.
 *
 * Architecture:
 * @code
 *   default_alloc (public API)
 *     └── pool_impl::instance() (singleton)
 *           ├── freelist[0..27] — per-size-class free lists
 *           ├── size_class_index(n) — maps size to bucket
 *           └── malloc_alloc — fallback for large allocations
 * @endcode
 *
 * @ingroup memory
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <atomic>
#include <mutex>

#include "alloc.h"
#include "utility.h"

namespace lstl {

// =========================================================================
// Size class configuration
// =========================================================================

namespace detail {

/// Number of size classes in the pool.
static const size_t kNumSizeClasses = 28;

/**
 * @brief  Pre-defined size class table.
 *
 * Each entry is the actual block size served by that class.
 * The table is designed so that:
 * - Small sizes: 8, 16, 32, 48, ..., 256
 * - Medium sizes: 320, 384, ..., 3584
 * - Large sizes: 4096, 5120, 6144, 7168
 */
static const size_t kSizeClassTable[kNumSizeClasses] = {
    8, 16, 32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256,
    320, 384, 448, 512, 768, 1024, 1536, 2048, 2560, 3072, 3584,
    4096, 5120, 6144, 7168
};

/// Maximum size served by the pool. Allocations larger than this go to malloc_alloc directly.
static const size_t kMaxPoolSize = 8192;

/**
 * @brief  Returns the number of blocks to allocate per chunk for a given size class.
 *
 * Smaller size classes get more blocks per chunk to reduce allocation overhead.
 *
 * @param  idx  Size class index.
 * @return      Number of blocks per chunk.
 */
inline size_t chunk_count(size_t idx) {
    if (idx < 4)  return 64;   // 8-48 bytes
    if (idx < 13) return 32;   // 64-256 bytes
    if (idx < 24) return 16;   // 320-3584
    return 4;                   // 4096-7168
}

/**
 * @brief  Maps an allocation size to the appropriate size class index.
 *
 * Rounds @p n up to the nearest size class. For sizes beyond the
 * largest class, returns kNumSizeClasses (indicating direct malloc).
 *
 * @param  n   Requested allocation size in bytes.
 * @return     Size class index, or kNumSizeClasses for oversized requests.
 *
 * @note  This is a simple if-else chain that compiles to an efficient
 *        jump table on modern compilers.
 */
inline size_t size_class_index(size_t n) {
    if (n <= 8)    return 0;
    if (n <= 16)   return 1;
    if (n <= 32)   return 2;
    if (n <= 48)   return 3;
    if (n <= 64)   return 4;
    if (n <= 80)   return 5;
    if (n <= 96)   return 6;
    if (n <= 112)  return 7;
    if (n <= 128)  return 8;
    if (n <= 160)  return 9;
    if (n <= 192)  return 10;
    if (n <= 224)  return 11;
    if (n <= 256)  return 12;
    if (n <= 320)  return 13;
    if (n <= 384)  return 14;
    if (n <= 448)  return 15;
    if (n <= 512)  return 16;
    if (n <= 768)  return 17;
    if (n <= 1024) return 18;
    if (n <= 1536) return 19;
    if (n <= 2048) return 20;
    if (n <= 2560) return 21;
    if (n <= 3072) return 22;
    if (n <= 3584) return 23;
    if (n <= 4096) return 24;
    if (n <= 5120) return 25;
    if (n <= 6144) return 26;
    if (n <= 7168) return 27;
    return kNumSizeClasses;
}

/**
 * @brief  Rounds up a size to the nearest size class value.
 *
 * @param  n  Requested size.
 * @return    The actual block size that will be allocated,
 *            or n if n exceeds kMaxPoolSize.
 */
inline size_t round_up_size(size_t n) {
    size_t idx = size_class_index(n);
    if (idx >= kNumSizeClasses) return n;
    return kSizeClassTable[idx];
}

} // namespace detail

// =========================================================================
// freelist — Intrusive singly-linked freelist
// =========================================================================

/**
 * @brief  Intrusive freelist for O(1) block management.
 *
 * Each free block stores a pointer to the next free block at
 * offset 0. Both push and pop are O(1) and lock-free-safe
 * (though external synchronization is still required for the
 * global pool).
 *
 * Memory layout of a free block:
 * @code
 *   [next pointer (sizeof(void*))][... unused space ...]
 * @endcode
 */
class freelist {
public:
    freelist() : head_(nullptr) {}

    /**
     * @brief  Pushes a block onto the free list.
     *
     * @param  p  Pointer to the block. The first sizeof(void*) bytes
     *            will be overwritten with the next pointer.
     *
     * @post  p is the new head of the freelist.
     */
    void push(void* p) {
        *static_cast<void**>(p) = head_;
        head_ = p;
    }

    /**
     * @brief  Pops a block from the free list.
     *
     * @return  Pointer to a free block, or nullptr if the list is empty.
     *
     * @post  If non-null, the returned block is removed from the list.
     */
    void* pop() {
        if (head_ == nullptr) return nullptr;
        void* result = head_;
        head_ = *static_cast<void**>(head_);
        return result;
    }

    /**
     * @brief  Checks if the freelist is empty.
     * @return true if no free blocks are available.
     */
    bool empty() const { return head_ == nullptr; }

    /**
     * @brief  Bulk-pushes a range of equally-spaced blocks.
     *
     * Blocks are spaced @p block_size bytes apart in memory
     * (carved from a contiguous chunk).
     *
     * @param  start       Pointer to the first block in the chunk.
     * @param  end         Pointer past the last block in the chunk.
     * @param  block_size  Distance between consecutive blocks.
     */
    void push_range(char* start, char* end, size_t block_size) {
        char* cur = start;
        while (cur + block_size <= end) {
            push(cur);
            cur += block_size;
        }
    }

private:
    void* head_;  ///< Head of the singly-linked free list.
};

// =========================================================================
// pool_impl — Thread-safe global memory pool singleton
// =========================================================================

/**
 * @brief  Thread-safe, global memory pool.
 *
 * Uses a Meyer's singleton pattern with an array of freelists,
 * one per size class. Large allocations bypass the pool.
 *
 * @note  This class is not meant to be used directly. Use
 *        default_alloc or simple_alloc<..., default_alloc> instead.
 */
class pool_impl {
public:
    /**
     * @brief  Returns the global pool singleton.
     *
     * Thread-safe in C++11 and later (guaranteed by the standard).
     *
     * @return  Reference to the singleton pool_impl instance.
     */
    static pool_impl& instance() {
        static pool_impl p;
        return p;
    }

    /**
     * @brief  Allocates n bytes from the pool.
     *
     * For sizes ≤ kMaxPoolSize: pops from the appropriate freelist.
     * If the freelist is empty, refills by allocating a new chunk.
     * For larger sizes: delegates to malloc_alloc.
     *
     * @param  n  Requested size in bytes.
     * @return    Pointer to at least n bytes of memory.
     *
     * @throws std::bad_alloc  If a new chunk cannot be allocated.
     */
    void* allocate(size_t n) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (n > detail::kMaxPoolSize) {
            return malloc_alloc::allocate(n);
        }

        size_t idx = detail::size_class_index(n);
        if (idx >= detail::kNumSizeClasses) {
            return malloc_alloc::allocate(n);
        }

        void* p = freelists_[idx].pop();
        if (p != nullptr) {
            return p;
        }

        return refill(idx);
    }

    /**
     * @brief  Returns memory to the pool.
     *
     * Pushes the block onto the appropriate freelist for later reuse.
     * Large blocks are freed directly via malloc_alloc.
     *
     * @param  p  Pointer to deallocate.
     * @param  n  Size of the original allocation (used to find the
     *            correct size class).
     *
     * @pre    @p p was previously returned by pool_impl::allocate().
     * @pre    @p n matches the size passed to allocate().
     */
    void deallocate(void* p, size_t n) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (p == nullptr) return;

        if (n > detail::kMaxPoolSize) {
            malloc_alloc::deallocate(p, n);
            return;
        }

        size_t idx = detail::size_class_index(n);
        if (idx >= detail::kNumSizeClasses) {
            malloc_alloc::deallocate(p, n);
            return;
        }

        freelists_[idx].push(p);
    }

    /**
     * @brief  Reallocates memory, preserving contents.
     *
     * If the new size fits in the same size class, returns p unchanged.
     * Otherwise allocates new memory, copies min(old_size, new_size)
     * bytes, and frees the old block.
     *
     * @param  p         Existing allocation (may be nullptr).
     * @param  old_size  Original allocation size.
     * @param  new_size  Desired new size.
     * @return           Pointer to the reallocated memory.
     */
    void* reallocate(void* p, size_t old_size, size_t new_size) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (p == nullptr) return allocate(new_size);

        size_t old_idx = detail::size_class_index(old_size);
        size_t new_idx = detail::size_class_index(new_size);

        if (old_idx == new_idx && new_idx < detail::kNumSizeClasses) {
            return p;
        }

        void* new_p = allocate(new_size);
        if (new_p) {
            size_t copy_size = min(old_size, new_size);
            std::memcpy(new_p, p, copy_size);
        }
        deallocate(p, old_size);
        return new_p;
    }

    /**
     * @brief  Returns total bytes allocated from the OS (for debugging).
     * @return  Approximate total bytes allocated across all chunks.
     */
    size_t allocated_bytes() const {
        return bytes_allocated_.load(std::memory_order_relaxed);
    }

private:
    pool_impl() : bytes_allocated_(0) {
        for (size_t i = 0; i < detail::kNumSizeClasses; ++i) {
            freelists_[i] = freelist();
        }
    }

    ~pool_impl() {
        // Memory is intentionally leaked on shutdown to avoid
        // static destruction order fiasco with other statics
        // that may still hold pool-allocated memory.
    }

    // Non-copyable, non-movable
    pool_impl(const pool_impl&) = delete;
    pool_impl& operator=(const pool_impl&) = delete;

    /**
     * @brief  Allocates a new chunk for the given size class.
     *
     * Carves the chunk into block_size blocks, pushes all but the
     * first onto the freelist, and returns the first block to the caller.
     *
     * @param  idx  Size class index to refill.
     * @return      Pointer to a single block from the new chunk.
     */
    void* refill(size_t idx) {
        size_t block_size = detail::kSizeClassTable[idx];
        size_t count = detail::chunk_count(idx);
        size_t total_bytes = block_size * count;

        char* chunk = static_cast<char*>(malloc_alloc::allocate(total_bytes));

        bytes_allocated_.fetch_add(total_bytes, std::memory_order_relaxed);

        // First block returned to caller, rest added to freelist
        char* first = chunk;
        for (size_t i = 1; i < count; ++i) {
            freelists_[idx].push(chunk + i * block_size);
        }

        return first;
    }

    freelist freelists_[detail::kNumSizeClasses];  ///< Per-size-class free lists.
    std::atomic<size_t> bytes_allocated_;           ///< Total bytes allocated (debug).
    std::recursive_mutex mutex_;                      ///< Recursive mutex for thread safety.
};

// =========================================================================
// default_alloc — Public pool-based allocator
// =========================================================================

/**
 * @brief  The default pool-based allocator.
 *
 * All methods delegate to the global pool_impl singleton. This is the
 * recommended allocator for most lstl containers.
 *
 * Usage:
 * @code
 * void* p = default_alloc::allocate(128);
 * default_alloc::deallocate(p, 128);
 * @endcode
 *
 * Or via simple_alloc:
 * @code
 * typedef simple_alloc<MyClass, default_alloc> alloc;
 * MyClass* obj = alloc::allocate(1);
 * alloc::deallocate(obj, 1);
 * @endcode
 */
class default_alloc {
public:
    /// @copydoc pool_impl::allocate
    static void* allocate(size_t n) {
        return pool_impl::instance().allocate(n);
    }

    /// @copydoc pool_impl::deallocate
    static void deallocate(void* p, size_t n) {
        pool_impl::instance().deallocate(p, n);
    }

    /// @copydoc pool_impl::reallocate
    static void* reallocate(void* p, size_t old_size, size_t new_size) {
        return pool_impl::instance().reallocate(p, old_size, new_size);
    }
};

// =========================================================================
// pool_single — Single-threaded memory pool (no lock overhead)
// =========================================================================

/**
 * @brief  Single-threaded memory pool for a fixed block size.
 *
 * Unlike default_alloc (which handles multiple sizes via the global
 * singleton), pool_single manages blocks of a single size. This
 * eliminates locking overhead and is ideal for:
 * - Fiber-local allocators where each fiber has its own pool.
 * - Per-container allocators with a known fixed node size.
 *
 * @note  NOT thread-safe. Each thread/fiber should use its own instance.
 *
 * Usage:
 * @code
 * pool_single my_pool(sizeof(MyNode), 64);
 * MyNode* node = static_cast<MyNode*>(my_pool.allocate());
 * my_pool.deallocate(node);
 * @endcode
 */
class pool_single {
public:
    /**
     * @brief  Constructs a single-size memory pool.
     *
     * @param  block_size        Size of each block in bytes.
     * @param  blocks_per_chunk  Number of blocks to allocate per chunk (default 64).
     *
     * @pre    block_size >= sizeof(void*) (required by the intrusive freelist).
     */
    explicit pool_single(size_t block_size, size_t blocks_per_chunk = 64)
        : block_size_(block_size)
        , blocks_per_chunk_(blocks_per_chunk)
        , free_list_()
        , bytes_allocated_(0) {}

    ~pool_single() {
        // Chunks are intentionally leaked to avoid tracking them.
        // In a production system, you would track and free all chunks.
    }

    // Non-copyable
    pool_single(const pool_single&) = delete;
    pool_single& operator=(const pool_single&) = delete;

    /**
     * @brief  Allocates a single block from the pool.
     * @return  Pointer to a block of block_size_ bytes.
     *
     * @throws std::bad_alloc  If a new chunk cannot be allocated.
     */
    void* allocate() {
        void* p = free_list_.pop();
        if (p != nullptr) return p;
        return refill();
    }

    /**
     * @brief  Returns a block to the pool.
     * @param  p  Pointer previously returned by allocate().
     */
    void deallocate(void* p) {
        free_list_.push(p);
    }

    /**
     * @brief  Returns the block size this pool manages.
     * @return  Block size in bytes.
     */
    size_t block_size() const { return block_size_; }

    /**
     * @brief  Returns total bytes allocated from the OS.
     * @return  Total bytes across all chunks.
     */
    size_t allocated_bytes() const { return bytes_allocated_; }

private:
    /**
     * @brief  Allocates a new chunk and carves it into blocks.
     * @return  Pointer to the first block (the rest go to the freelist).
     */
    void* refill() {
        size_t total = block_size_ * blocks_per_chunk_;
        char* chunk = static_cast<char*>(malloc_alloc::allocate(total));
        bytes_allocated_ += total;

        // Push all blocks except the first onto the freelist
        for (size_t i = 1; i < blocks_per_chunk_; ++i) {
            free_list_.push(chunk + i * block_size_);
        }

        return chunk;
    }

    size_t block_size_;         ///< Size of each block.
    size_t blocks_per_chunk_;   ///< Number of blocks per chunk.
    freelist free_list_;        ///< Free list for this pool.
    size_t bytes_allocated_;    ///< Total bytes allocated (debug).
};

} // namespace lstl
