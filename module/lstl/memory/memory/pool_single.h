#ifndef LSTL_MEMORY_POOL_SINGLE_H
#define LSTL_MEMORY_POOL_SINGLE_H

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "../config.h"
#include "freelist.h"
#include "large.h"
#include "malloc_alloc.h"
#include "size_class.h"

#if !LSTL_POOL_LIGHT
#include "span.h"
#include "span_registry.h"
#endif

namespace lstl {
namespace detail {

struct PoolSingle {
  FreeNode* free_list[LSTL_FREELISTS];
  char* bump_start;
  char* bump_end;
  size_t heap_size;

#ifndef LSTL_POOL_DISABLE_STATS
  uint64_t alloc_count;
  uint64_t free_count;
  uint64_t refill_count_;
#endif

  PoolSingle() : bump_start(0), bump_end(0), heap_size(0) {
    std::memset(free_list, 0, sizeof(free_list));
#ifndef LSTL_POOL_DISABLE_STATS
    alloc_count = 0;
    free_count = 0;
    refill_count_ = 0;
#endif
  }

  static PoolSingle& instance() {
    static PoolSingle pool;
    return pool;
  }

  static void* allocate(size_t n) {
    if (n == 0) {
      return 0;
    }
    if (is_large_request(n)) {
      return large_allocate(n);
    }
    if (!is_small_request(n)) {
      return malloc_alloc_t::allocate(n);
    }

    PoolSingle& pool = instance();
    const size_t bytes = align_up(n);
    const size_t index = size_class_index(bytes);
    FreeNode** list = &pool.free_list[index];

    FreeNode* node = freelist_pop(list);
    if (!node) {
      node = static_cast<FreeNode*>(pool.refill(bytes, index));
      if (!node) {
        return 0;
      }
#if !LSTL_POOL_LIGHT
    } else if (SpanHeader* span = span_from_ptr(node)) {
      --span->free_count;
#endif
    }

#ifndef LSTL_POOL_DISABLE_STATS
    ++pool.alloc_count;
#endif
    return node;
  }

  static void deallocate(void* p, size_t n) {
    if (!p || n == 0) {
      return;
    }
    if (is_large_request(n)) {
      large_deallocate(p, n);
      return;
    }
    if (!is_small_request(n)) {
      malloc_alloc_t::deallocate(p, n);
      return;
    }

    PoolSingle& pool = instance();
    const size_t bytes = align_up(n);
    const size_t index = size_class_index(bytes);
    FreeNode* node = static_cast<FreeNode*>(p);

#if !LSTL_POOL_LIGHT
    if (SpanHeader* span = span_from_ptr(p)) {
      ++span->free_count;
    }
#endif

    freelist_push(&pool.free_list[index], node);
#ifndef LSTL_POOL_DISABLE_STATS
    ++pool.free_count;
#endif
  }

  static size_t purge_idle_memory() {
#if LSTL_POOL_LIGHT
    return 0;
#else
    PoolSingle& pool = instance();
    SpanHeader* active = 0;
    if (pool.bump_start) {
      active = span_from_ptr(pool.bump_start);
    }
    return span_registry().purge_idle(active, pool.free_list, LSTL_FREELISTS);
#endif
  }

  static size_t mapped_bytes() {
#if LSTL_POOL_LIGHT
    return instance().heap_size;
#else
    return span_registry().mapped_bytes;
#endif
  }

#ifndef LSTL_POOL_DISABLE_STATS
  static uint64_t small_alloc_count() { return instance().alloc_count; }
  static uint64_t small_free_count() { return instance().free_count; }
  static uint64_t refill_count() { return instance().refill_count_; }
#endif

  static void (*set_malloc_handler(void (*f)()))() {
    return malloc_alloc_t::set_malloc_handler(f);
  }

 private:
#if !LSTL_POOL_LIGHT
  void account_new_blocks(char* chunk, int nobjs) {
    if (SpanHeader* span = span_from_ptr(chunk)) {
      span->block_count += static_cast<uint32_t>(nobjs);
      if (nobjs > 1) {
        span->free_count += static_cast<uint32_t>(nobjs - 1);
      }
    }
  }
#endif

  void* refill(size_t block_size, size_t index) {
#ifndef LSTL_POOL_DISABLE_STATS
    ++refill_count_;
#endif
    int nobjs = LSTL_POOL_REFILL_BATCH;
    char* chunk = chunk_alloc(block_size, nobjs, index);
    if (!chunk) {
      return 0;
    }

#if !LSTL_POOL_LIGHT
    account_new_blocks(chunk, nobjs);
#endif

    if (nobjs == 1) {
      return chunk;
    }

    FreeNode** list = &free_list[index];
    FreeNode* result = reinterpret_cast<FreeNode*>(chunk);
    FreeNode* next = reinterpret_cast<FreeNode*>(chunk + block_size);
    freelist_push(list, next);

    char* cursor = chunk + block_size;
    for (int i = 1; i < nobjs - 1; ++i) {
      FreeNode* current = reinterpret_cast<FreeNode*>(cursor);
      cursor += block_size;
      FreeNode* link = reinterpret_cast<FreeNode*>(cursor);
      current->next = link;
    }
    reinterpret_cast<FreeNode*>(cursor)->next = 0;
    return result;
  }

