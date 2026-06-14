#pragma once

#include <atomic>
#include <utility>

namespace ledis {

/**
 * @brief 无界 MPSC 无锁队列（Ledis InboundQueue 底层）。
 *
 * 仅允许一个消费者线程调用 try_pop。
 */
template <typename T>
class LockFreeMpscQueue {
 public:
  LockFreeMpscQueue() {
    Node* stub = new Node();
    head_.store(stub, std::memory_order_relaxed);
    tail_.store(stub, std::memory_order_relaxed);
  }

  ~LockFreeMpscQueue() {
    T unused;
    while (try_pop(unused)) {
    }
    Node* stub = head_.load(std::memory_order_relaxed);
    delete stub;
  }

  LockFreeMpscQueue(const LockFreeMpscQueue&) = delete;
  LockFreeMpscQueue& operator=(const LockFreeMpscQueue&) = delete;

  void push(const T& value) {
    Node* node = new Node();
    node->value = value;
    enqueueNode(node);
  }

  void push(T&& value) {
    Node* node = new Node();
    node->value = std::move(value);
    enqueueNode(node);
  }

  bool try_pop(T& out) {
    Node* head = head_.load(std::memory_order_relaxed);
    Node* next = head->next.load(std::memory_order_acquire);
    if (next == nullptr) {
      return false;
    }
    out = std::move(next->value);
    head_.store(next, std::memory_order_release);
    delete head;
    return true;
  }

  bool empty() const {
    const Node* head = head_.load(std::memory_order_relaxed);
    return head->next.load(std::memory_order_acquire) == nullptr;
  }

 private:
  struct Node {
    std::atomic<Node*> next;
    T value;
    Node() : next(nullptr) {}
  };

  void enqueueNode(Node* node) {
    node->next.store(nullptr, std::memory_order_relaxed);
    Node* prev = tail_.exchange(node, std::memory_order_acq_rel);
    prev->next.store(node, std::memory_order_release);
  }

  std::atomic<Node*> head_;
  std::atomic<Node*> tail_;
};

}  // namespace ledis
