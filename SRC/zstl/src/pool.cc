// zstl MultiSizeClassPool — slow-path implementation
//
// This file implements:
// - Singleton instance (Meyer's singleton)
// - Constructor: pre-allocate initial chunks for small size classes
// - allocateSlow: refill tcache from global freelist, or allocate new chunk
// - deallocateSlow: flush tcache to global freelist when full
// - refillBatch / flushBatch: batch transfer between tcache and global freelist
// - allocateChunk: mmap a new chunk, split into blocks, push to freelist
// - pool_trim: iterate chunks, munmap fully-free ones

#include <unistd.h>
#include <cstdio>
// - pool_stats: aggregate statistics across all size classes

#include "zstl/memory/pool.h"

#include <sys/mman.h>       // mmap, munmap
#include <cstring>          // memcpy
#include <cstdio>           // stderr
#include <cstdlib>          // abort
#include <algorithm>        // std::max (only std usage for numeric clamp)

namespace zstl {

// ============================================================
// Size class constants (mirrors type_traits.h for ODR safety)
// ============================================================

namespace {

constexpr size_t kNumSC = kNumSizeClasses;  // 28

// Block size for each size class
constexpr size_t kBlockSizes[kNumSC] = {
    8,   16,  32,  48,  64,  80,  96,  112,
    128, 160, 192, 224, 256, 320, 384, 448,
    512, 768, 1024, 1536, 2048, 2560, 3072, 3584,
    4096, 5120, 6144, 8192
};

// Tcache capacity per size class
constexpr size_t kTcacheCaps[kNumSC] = {
    64,  64,  64,  64,  64,  48,  48,  48,
    32,  32,  32,  32,  16,  16,  16,  16,
    16,  8,   8,   8,   8,   4,   4,   4,
    4,   4,   4,   4
};

// Number of blocks per mmap'd chunk for each size class.
// Larger blocks mean fewer blocks per chunk to limit chunk size.
constexpr size_t kBlocksPerChunk(size_t idx) noexcept {
    // Target chunk size between 64KB and 256KB
    constexpr size_t kTargetChunkSize = 128 * 1024;
    size_t block_size = kBlockSizes[idx];
    size_t blocks = kTargetChunkSize / block_size;
    if (blocks < 32)  blocks = 32;
    if (blocks > 1024) blocks = 1024;
    return blocks;
}

// Page size (cached at startup)
size_t get_page_size() noexcept {
    static size_t ps = 0;
    if (__builtin_expect(ps == 0, 0)) {
        ps = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        if (ps == 0) ps = 4096;
    }
    return ps;
}

} // anonymous namespace

// ============================================================
// Thread-local tcache storage
// ============================================================

thread_local TCacheBin t_tcache[kNumSizeClasses];

namespace {
// RAII helper to initialize tcache low watermarks on thread startup
struct TCacheInit {
    TCacheInit() {
        for (size_t i = 0; i < kNumSizeClasses; ++i) {
            t_tcache[i].low_watermark = tcache_low_watermark(i);
        }
    }
};
thread_local TCacheInit t_tcache_init;
} // anonymous namespace

// ============================================================
// Singleton — Meyer's singleton, thread-safe in C++11+
// ============================================================

MultiSizeClassPool& MultiSizeClassPool::instance() noexcept {
    static MultiSizeClassPool pool;
    return pool;
}

// ============================================================
// Constructor — pre-allocate initial chunks for small size
// classes (high-demand, small blocks)
// ============================================================

MultiSizeClassPool::MultiSizeClassPool() {
    // Pre-allocate initial chunks for small size classes (indices 0–8).
    // These cover allocations up to 128 bytes, which is the most common range.
    // Each pre-allocated chunk goes directly to the global freelist so
    // the first allocation from any thread hits the global list, not mmap.
    //
    // We skip larger size classes to avoid excessive RSS on startup.
    // They will fault in on first use via allocateSlow.
    for (size_t i = 0; i <= 8; ++i) {
        size_t block_size = kBlockSizes[i];
        size_t num_blocks = kBlocksPerChunk(i);

        // Allocate and immediately push all blocks to global freelist
        void* chunk_raw = allocateChunk(block_size, num_blocks);
        if (!chunk_raw) continue;  // OOM — skip this class

        FreeNode* head = static_cast<FreeNode*>(chunk_raw);
        FreeNode* tail = head;
        for (size_t j = 1; j < num_blocks; ++j) {
            tail = tail->next;
        }
        global_freelists_[i].pushList(head, tail, num_blocks);
    }
}

// ============================================================
// Destructor — return all chunks to OS
// ============================================================

MultiSizeClassPool::~MultiSizeClassPool() {
    // Walk chunk list and munmap everything.
    // Note: In a well-behaved program, all allocations should be freed
    // before the pool is destroyed. Any outstanding allocations are
    // leaked intentionally (the OS will clean them up regardless).
    ChunkHeader* chunk = chunk_list_;
    while (chunk) {
        ChunkHeader* next = chunk->next;
        // The chunk allocation includes the header, so we munmap from
        // the header address, not data().
        size_t total_size = sizeof(ChunkHeader) +
            chunk->block_size * chunk->num_blocks;
        total_size = (total_size + get_page_size() - 1) &
                     ~(get_page_size() - 1);
        munmap(chunk, total_size);
        chunk = next;
    }
    chunk_list_ = nullptr;
    chunk_count_.store(0, std::memory_order_relaxed);
}

// ============================================================
// allocateChunk — mmap a new chunk, carve into blocks, return
// pointer to the first block
// ============================================================

void* MultiSizeClassPool::allocateChunk(size_t block_size,
                                         size_t num_blocks) {
    // Compute total allocation size including chunk header
    size_t data_size = block_size * num_blocks;
    size_t total_size = sizeof(ChunkHeader) + data_size;

    // Page-align the allocation for mmap efficiency
    size_t page_size = get_page_size();
    total_size = (total_size + page_size - 1) & ~(page_size - 1);

    // Try mmap first (preferred for large allocations)
    void* raw = mmap(nullptr, total_size,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS,
                     -1, 0);
    if (raw == MAP_FAILED) {
        // Fallback to standard operator new if mmap fails
        // (e.g., on systems where mmap is unavailable or limited)
        raw = ::operator new(total_size, std::nothrow);
        if (!raw) {
            return nullptr;  // OOM
        }
    }

    total_allocated_.fetch_add(total_size, std::memory_order_relaxed);

    // Initialize chunk header
    ChunkHeader* header = static_cast<ChunkHeader*>(raw);
    header->block_size = block_size;
    header->num_blocks = num_blocks;
    header->num_free   = num_blocks;

    // Insert into chunk list (lock-free push to front)
    header->next = chunk_list_;
    chunk_list_ = header;
    chunk_count_.fetch_add(1, std::memory_order_relaxed);

    // Carve data region into blocks and thread onto a linked list
    char* data = static_cast<char*>(header->data());
    for (size_t i = 0; i < num_blocks - 1; ++i) {
        FreeNode* node = reinterpret_cast<FreeNode*>(data + i * block_size);
        node->next = reinterpret_cast<FreeNode*>(data + (i + 1) * block_size);
    }
    FreeNode* last = reinterpret_cast<FreeNode*>(
        data + (num_blocks - 1) * block_size);
    last->next = nullptr;

    return static_cast<void*>(data);  // Return first block
}

// ============================================================
// allocateSlow — refill tcache and return one block
//
// Strategy:
// 1. Try to pop a single block from the global freelist
// 2. If global freelist is empty, allocate a new chunk from OS
// 3. Push all blocks from chunk (except one) into tcache
// 4. Return the one block to the caller
//
// The batch push into tcache amortizes the cost of the global
// freelist access and OS allocation across many calls.
// ============================================================

void* MultiSizeClassPool::allocateSlow(size_t idx) {
    TCacheBin& bin = t_tcache[idx];
    size_t block_size = kBlockSizes[idx];

    // Step 1: Try to grab a batch from the global freelist
    // We want enough to fill the tcache to its capacity.
    size_t needed = tcache_capacity(idx) - bin.count;
    if (needed > 0) {
        FreeNode* batch = global_freelists_[idx].popBatch(needed);
        if (batch) {
            // Count batch size and find tail
            FreeNode* tail = batch;
            size_t count = 1;
            while (tail->next) {
                tail = tail->next;
                ++count;
            }

            // Pop first block for caller, rest goes to tcache
            FreeNode* result = batch;
            bin.head = batch->next;
            bin.count = count - 1;
            return static_cast<void*>(result);
        }
    }

    // Step 2: Global freelist is empty — allocate a new chunk from OS
    size_t num_blocks = kBlocksPerChunk(idx);
    FreeNode* chunk_head = static_cast<FreeNode*>(
        allocateChunk(block_size, num_blocks));
    if (!chunk_head) {
        // OOM: fall back to ::operator new for this allocation only
        fallback_count_.fetch_add(1, std::memory_order_relaxed);
        return ::operator new(block_size, std::nothrow);
    }

    // Step 3: Pop first block for caller, rest goes to tcache
    FreeNode* result = chunk_head;
    bin.head = chunk_head->next;
    bin.count = num_blocks - 1;

    return static_cast<void*>(result);
}

// ============================================================
// deallocateSlow — flush excess tcache entries and add freed
// block to tcache
//
// Strategy:
// 1. If tcache is at capacity, flush half to global freelist
// 2. Add freed block to tcache
//
// This ensures the tcache doesn't hoard too many blocks while
// other threads might be starving.
// ============================================================

void MultiSizeClassPool::deallocateSlow(void* ptr, size_t idx) {
    if (!ptr) return;

    TCacheBin& bin = t_tcache[idx];

    // If tcache is full (or nearly full), push half to global freelist
    size_t cap = tcache_capacity(idx);
    if (bin.count >= cap) {
        size_t flush_count = bin.count / 2;
        if (flush_count == 0) flush_count = 1;

        // Extract 'flush_count' nodes from the front of tcache list
        FreeNode* batch = bin.head;
        FreeNode* tail = batch;
        for (size_t i = 1; i < flush_count && tail; ++i) {
            tail = tail->next;
        }
        if (tail) {
            bin.head = tail->next;
            bin.count -= flush_count;
            tail->next = nullptr;
            global_freelists_[idx].pushList(batch, tail, flush_count);
        }
    }

    // Add the freed block to the tcache
    FreeNode* node = static_cast<FreeNode*>(ptr);
    node->next = bin.head;
    bin.head = node;
    ++bin.count;
}

// ============================================================
// refillBatch — grab a batch from the global freelist into tcache
//
// This is called when tcache count drops below the low watermark.
// We try to grab enough to fill to half capacity (batch refill
// amortizes global freelist access cost).
// ============================================================

void MultiSizeClassPool::refillBatch(TCacheBin& bin, size_t idx) {
    size_t cap = tcache_capacity(idx);
    size_t batch_size = cap / 2;
    if (batch_size < 4) batch_size = 4;
    if (batch_size > cap - bin.count) batch_size = cap - bin.count;
    if (batch_size == 0) return;

    // Try global freelist first
    FreeNode* batch = global_freelists_[idx].popBatch(batch_size);

    if (!batch) {
        // Global freelist is also empty — allocate a new chunk
        size_t block_size = kBlockSizes[idx];
        size_t num_blocks = kBlocksPerChunk(idx);
        batch = static_cast<FreeNode*>(
            allocateChunk(block_size, num_blocks));

        if (!batch) return;  // OOM

        // Limit to the requested batch size
        // (allocateChunk may return more than batch_size blocks)
        FreeNode* tail = batch;
        size_t count = 1;
        while (count < batch_size && tail->next) {
            tail = tail->next;
            ++count;
        }

        // Push remainder to global freelist
        if (tail->next) {
            FreeNode* remaining = tail->next;
            tail->next = nullptr;  // Terminate batch
            FreeNode* remaining_tail = remaining;
            size_t remaining_count = 1;
            while (remaining_tail->next) {
                remaining_tail = remaining_tail->next;
                ++remaining_count;
            }
            global_freelists_[idx].pushList(remaining, remaining_tail,
                                             remaining_count);
        }
    }

    // Prepend batch to tcache's existing freelist
    if (batch) {
        FreeNode* tail = batch;
        size_t count = 1;
        while (tail->next) {
            tail = tail->next;
            ++count;
        }
        tail->next = bin.head;
        bin.head = batch;
        bin.count += count;
    }
}

// ============================================================
// flushBatch — return excess tcache objects to global freelist
//
// Keep roughly half of tcache capacity locally; push the rest
// back to the global freelist so other threads can use them.
// ============================================================

void MultiSizeClassPool::flushBatch(TCacheBin& bin, size_t idx) {
    size_t keep = tcache_capacity(idx) / 2;
    if (bin.count <= keep) return;

    size_t to_flush = bin.count - keep;

    // Extract the first 'to_flush' nodes
    FreeNode* batch = bin.head;
    FreeNode* tail = batch;
    for (size_t i = 1; i < to_flush && tail; ++i) {
        tail = tail->next;
    }
    if (!tail || !tail->next) return;  // Safety check

    bin.head = tail->next;
    bin.count = keep;
    tail->next = nullptr;

    global_freelists_[idx].pushList(batch, tail, to_flush);
}

// ============================================================
// pool_trim — return fully-free chunks to the OS
//
// Strategy:
// 1. Pull all nodes from each global freelist
// 2. For each chunk, check if all its blocks are in the freelist
//    (meaning the chunk is fully free)
// 3. munmap fully-free chunks
// 4. Push remaining blocks back to freelists
//
// Returns the total number of bytes returned to the OS.
// ============================================================

size_t MultiSizeClassPool::pool_trim() {
    size_t total_trimmed = 0;

    // First, drain all global freelists so we can inspect chunks
    FreeNode* drained[kNumSizeClasses] = {};
    size_t drained_counts[kNumSizeClasses] = {};

    for (size_t i = 0; i < kNumSizeClasses; ++i) {
        // Pop all nodes from this size class's freelist
        FreeNode* list = global_freelists_[i].popAll();
        if (!list) continue;

        // Count total drained nodes
        FreeNode* cur = list;
        size_t count = 0;
        while (cur) {
            ++count;
            cur = cur->next;
        }
        drained[i] = list;
        drained_counts[i] = count;
    }

    // Now walk the chunk list and identify fully-free chunks
    ChunkHeader** prev = &chunk_list_;
    ChunkHeader* chunk = chunk_list_;

    while (chunk) {
        // Find which size class this chunk belongs to
        size_t idx = 0;
        for (; idx < kNumSizeClasses; ++idx) {
            if (kBlockSizes[idx] == chunk->block_size) break;
        }
        if (idx >= kNumSizeClasses) {
            prev = &chunk->next;
            chunk = chunk->next;
            continue;
        }

        // Check if all blocks in this chunk are in the drained freelist.
        // We need chunk->num_free == chunk->num_blocks AND all freed
        // blocks must be in this chunk's address range.
        // A simpler heuristic: if all blocks in the freelist for this
        // size class, when added up, cover this chunk entirely.
        // For correctness, we scan the drained list and count blocks
        // that fall within this chunk's data range.
        char* data_start = static_cast<char*>(chunk->data());
        char* data_end = data_start + chunk->block_size * chunk->num_blocks;
        size_t freed_in_chunk = 0;

        FreeNode** node_prev = &drained[idx];
        FreeNode* node = drained[idx];
        while (node) {
            char* addr = reinterpret_cast<char*>(node);
            if (addr >= data_start && addr < data_end) {
                // Block belongs to this chunk — remove from drained list
                freed_in_chunk++;
                *node_prev = node->next;
                node = node->next;
                // Don't advance node_prev (it already points to next)
            } else {
                node_prev = &node->next;
                node = node->next;
            }
        }

        if (freed_in_chunk == chunk->num_blocks) {
            // Chunk is completely free — munmap it
            size_t total_size = sizeof(ChunkHeader) +
                chunk->block_size * chunk->num_blocks;
            size_t page_size = get_page_size();
            total_size = (total_size + page_size - 1) & ~(page_size - 1);

            ChunkHeader* next = chunk->next;
            *prev = next;

            total_trimmed += total_size;
            total_allocated_.fetch_sub(total_size, std::memory_order_relaxed);
            chunk_count_.fetch_sub(1, std::memory_order_relaxed);

            munmap(chunk, total_size);
            chunk = next;
        } else {
            prev = &chunk->next;
            chunk = chunk->next;
        }
        drained_counts[idx] -= freed_in_chunk;
    }

    // Push remaining drained nodes back to their freelists
    for (size_t i = 0; i < kNumSizeClasses; ++i) {
        if (drained[i] && drained_counts[i] > 0) {
            // Find tail
            FreeNode* tail = drained[i];
            while (tail->next) tail = tail->next;
            global_freelists_[i].pushList(drained[i], tail,
                                           drained_counts[i]);
        }
    }

    return total_trimmed;
}

// ============================================================
// pool_stats — aggregate statistics across all size classes
// ============================================================

void MultiSizeClassPool::pool_stats(struct pool_stats& out) const {
    out.total_allocated_bytes = total_allocated_.load(std::memory_order_relaxed);
    out.fallback_allocations = fallback_count_.load(std::memory_order_relaxed);
    out.chunk_count = chunk_count_.load(std::memory_order_relaxed);

    out.global_free_count = 0;
    out.trim_candidates = 0;

    for (size_t i = 0; i < kNumSizeClasses; ++i) {
        size_t free_cnt = global_freelists_[i].count();
        out.global_free_count += free_cnt;

        out.classes[i].block_size  = kBlockSizes[i];
        out.classes[i].global_free = free_cnt;
        out.classes[i].capacity    = tcache_capacity(i);

        // A chunk is a trim candidate if it's over 75% free
        // (we can't know exactly without walking the chunk list,
        //  so this is an approximation)
        if (free_cnt > 0) {
            out.trim_candidates++;
        }
    }

    // tcache stats are approximate (we can only see our own thread)
    out.tcache_free_count = 0;
    for (size_t i = 0; i < kNumSizeClasses; ++i) {
        out.tcache_free_count += t_tcache[i].count;
    }

    // Estimate total bytes in use: allocated - (global_free + tcache_free)
    // by converting free counts to bytes per size class (approximate)
    size_t approx_free_bytes = 0;
    for (size_t i = 0; i < kNumSizeClasses; ++i) {
        approx_free_bytes += out.classes[i].global_free * kBlockSizes[i];
        approx_free_bytes += t_tcache[i].count * kBlockSizes[i];
    }
    out.total_in_use_bytes = (out.total_allocated_bytes > approx_free_bytes)
        ? (out.total_allocated_bytes - approx_free_bytes)
        : 0;
}

} // namespace zstl