  char* chunk_alloc(size_t block_size, int& nobjs, size_t index) {
    const size_t total_bytes = block_size * static_cast<size_t>(nobjs);
    const size_t bytes_left =
        bump_end > bump_start ? static_cast<size_t>(bump_end - bump_start) : 0;

    if (bytes_left >= total_bytes) {
      char* result = bump_start;
      bump_start += total_bytes;
      return result;
    }
    if (bytes_left >= block_size) {
      nobjs = static_cast<int>(bytes_left / block_size);
      char* result = bump_start;
      bump_start += block_size * static_cast<size_t>(nobjs);
      return result;
    }

    size_t bytes_to_get = 2 * total_bytes + align_up(heap_size >> 4);

    if (bytes_left > 0) {
      const size_t leftover_index = size_class_index(bytes_left);
      freelist_push(&free_list[leftover_index],
                    reinterpret_cast<FreeNode*>(bump_start));
#if !LSTL_POOL_LIGHT
      if (SpanHeader* span = span_from_ptr(bump_start)) {
        ++span->block_count;
        ++span->free_count;
      }
      mark_bump_inactive();
#else
      bump_start = 0;
      bump_end = 0;
#endif
    }

#if LSTL_POOL_LIGHT
    char* raw = static_cast<char*>(malloc_alloc_t::allocate(bytes_to_get));
    if (!raw) {
      for (size_t i = block_size; i <= LSTL_POOL_MAX_BYTES; i += LSTL_ALIGN) {
        const size_t try_index = size_class_index(i);
        FreeNode* node = freelist_pop(&free_list[try_index]);
        if (node) {
          bump_start = reinterpret_cast<char*>(node);
          bump_end = bump_start + i;
          return chunk_alloc(block_size, nobjs, index);
        }
      }
      bump_start = 0;
      bump_end = 0;
      return 0;
    }
    heap_size += bytes_to_get;
    bump_start = raw;
    bump_end = raw + bytes_to_get;
    return chunk_alloc(block_size, nobjs, index);
#else
    if (bytes_to_get < LSTL_PAGE_SIZE) {
      bytes_to_get = LSTL_PAGE_SIZE;
    }

    const size_t aligned_size = align_up(bytes_to_get, LSTL_PAGE_SIZE);
    void* raw = 0;
    void* free_ptr = 0;
    size_t free_size = 0;
    if (!alloc_page(aligned_size, raw, free_ptr, free_size)) {
      mark_bump_inactive();
      return 0;
    }

    heap_size += free_size;
    SpanHeader* span = static_cast<SpanHeader*>(raw);
    init_span(span, index, static_cast<uint16_t>(block_size), free_ptr, free_size);
    span->flags |= 1u;
    span_registry().register_span(span);

    bump_start = span_user_base(span);
    bump_end = static_cast<char*>(free_ptr) + free_size;
    return chunk_alloc(block_size, nobjs, index);
#endif
  }

#if !LSTL_POOL_LIGHT
  void mark_bump_inactive() {
    if (bump_start) {
      if (SpanHeader* span = span_from_ptr(bump_start)) {
        span->flags &= ~1u;
      }
    }
    bump_start = 0;
    bump_end = 0;
  }

  static void init_span(SpanHeader* span, size_t size_class, uint16_t block_size,
                        void* base, size_t size) {
    std::memset(span, 0, sizeof(SpanHeader));
    span->magic = LSTL_SPAN_MAGIC;
    span->size_class = static_cast<uint16_t>(size_class);
    span->block_size = block_size;
    span->alloc_base = base;
    span->alloc_size = size;
  }

  static bool alloc_page(size_t span_bytes, void*& span_ptr, void*& free_ptr,
                         size_t& free_size) {
#if defined(__GLIBC__) || defined(__linux__)
    void* p = 0;
    if (posix_memalign(&p, LSTL_PAGE_SIZE, span_bytes) == 0 && p) {
      span_ptr = p;
      free_ptr = p;
      free_size = span_bytes;
      return true;
    }
#endif
    free_size = span_bytes + LSTL_PAGE_SIZE;
    void* raw = malloc_alloc_t::allocate(free_size);
    if (!raw) {
      return false;
    }
    span_ptr =
        reinterpret_cast<void*>(align_up(reinterpret_cast<uintptr_t>(raw), LSTL_PAGE_SIZE));
    free_ptr = raw;
    return true;
  }
#endif
};

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_MEMORY_POOL_SINGLE_H
