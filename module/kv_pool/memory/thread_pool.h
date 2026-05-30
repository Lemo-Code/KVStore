#ifndef KV_POOL_THREAD_POOL_H
#define KV_POOL_THREAD_POOL_H

#include <atomic>
#include <cstdint>
#include <cstring>

#include "../config.h"
#include "arena.h"
#include "chunk.h"
#include "freelist.h"
#include "malloc_alloc.h"
#include "size_class.h"

namespace kv {
namespace detail {

inline uint16_t current_thread_tag() {
  static thread_local uint16_t tag = 0;
  if (tag == 0) {
    static std::atomic<uint32_t> next{1};
    tag = static_cast<uint16_t>(next.fetch_add(1, std::memory_order_relaxed) & 0xFFFFu);
    if (tag == 0) {
      tag = 1;
    }
  }
  return tag;
}

// 布局对齐 lstl PoolSingle：free_list 置首，同线程热路径 freelist + bump + malloc chunk。
class ThreadLocalPool {
 public:
  FreeNode* free_list_[KV_POOL_FREELISTS];

  ThreadLocalPool()
      : bump_start_(0),
        bump_end_(0),
        heap_size_(0),
        has_local_range_(false),
        local_min_(0),
        local_max_(0),
        arena_(0),
        arena_id_(0),
        thread_tag_(0),
        inited_(false),
        pending_registry_head_(0) {
    std::memset(free_list_, 0, sizeof(free_list_));
  }

  void init(uint32_t arena_id, uint16_t thread_tag, Arena* arena) {
    arena_id_ = arena_id;
    thread_tag_ = thread_tag;
    arena_ = arena;
    inited_ = true;
  }

  bool inited() const { return inited_; }

  Arena* arena() const { return arena_; }

  uint32_t arena_id() const { return arena_id_; }

  uint16_t thread_tag() const { return thread_tag_; }

  bool owns_pointer(const void* p) const {
    if (!has_local_range_ || !p) {
      return false;
    }
    const char* cp = static_cast<const char*>(p);
    return cp >= local_min_ && cp < local_max_;
  }

  void* refill_on_miss(size_t index, size_t block_size) { return refill(index, block_size); }

  void push(size_t index, FreeNode* node) { freelist_push(&free_list_[index], node); }

  void register_pending_chunks() {
    if (!arena_ || !pending_registry_head_) {
      return;
    }
    ChunkHeader* chunk = pending_registry_head_;
    pending_registry_head_ = 0;
    while (chunk) {
      ChunkHeader* next = chunk->next_registry;
      chunk->next_registry = 0;
      arena_->register_chunk(chunk);
      chunk = next;
    }
  }

  void flush_all() {
    register_pending_chunks();
    if (bump_start_ && bump_end_ > bump_start_) {
      const size_t leftover = static_cast<size_t>(bump_end_ - bump_start_);
      if (leftover <= KV_POOL_MAX_BYTES) {
        const size_t leftover_index = size_class_index(leftover);
        freelist_push(&free_list_[leftover_index], reinterpret_cast<FreeNode*>(bump_start_));
      }
      bump_start_ = 0;
      bump_end_ = 0;
    }
    for (size_t i = 0; i < KV_POOL_FREELISTS; ++i) {
      flush_bin(i);
    }
  }

 private:
  void note_range(char* start, char* end) {
    if (!start || !end || end <= start) {
      return;
    }
    if (!has_local_range_ || start < local_min_) {
      local_min_ = start;
    }
    if (!has_local_range_ || end > local_max_) {
      local_max_ = end;
    }
    has_local_range_ = true;
  }

  void adopt_pointer(const void* p) {
    if (!arena_) {
      return;
    }
    ChunkHeader* header = arena_->find_chunk(p);
    if (header) {
      note_range(header->user_start, header->user_end);
    }
  }

  void* refill(size_t index, size_t block_size) {
    FreeNode* batch = arena_->withdraw_bin(index);
    if (batch) {
      adopt_pointer(batch);
      FreeNode* node = batch;
      batch = batch->next;
      if (batch) {
        FreeNode* tail = batch;
        while (tail->next) {
          tail = tail->next;
        }
        freelist_push_list(&free_list_[index], batch, tail);
      }
      return node;
    }

    int nobjs = KV_POOL_REFILL_BATCH;
    char* chunk = chunk_alloc(block_size, nobjs, index);
    if (!chunk) {
      return 0;
    }

    if (nobjs == 1) {
      return chunk;
    }

    FreeNode* result = reinterpret_cast<FreeNode*>(chunk);
    freelist_push(&free_list_[index], reinterpret_cast<FreeNode*>(chunk + block_size));

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
        bump_end_ > bump_start_ ? static_cast<size_t>(bump_end_ - bump_start_) : 0;

    if (bytes_left >= total_bytes) {
      char* result = bump_start_;
      bump_start_ += total_bytes;
      return result;
    }
    if (bytes_left >= block_size) {
      nobjs = static_cast<int>(bytes_left / block_size);
      char* result = bump_start_;
      bump_start_ += block_size * static_cast<size_t>(nobjs);
      return result;
    }

    if (bytes_left > 0) {
      if (bytes_left <= KV_POOL_MAX_BYTES) {
        const size_t leftover_index = size_class_index(bytes_left);
        freelist_push(&free_list_[leftover_index], reinterpret_cast<FreeNode*>(bump_start_));
      }
      bump_start_ = 0;
      bump_end_ = 0;
    }

    size_t bytes_to_get = 2 * total_bytes + align_up(heap_size_ >> 4);
    const size_t raw_bytes = bytes_to_get + sizeof(ChunkHeader);
    char* raw = static_cast<char*>(malloc_alloc::allocate(raw_bytes));
    if (!raw) {
      for (size_t try_bytes = block_size; try_bytes <= KV_POOL_MAX_BYTES;
           try_bytes += KV_POOL_ALIGN) {
        const size_t try_index = size_class_index(try_bytes);
        FreeNode* node = freelist_pop(&free_list_[try_index]);
        if (node) {
          bump_start_ = reinterpret_cast<char*>(node);
          bump_end_ = bump_start_ + try_bytes;
          return chunk_alloc(block_size, nobjs, index);
        }
      }
      return 0;
    }

    ChunkHeader* header = reinterpret_cast<ChunkHeader*>(raw + bytes_to_get);
    header->next_registry = pending_registry_head_;
    pending_registry_head_ = header;
    header->magic = KV_CHUNK_MAGIC;
    header->arena_id = static_cast<uint16_t>(arena_id_);
    header->thread_tag = thread_tag_;
    header->user_start = raw;
    header->user_end = raw + bytes_to_get;

    heap_size_ += bytes_to_get;
    note_range(header->user_start, header->user_end);

    bump_start_ = header->user_start;
    bump_end_ = header->user_end;
    return chunk_alloc(block_size, nobjs, index);
  }

  void flush_bin(size_t index) {
    if (!free_list_[index]) {
      return;
    }
    FreeNode* head = free_list_[index];
    FreeNode* tail = head;
    while (tail->next) {
      tail = tail->next;
    }
    arena_->deposit_bin(index, head, tail);
    free_list_[index] = 0;
  }

  char* bump_start_;
  char* bump_end_;
  size_t heap_size_;
  bool has_local_range_;
  char* local_min_;
  char* local_max_;
  Arena* arena_;
  uint32_t arena_id_;
  uint16_t thread_tag_;
  bool inited_;
  ChunkHeader* pending_registry_head_;
};

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_THREAD_POOL_H
