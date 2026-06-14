#ifndef LEMO_FIBER_RUN_QUEUE_H
#define LEMO_FIBER_RUN_QUEUE_H

#include "lemo/fiber/fiber.h"
#include "lemo/thread/mutex.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <utility>

namespace lemo {
namespace fiber {

struct ScheduleTask {
  Fiber::ptr fiber;
  std::function<void()> cb;
  int thread = -1;

  ScheduleTask() = default;

  ScheduleTask(Fiber::ptr f, int thr) : fiber(std::move(f)), thread(thr) {}

  ScheduleTask(Fiber::ptr* f, int thr) : thread(thr) {
    if (f != nullptr) {
      fiber.swap(*f);
    }
  }

  ScheduleTask(std::function<void()> c, int thr)
      : cb(std::move(c)), thread(thr) {}

  ScheduleTask(std::function<void()>* c, int thr) : thread(thr) {
    if (c != nullptr) {
      cb.swap(*c);
    }
  }

  bool valid() const { return fiber != nullptr || static_cast<bool>(cb); }

  void reset() {
    fiber.reset();
    cb = nullptr;
    thread = -1;
  }
};

/**
 * @brief Go P.runq 风格本地环形队列。
 *
 * - pushBack 仅在成功时 move 任务
 * - 满时由调度器 drainHalf 到全局队列
 * - 所有者 popFront，其他 P stealBack
 */
class LocalRunQueue {
 public:
  static constexpr uint32_t kCapacity = 256;

  struct PushResult {
    bool ok;
    bool was_empty;
    PushResult() : ok(false), was_empty(false) {}
    PushResult(bool o, bool e) : ok(o), was_empty(e) {}
  };

  bool empty() const {
    thread::Mutex::Lock lock(mutex_);
    return countLocked() == 0;
  }

  size_t size() const {
    thread::Mutex::Lock lock(mutex_);
    return countLocked();
  }

  PushResult pushBack(ScheduleTask& task) {
    thread::Mutex::Lock lock(mutex_);
    if (countLocked() >= kCapacity) {
      return PushResult();
    }
    const bool was_empty = (countLocked() == 0);
    ring_[tail_ % kCapacity] = std::move(task);
    ++tail_;
    return PushResult(true, was_empty);
  }

  bool popFront(ScheduleTask& task) {
    thread::Mutex::Lock lock(mutex_);
    if (countLocked() == 0) {
      return false;
    }
    task = std::move(ring_[head_ % kCapacity]);
    ring_[head_ % kCapacity].reset();
    ++head_;
    return true;
  }

  bool stealBack(ScheduleTask& task) {
    thread::Mutex::Lock lock(mutex_);
    if (countLocked() == 0) {
      return false;
    }
    --tail_;
    task = std::move(ring_[tail_ % kCapacity]);
    ring_[tail_ % kCapacity].reset();
    return true;
  }

  void drainHalfTo(std::deque<ScheduleTask>& overflow) {
    thread::Mutex::Lock lock(mutex_);
    const uint32_t count = countLocked();
    if (count < 2) {
      return;
    }
    const uint32_t n = count / 2;
    for (uint32_t i = 0; i < n; ++i) {
      --tail_;
      overflow.push_back(std::move(ring_[tail_ % kCapacity]));
      ring_[tail_ % kCapacity].reset();
    }
  }

 private:
  uint32_t countLocked() const { return tail_ - head_; }

  mutable thread::Mutex mutex_;
  std::array<ScheduleTask, kCapacity> ring_{};
  uint32_t head_ = 0;
  uint32_t tail_ = 0;
};

class GlobalRunQueue {
 public:
  static constexpr size_t kMaxBatchPop = 32;

  bool empty() const {
    thread::Mutex::Lock lock(mutex_);
    return queue_.empty();
  }

  size_t size() const {
    thread::Mutex::Lock lock(mutex_);
    return queue_.size();
  }

  void push(ScheduleTask task) {
    thread::Mutex::Lock lock(mutex_);
    queue_.push_back(std::move(task));
  }

  void pushBatch(std::deque<ScheduleTask>& batch) {
    if (batch.empty()) {
      return;
    }
    thread::Mutex::Lock lock(mutex_);
    for (auto& item : batch) {
      queue_.push_back(std::move(item));
    }
    batch.clear();
  }

  bool popFront(ScheduleTask& task) {
    thread::Mutex::Lock lock(mutex_);
    if (queue_.empty()) {
      return false;
    }
    task = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }

  size_t popBatch(std::deque<ScheduleTask>& batch, size_t max_count) {
    thread::Mutex::Lock lock(mutex_);
    size_t n = 0;
    while (n < max_count && !queue_.empty()) {
      batch.push_back(std::move(queue_.front()));
      queue_.pop_front();
      ++n;
    }
    return n;
  }

 private:
  mutable thread::Mutex mutex_;
  std::deque<ScheduleTask> queue_;
};

}  // namespace fiber
}  // namespace lemo

#endif  // LEMO_FIBER_RUN_QUEUE_H
