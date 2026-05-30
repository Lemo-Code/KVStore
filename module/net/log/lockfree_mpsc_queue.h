#ifndef NET_LOG_LOCKFREE_MPSC_QUEUE_H
#define NET_LOG_LOCKFREE_MPSC_QUEUE_H

#include <atomic>
#include <utility>

namespace net {

/**
 * @brief 无界 MPSC 无锁队列（net 异步日志唯一队列实现）。
 *
 * 仅允许一个消费者线程调用 try_pop / drain_all / empty。
 * 多线程消费须由 AsyncLogManager::drain_mtx_ 串行化。
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
    EnqueueNode(node);
  }

  void push(T&& value) {
    Node* node = new Node();
    node->value = std::move(value);
    EnqueueNode(node);
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

  template <typename Container>
  void drain_all(Container& out) {
    T item;
    while (try_pop(item)) {
      out.push_back(std::move(item));
    }
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

  void EnqueueNode(Node* node) {
    node->next.store(nullptr, std::memory_order_relaxed);
    Node* prev = tail_.exchange(node, std::memory_order_acq_rel);
    prev->next.store(node, std::memory_order_release);
  }

  std::atomic<Node*> head_;
  std::atomic<Node*> tail_;
};

}  // namespace net

#endif  // NET_LOG_LOCKFREE_MPSC_QUEUE_H
