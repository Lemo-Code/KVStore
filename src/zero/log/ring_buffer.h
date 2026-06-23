#pragma once

#include <atomic>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace zero {

// ============ RingBuffer (lock-free SPSC) ============
//
// 单生产者-单消费者环形缓冲, 无锁设计
// 参考 LMAX Disruptor pattern
//
// @tparam T  条目类型
// @tparam Capacity  容量 (必须是 2 的幂)
template<typename T, size_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static constexpr size_t kMask = Capacity - 1;

public:
    RingBuffer() : buffer_(Capacity) {
        write_cursor_.store(0, std::memory_order_relaxed);
        read_cursor_.store(0, std::memory_order_relaxed);
    }

    // ---- Producer API ----

    // 申请一个写入槽位, 返回 nullptr 表示满
    T* tryClaim(size_t& index) {
        size_t w = write_cursor_.load(std::memory_order_relaxed);
        size_t r = read_cursor_.load(std::memory_order_acquire);
        if (w - r >= Capacity) return nullptr;  // 满
        index = w & kMask;
        return &buffer_[index];
    }

    // 提交写入 (调用 tryClaim 成功后调用)
    void commit(size_t index) {
        (void)index;
        write_cursor_.fetch_add(1, std::memory_order_release);
    }

    // ---- Consumer API ----

    // 批量读取, 返回可读条目数
    size_t tryReadBatch(T*& out, size_t max_count) {
        size_t r = read_cursor_.load(std::memory_order_relaxed);
        size_t w = write_cursor_.load(std::memory_order_acquire);
        size_t avail = w - r;
        if (avail == 0) return 0;

        size_t count = std::min(avail, max_count);
        out = &buffer_[r & kMask];
        return count;
    }

    // 提交读取 (调用 tryReadBatch 成功后调用)
    void commitRead(size_t count) {
        read_cursor_.fetch_add(count, std::memory_order_release);
    }

    // 当前可读数量
    size_t available() const {
        size_t w = write_cursor_.load(std::memory_order_acquire);
        size_t r = read_cursor_.load(std::memory_order_relaxed);
        return w >= r ? w - r : 0;
    }

    bool empty() const { return available() == 0; }
    size_t capacity() const { return Capacity; }

private:
    // Cache-line padding 避免 false sharing
    alignas(64) std::atomic<size_t> write_cursor_;
    alignas(64) std::atomic<size_t> read_cursor_;
    std::vector<T> buffer_;
};

} // namespace zero
