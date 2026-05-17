#ifndef KV_POOL_ATOMIC_FREELIST_H
#define KV_POOL_ATOMIC_FREELIST_H

#include <atomic>

#include "freelist.h"

namespace kv {
namespace detail {

class AtomicFreelist {
 public:
  AtomicFreelist() : head_(0) {}

  FreeNode* pop_all() { return head_.exchange(0, std::memory_order_acquire); }

  void push_all(FreeNode* head, FreeNode* tail) {
    if (!head) {
      return;
    }
    FreeNode* old = head_.load(std::memory_order_relaxed);
    do {
      tail->next = old;
    } while (!head_.compare_exchange_weak(old, head, std::memory_order_release,
                                          std::memory_order_relaxed));
  }

  void push_one(FreeNode* node) {
    if (!node) {
      return;
    }
    FreeNode* old = head_.load(std::memory_order_relaxed);
    do {
      node->next = old;
    } while (!head_.compare_exchange_weak(old, node, std::memory_order_release,
                                          std::memory_order_relaxed));
  }

 private:
  std::atomic<FreeNode*> head_;
};

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_ATOMIC_FREELIST_H
