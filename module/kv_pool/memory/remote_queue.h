#ifndef KV_POOL_REMOTE_QUEUE_H
#define KV_POOL_REMOTE_QUEUE_H

#include <atomic>
#include <cstdint>

#include "../config.h"

namespace kv {
namespace detail {

struct RemoteItem {
  void* ptr;
  uint16_t size_class;
  uint16_t pad;
};

struct RemoteBatch {
  RemoteBatch* next;
  uint32_t count;
  RemoteItem items[KV_POOL_REMOTE_BATCH];
};

class RemoteFreeQueue {
 public:
  RemoteFreeQueue() : head_(0) {}

  void push(RemoteBatch* batch) {
    RemoteBatch* old = head_.load(std::memory_order_relaxed);
    do {
      batch->next = old;
    } while (!head_.compare_exchange_weak(old, batch, std::memory_order_release,
                                          std::memory_order_relaxed));
  }

  RemoteBatch* drain_all() { return head_.exchange(0, std::memory_order_acquire); }

 private:
  std::atomic<RemoteBatch*> head_;
};

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_REMOTE_QUEUE_H
