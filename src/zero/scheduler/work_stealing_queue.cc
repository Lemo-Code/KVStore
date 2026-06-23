#include "zero/scheduler/work_stealing_queue.h"

#include <cmath>

namespace zero {

WorkStealingQueue::WorkStealingQueue(size_t capacity) {
    // 容量向上取整到 2 的幂
    size_t pow2 = 1;
    while (pow2 < capacity && pow2 < (1ul << 20)) {  // 上限 1M
        pow2 <<= 1;
    }
    capacity_ = pow2;
    mask_ = capacity_ - 1;
    buffer_.resize(capacity_);
    // top_.value 和 bottom_.value 已由构造函数初始化为 0
}

bool WorkStealingQueue::push(Fiber::ptr fiber) {
    int64_t b = bottom_.value.load(std::memory_order_relaxed);
    int64_t t = top_.value.load(std::memory_order_acquire);

    if (b - t >= static_cast<int64_t>(capacity_)) {
        return false;
    }

    buffer_[static_cast<size_t>(b & static_cast<int64_t>(mask_))] = std::move(fiber);
    std::atomic_thread_fence(std::memory_order_release);
    bottom_.value.store(b + 1, std::memory_order_relaxed);
    return true;
}

Fiber::ptr WorkStealingQueue::pop() {
    int64_t b = bottom_.value.load(std::memory_order_relaxed) - 1;
    bottom_.value.store(b, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);

    int64_t t = top_.value.load(std::memory_order_relaxed);

    if (t <= b) {
        Fiber::ptr fiber = std::move(buffer_[static_cast<size_t>(b & static_cast<int64_t>(mask_))]);

        if (t < b) {
            return fiber;
        }

        // t == b: 可能和 steal 竞争
        int64_t expected = t;
        if (!top_.value.compare_exchange_strong(expected, t + 1,
                                                std::memory_order_release,
                                                std::memory_order_relaxed)) {
            fiber.reset();
        }
        bottom_.value.store(t + 1, std::memory_order_relaxed);
        return fiber;
    }

    bottom_.value.store(t, std::memory_order_relaxed);
    return nullptr;
}

Fiber::ptr WorkStealingQueue::steal() {
    int64_t t = top_.value.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    int64_t b = bottom_.value.load(std::memory_order_acquire);

    if (t < b) {
        Fiber::ptr fiber = buffer_[static_cast<size_t>(t & static_cast<int64_t>(mask_))];

        if (!top_.value.compare_exchange_strong(t, t + 1,
                                                std::memory_order_release,
                                                std::memory_order_relaxed)) {
            return nullptr;
        }
        return fiber;
    }

    return nullptr;
}

bool WorkStealingQueue::empty() const {
    int64_t t = top_.value.load(std::memory_order_acquire);
    int64_t b = bottom_.value.load(std::memory_order_acquire);
    return b <= t;
}

size_t WorkStealingQueue::size() const {
    int64_t t = top_.value.load(std::memory_order_acquire);
    int64_t b = bottom_.value.load(std::memory_order_acquire);
    if (b <= t) return 0;
    return static_cast<size_t>(b - t);
}

} // namespace zero
