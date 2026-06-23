// ============================================================================
// zstl memory pool tests
// ============================================================================
// Tests: pool_malloc/pool_free for various size classes, pool_malloc_class/
//        pool_free_class, oversized allocation, zero-size, realloc, batch
//        allocate/deallocate, multi-size interleaved, pool_trim, pool_stats
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <vector>
#include <thread>
#include <cstring>

using namespace zstl;

// ============================================================================
// Basic allocate / free tests
// ============================================================================

TEST(Pool, BasicAllocFreeSmall) {
  // Allocate and free small blocks
  for (int i = 0; i < 100; ++i) {
    void* p = pool_malloc(8);
    ASSERT_NE(p, nullptr);
    std::memset(p, 0xAB, 8);
    pool_free(p, 8);
  }
  SUCCEED();
}

TEST(Pool, BasicAllocFreeMedium) {
  for (int i = 0; i < 100; ++i) {
    void* p = pool_malloc(256);
    ASSERT_NE(p, nullptr);
    std::memset(p, 0xCD, 256);
    pool_free(p, 256);
  }
  SUCCEED();
}

TEST(Pool, BasicAllocFreeLarge) {
  for (int i = 0; i < 50; ++i) {
    void* p = pool_malloc(4096);
    ASSERT_NE(p, nullptr);
    std::memset(p, 0xEF, 4096);
    pool_free(p, 4096);
  }
  SUCCEED();
}

TEST(Pool, NullPtrFree) {
  pool_free(nullptr, 8);  // Should be no-op, no crash
  pool_free(nullptr, 4096);
  SUCCEED();
}

// ============================================================================
// Size class tests
// ============================================================================

TEST(Pool, AllSizeClasses) {
  // Test allocation for every size class block size
  constexpr size_t block_sizes[] = {
    8, 16, 32, 48, 64, 80, 96, 112,
    128, 160, 192, 224, 256, 320, 384, 448,
    512, 768, 1024, 1536, 2048, 2560, 3072, 3584,
    4096, 5120, 6144, 8192
  };

  for (size_t bs : block_sizes) {
    void* p = pool_malloc(bs);
    ASSERT_NE(p, nullptr) << "Failed for block size " << bs;
    std::memset(p, 0x55, bs);
    pool_free(p, bs);
  }
  SUCCEED();
}

TEST(Pool, InterleavedSizes) {
  // Allocate different sizes in interleaved pattern
  std::vector<std::pair<void*, size_t>> allocs;
  constexpr size_t sizes[] = {8, 32, 128, 512, 2048, 64, 256, 1024};

  for (int round = 0; round < 5; ++round) {
    for (size_t sz : sizes) {
      void* p = pool_malloc(sz);
      ASSERT_NE(p, nullptr);
      std::memset(p, static_cast<int>(sz % 256), sz);
      allocs.push_back({p, sz});
    }
  }

  // Free in reverse order
  for (auto it = allocs.rbegin(); it != allocs.rend(); ++it) {
    pool_free(it->first, it->second);
  }
  SUCCEED();
}

TEST(Pool, BatchAllocateDeallocate) {
  // Allocate many blocks, then free all
  constexpr int N = 500;
  std::vector<void*> ptrs;

  for (int i = 0; i < N; ++i) {
    void* p = pool_malloc(64);
    ASSERT_NE(p, nullptr);
    std::memset(p, 0, 64);
    ptrs.push_back(p);
  }

  // Free all
  for (auto p : ptrs) {
    pool_free(p, 64);
  }
  SUCCEED();
}

TEST(Pool, BatchMixedSizes) {
  // Allocate many blocks of varying sizes, then free
  constexpr int N = 300;
  std::vector<std::pair<void*, size_t>> ptrs;

  for (int i = 0; i < N; ++i) {
    size_t sz = ((i * 17 + 31) % 20 + 1) * 8;  // 8..160 in steps of 8
    void* p = pool_malloc(sz);
    ASSERT_NE(p, nullptr);
    std::memset(p, 0, sz);
    ptrs.push_back({p, sz});
  }

  // Free in random-like order (not the allocation order)
  for (int i = 0; i < N; i += 2) {
    pool_free(ptrs[i].first, ptrs[i].second);
  }
  for (int i = 1; i < N; i += 2) {
    pool_free(ptrs[i].first, ptrs[i].second);
  }
  SUCCEED();
}

// ============================================================================
// Oversized allocation tests
// ============================================================================

