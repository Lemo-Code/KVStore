#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <utility>

namespace ledis {

/**
 * @brief 有界 SPSC 无锁环形队列（每 IO 线程一条 Outbound 队列）。
 */
template <typename T, size_t Capacity>
class LockFreeSpscQueue {
 public:
  static_assert(Capacity >= 2, "Capacity must be >= 2");

  LockFreeSpscQueue() : head_(0), tail_(0) {}

  bool try_push(T value) {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t next = (tail + 1) % Capacity;
    if (next == head_.load(std::memory_order_acquire)) {
      return false;
    }
    slots_[tail] = std::move(value);
    tail_.store(next, std::memory_order_release);
    return true;
  }

  bool try_pop(T& out) {
    const size_t head = head_.load(std::memory_order_relaxed);
    if (head == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    out = std::move(slots_[head]);
    head_.store((head + 1) % Capacity, std::memory_order_release);
    return true;
  }

  bool empty() const {
    return head_.load(std::memory_order_acquire) ==
           tail_.load(std::memory_order_acquire);
  }

 private:
  std::atomic<size_t> head_;
  std::atomic<size_t> tail_;
  T slots_[Capacity];
};

}  // namespace ledis
