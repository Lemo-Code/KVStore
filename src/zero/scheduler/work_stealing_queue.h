#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "zero/fiber/fiber.h"

namespace zero {

// ============ WorkStealingQueue ============
//
// Chase-Lev 双端队列 (lock-free, bounded)
//
// - Owner thread (唯一): push + pop (LIFO 端 = bottom)
// - Thief threads (多个): steal (FIFO 端 = top)
//
// 不变量:
//   top ≤ bottom  (top 可能短暂 > bottom 在 steal 竞争中)
//   0 ≤ top ≤ capacity
//   0 ≤ bottom ≤ capacity
//
// 参考:
//   "Correct and Efficient Work-Stealing for Weak Memory Models"
//   Nhat Minh Lê et al., PPoPP '13
class WorkStealingQueue {
public:
    static constexpr size_t DEFAULT_CAPACITY = 256;

    explicit WorkStealingQueue(size_t capacity = DEFAULT_CAPACITY);
    ~WorkStealingQueue() = default;

    // ---- Owner 操作 (仅拥有者线程调用) ----

    // 从 LIFO 端放入 (bottom)
    // 返回 false 表示队列满
    bool push(Fiber::ptr fiber);

    // 从 LIFO 端取出 (bottom)
    Fiber::ptr pop();

    // ---- Thief 操作 (任意线程调用) ----

    // 从 FIFO 端偷取 (top)
    Fiber::ptr steal();

    // ---- 查询 ----
    bool empty() const;
    size_t size() const;

private:
    // Cache-line padded to avoid false sharing
    struct alignas(64) TopSlot {
        std::atomic<int64_t> value{0};
    } top_;

    struct alignas(64) BottomSlot {
        std::atomic<int64_t> value{0};
    } bottom_;

    size_t capacity_;
    size_t mask_;                     // capacity_ - 1 (capacity_ 为 2 的幂)
    std::vector<Fiber::ptr> buffer_;  // capacity_ 大小
};

} // namespace zero