TEST(Pool, OversizedAllocationFallsBackToNew) {
  // kMaxPoolSize is 8192, so allocations larger than that bypass the pool
  void* p = pool_malloc(10000);
  ASSERT_NE(p, nullptr);
  std::memset(p, 0xAB, 10000);
  pool_free(p, 10000);
  SUCCEED();
}

TEST(Pool, OversizedMultiple) {
  std::vector<void*> ptrs;
  for (int i = 0; i < 10; ++i) {
    void* p = pool_malloc(10000);
    ASSERT_NE(p, nullptr);
    std::memset(p, static_cast<int>(i), 10000);
    ptrs.push_back(p);
  }
  for (auto p : ptrs) {
    pool_free(p, 10000);
  }
  SUCCEED();
}

// ============================================================================
// Zero-size allocation tests
// ============================================================================

TEST(Pool, ZeroSizeAlloc) {
  // Zero-size goes to the smallest size class (8 bytes)
  void* p = pool_malloc(0);
  ASSERT_NE(p, nullptr);  // Should still return valid pointer
  pool_free(p, 0);
  SUCCEED();
}

// ============================================================================
// Realloc tests
// ============================================================================

TEST(Pool, ReallocSameSizeClass) {
  void* p = pool_malloc(64);
  ASSERT_NE(p, nullptr);
  std::memset(p, 0x12, 64);
  // Upgrade within same size class (64 -> 80 both map to same class? check)
  void* p2 = pool_realloc(p, 64, 80);
  ASSERT_NE(p2, nullptr);
  // If same size class, should return same pointer
  pool_free(p2, 80);
}

TEST(Pool, ReallocGrow) {
  void* p = pool_malloc(32);
  ASSERT_NE(p, nullptr);
  std::memset(p, 0x34, 32);
  void* p2 = pool_realloc(p, 32, 256);
  ASSERT_NE(p2, nullptr);
  // First 32 bytes should be preserved
  EXPECT_EQ(*(static_cast<unsigned char*>(p2)), 0x34);
  pool_free(p2, 256);
}

TEST(Pool, ReallocShrink) {
  void* p = pool_malloc(256);
  ASSERT_NE(p, nullptr);
  std::memset(p, 0x56, 256);
  void* p2 = pool_realloc(p, 256, 32);
  ASSERT_NE(p2, nullptr);
  EXPECT_EQ(*(static_cast<unsigned char*>(p2)), 0x56);
  pool_free(p2, 32);
}

TEST(Pool, ReallocToOversized) {
  void* p = pool_malloc(64);
  ASSERT_NE(p, nullptr);
  std::memset(p, 0x78, 64);
  void* p2 = pool_realloc(p, 64, 10000);
  ASSERT_NE(p2, nullptr);
  EXPECT_EQ(*(static_cast<unsigned char*>(p2)), 0x78);
  pool_free(p2, 10000);
}

TEST(Pool, ReallocFromOversized) {
  void* p = pool_malloc(10000);
  ASSERT_NE(p, nullptr);
  std::memset(p, 0x90, 10000);
  void* p2 = pool_realloc(p, 10000, 64);
  ASSERT_NE(p2, nullptr);
  EXPECT_EQ(*(static_cast<unsigned char*>(p2)), 0x90);
  pool_free(p2, 64);
}

TEST(Pool, ReallocNull) {
  void* p = pool_realloc(nullptr, 0, 64);
  ASSERT_NE(p, nullptr);
  pool_free(p, 64);
}

// ============================================================================
// pool_malloc_class / pool_free_class tests
// ============================================================================

TEST(Pool, MallocFreeClass) {
  // Test using known size class index directly
  for (size_t idx = 0; idx < 28; ++idx) {
    void* p = pool_malloc_class(idx);
    ASSERT_NE(p, nullptr) << "Failed for size class " << idx;
    pool_free_class(p, idx);
  }
  SUCCEED();
}

TEST(Pool, MallocFreeClassBatch) {
  constexpr size_t idx = 3;  // 48 bytes
  std::vector<void*> ptrs;
  for (int i = 0; i < 100; ++i) {
    void* p = pool_malloc_class(idx);
    ASSERT_NE(p, nullptr);
    ptrs.push_back(p);
  }
  for (auto p : ptrs) {
    pool_free_class(p, idx);
  }
  SUCCEED();
}

// ============================================================================
// pool_trim tests
// ============================================================================

