#pragma once


#include "lemo/utils/noncopyable.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sys/types.h>

namespace lemo::buffer {

/** 连续内存片段，用于 readv/writev scatter-gather。 */
struct BufferRegion {
  uint8_t* data = nullptr;
  size_t len = 0;
};

/**
 * @brief 工业级字节环形缓冲区（kfifo 风格）。
 *
 * 设计要点：
 * - 容量为 2 的幂，下标用 mask 映射，O(1) 寻址
 * - head/tail 为单调递增序号，可读 = tail - head，无空满歧义
 * - 环回时最多 2 段连续区，可直接对接 readv/writev
 * - 单线程所有权（每连接一个实例），无内置锁
 *
 * 非 Sylar/muduo 线性 Buffer，是真正环形存储。
 */
class RingBuffer : lemo::utils::NonCopyable {
 public:
  static constexpr size_t kDefaultCapacity = 4096;
  static constexpr size_t kMaxCapacity = 16 * 1024 * 1024;

  explicit RingBuffer(size_t capacity = kDefaultCapacity);
  ~RingBuffer();

  RingBuffer(RingBuffer&& other) noexcept;
  RingBuffer& operator=(RingBuffer&& other) noexcept;

  size_t capacity() const { return capacity_; }
  size_t readable() const { return static_cast<size_t>(tail_ - head_); }
  size_t writable() const { return capacity_ - readable(); }
  bool empty() const { return head_ == tail_; }
  bool full() const { return readable() == capacity_; }

  /** 读出并消费 len 字节，返回实际读出数。 */
  size_t read(void* dst, size_t len);
  /** 写入 len 字节，返回实际写入数。 */
  size_t write(const void* src, size_t len);
  /** 窥视（不消费），返回实际窥视数。 */
  size_t peek(void* dst, size_t len) const;
  /** 消费已读数据。 */
  void consume(size_t len);
  /** 提交已写入数据（配合 writable_regions 原地写）。 */
  void commit(size_t len);

  /**
   * 填充可读区视图，最多 2 段（环回）。
   * @return 实际段数（0~2）
   */
  size_t readable_regions(BufferRegion* out, size_t max_regions,
                          size_t skip = 0) const;

  /**
   * 填充可写区视图，最多 2 段（环回）。
   * @return 实际段数（0~2）
   */
  size_t writable_regions(BufferRegion* out, size_t max_regions) const;

  /** 从 skip 起第一段的连续可读指针与长度。 */
  const uint8_t* peek_ptr(size_t skip = 0) const;
  size_t peek_contiguous(size_t skip = 0) const;

  /** 从 fd 读入，最多填满可写区或 max_bytes。 */
  ssize_t readFd(int fd, size_t max_bytes = SIZE_MAX,
                 int* saved_errno = nullptr);
  /** 向 fd 写出，最多写出 readable 或 max_bytes。 */
  ssize_t writeFd(int fd, size_t max_bytes = SIZE_MAX,
                  int* saved_errno = nullptr);

  const uint8_t* find_byte(uint8_t byte, size_t skip = 0) const;
  const uint8_t* find(const void* pattern, size_t pattern_len,
                      size_t skip = 0) const;

  void clear();
  /** 保证至少 min_writable 字节可写空间（必要时 compact / grow）。 */
  void reserve(size_t min_writable);

 private:
  static size_t roundUpPow2(size_t n);
  size_t offset(uint64_t seq) const { return static_cast<size_t>(seq & mask_); }

  void compact();
  void grow(size_t min_writable);
  void resetStorage(size_t new_capacity);

  uint64_t head_ = 0;
  uint64_t tail_ = 0;
  size_t capacity_ = 0;
  size_t mask_ = 0;
  std::unique_ptr<uint8_t[]> storage_;
};

}  // namespace lemo::buffer


