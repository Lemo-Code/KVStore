#ifndef NET_LOG_LOCKFREE_MPSC_QUEUE_H
#define NET_LOG_LOCKFREE_MPSC_QUEUE_H

#include <atomic>
#include <utility>

namespace net {

/**
 * @brief 无界 MPSC（多生产者 / 单消费者）无锁队列。
 *
 * net 异步日志的唯一队列实现，不提供 mutex/deque 等替代方案。
 * 过载时在入队前降级丢弃，而非更换队列类型。
 *
 * 约束：仅允许一个消费者线程调用 try_pop / drain_all / empty。
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

  // 生产者：拷贝入队
  void push(const T& value) {
    Node* node = new Node();
    node->value = value;
    EnqueueNode(node);
  }

  // 生产者：移动入队
  void push(T&& value) {
    Node* node = new Node();
    node->value = std::move(value);
    EnqueueNode(node);
  }

  // 消费者：尝试弹出一条，成功返回 true
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

  // 消费者：弹出全部元素追加到 out
  template <typename Container>
  void drain_all(Container& out) {
    T item;
    while (try_pop(item)) {
      out.push_back(std::move(item));
    }
  }

  // 消费者：队列是否为空
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

  // 将新节点链接到链表尾部（仅生产者调用）
  void EnqueueNode(Node* node) {
    node->next.store(nullptr, std::memory_order_relaxed);
    Node* prev = tail_.exchange(node, std::memory_order_acq_rel);
    prev->next.store(node, std::memory_order_release);
  }

  std::atomic<Node*> head_;  // 消费者从 head_->next 取数据
  std::atomic<Node*> tail_;  // 生产者挂到 tail_
};

}  // namespace net

#endif  // NET_LOG_LOCKFREE_MPSC_QUEUE_H