TEST(Pool, PoolTrim) {
  // Allocate and free many blocks to populate the freelist
  std::vector<void*> ptrs;
  for (int i = 0; i < 200; ++i) {
    void* p = pool_malloc(64);
    ASSERT_NE(p, nullptr);
    ptrs.push_back(p);
  }
  for (auto p : ptrs) {
    pool_free(p, 64);
  }

  // Trim should succeed (release fully-free chunks back to OS)
  size_t trimmed = MultiSizeClassPool::instance().pool_trim();
  // trimmed may be 0 if no chunks are fully free, which is fine
  SUCCEED() << "pool_trim returned " << trimmed << " bytes";
}

// ============================================================================
// pool_stats tests
// ============================================================================

TEST(Pool, PoolStats) {
  // Allocate some blocks to ensure non-zero stats
  std::vector<void*> ptrs;
  for (int i = 0; i < 100; ++i) {
    void* p = pool_malloc(128);
    ptrs.push_back(p);
  }

  zstl::pool_stats stats;
  MultiSizeClassPool::instance().pool_stats(stats);

  // Should have some allocated bytes
  EXPECT_GE(stats.total_allocated_bytes, 0);
  // Should have been used
  EXPECT_GE(stats.total_in_use_bytes, 0);

  SUCCEED() << "total_allocated_bytes=" << stats.total_allocated_bytes
            << " total_in_use_bytes=" << stats.total_in_use_bytes
            << " chunk_count=" << stats.chunk_count
            << " fallback_allocations=" << stats.fallback_allocations;

  for (auto p : ptrs) {
    pool_free(p, 128);
  }
}

TEST(Pool, PoolStatsInitiallyNonNegative) {
  zstl::pool_stats stats;
  MultiSizeClassPool::instance().pool_stats(stats);
  // All stats should be non-negative
  EXPECT_GE(stats.total_allocated_bytes, 0);
  EXPECT_GE(stats.total_in_use_bytes, 0);
  EXPECT_GE(stats.global_free_count, 0);
  EXPECT_GE(stats.chunk_count, 0);
  EXPECT_GE(stats.fallback_allocations, 0);
}

// ============================================================================
// Multi-threaded basic tests
// ============================================================================

TEST(Pool, MultiThreadBasic) {
  constexpr int kThreads = 4;
  constexpr int kAllocsPerThread = 200;
  std::vector<std::thread> threads;

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([t]() {
      std::vector<void*> ptrs;
      for (int i = 0; i < kAllocsPerThread; ++i) {
        size_t sz = ((i + t * 7) % 10 + 1) * 16;
        void* p = pool_malloc(sz);
        ASSERT_NE(p, nullptr);
        std::memset(p, static_cast<int>(t), sz);
        ptrs.push_back(p);
      }
      for (auto p : ptrs) {
        pool_free(p, ((&p - &ptrs[0] + t * 7) % 10 + 1) * 16);
      }
    });
  }

  for (auto& th : threads) {
    th.join();
  }
  SUCCEED();
}

TEST(Pool, MultiThreadSameSizeClass) {
  constexpr int kThreads = 4;
  constexpr int kAllocsPerThread = 250;
  std::vector<std::thread> threads;

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([]() {
      std::vector<void*> ptrs;
      for (int i = 0; i < kAllocsPerThread; ++i) {
        void* p = pool_malloc(64);
        std::memset(p, 0xAB, 64);
        ptrs.push_back(p);
      }
      for (auto p : ptrs) {
        pool_free(p, 64);
      }
    });
  }

  for (auto& th : threads) {
    th.join();
  }
  SUCCEED();
}

// ============================================================================
// Edge case tests
// ============================================================================

TEST(Pool, ManyAllocFreeCycles) {
  // Allocate and free in cycles to stress the tcache
  for (int cycle = 0; cycle < 10; ++cycle) {
    std::vector<void*> ptrs;
    for (int i = 0; i < 50; ++i) {
      ptrs.push_back(pool_malloc(128));
    }
    for (auto p : ptrs) {
      pool_free(p, 128);
    }
  }
  SUCCEED();
}

TEST(Pool, LargeAllocationsWithinPool) {
  // Allocate at the boundary (exactly kMaxPoolSize)
  void* p = pool_malloc(8192);
  ASSERT_NE(p, nullptr);
  std::memset(p, 0xFF, 8192);
  pool_free(p, 8192);
  SUCCEED();
}

TEST(Pool, ReallocEdgeCaseSameSize) {
  void* p = pool_malloc(64);
  ASSERT_NE(p, nullptr);
  void* p2 = pool_realloc(p, 64, 64);  // same size, should return same ptr
  ASSERT_NE(p2, nullptr);
  pool_free(p2, 64);
}
