#include "lstl_test_common.h"

#include <cstring>
#include <vector>

#include "alloc.h"
#include "memory/pool.h"

int main() {
  // 小块：对齐到 8B size class
  void* p8 = lstl::pool_alloc_t::allocate(1);
  LSTL_CHECK(p8 != 0);
  lstl::pool_alloc_t::deallocate(p8, 1);

  void* p32 = lstl::pool_alloc_t::allocate(25);
  LSTL_CHECK(p32 != 0);
  lstl::pool_alloc_t::deallocate(p32, 25);

  void* p128 = lstl::pool_alloc_t::allocate(100);
  LSTL_CHECK(p128 != 0);
  lstl::pool_alloc_t::deallocate(p128, 100);

  // 大块：LargeHeader 路径
  void* large = lstl::pool_alloc_t::allocate(512);
  LSTL_CHECK(large != 0);
  std::memset(large, 0xAB, 512);
  lstl::pool_alloc_t::deallocate(large, 512);

  // 超大：直通 malloc
  void* huge = lstl::pool_alloc_t::allocate(65536);
  LSTL_CHECK(huge != 0);
  lstl::pool_alloc_t::deallocate(huge, 65536);

  // 默认 alloc 应为 pool
  int* arr = lstl::simple_alloc<int>::allocate(16);
  LSTL_CHECK(arr != 0);
  lstl::simple_alloc<int>::deallocate(arr, 16);

  // 高频 alloc/free 回归
  std::vector<void*> ptrs;
  ptrs.reserve(256);
  for (int round = 0; round < 4; ++round) {
    for (int i = 0; i < 256; ++i) {
      void* p = lstl::pool_alloc_t::allocate(32);
      LSTL_CHECK(p != 0);
      ptrs.push_back(p);
    }
    for (size_t i = 0; i < ptrs.size(); ++i) {
      lstl::pool_alloc_t::deallocate(ptrs[i], 32);
    }
    ptrs.clear();
  }

#ifndef LSTL_POOL_DISABLE_STATS
  LSTL_CHECK(lstl::pool_alloc_t::small_alloc_count() >= 256u * 4u);
  LSTL_CHECK(lstl::pool_alloc_t::small_free_count() >= 256u * 4u);
#endif

  const size_t mapped_before = lstl::pool_alloc_t::small_mapped_bytes();
  LSTL_CHECK(mapped_before > 0);

#if !LSTL_POOL_LIGHT
  const size_t released = lstl::pool_alloc_t::purge_idle_memory();
  std::printf("PASS test_pool_single (mapped_before=%zu released=%zu)\n", mapped_before,
              released);
#else
  std::printf("PASS test_pool_single (LIGHT mode, heap=%zu)\n", mapped_before);
#endif
  return 0;
}
