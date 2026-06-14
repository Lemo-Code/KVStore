#pragma once

#include "lemo/utils/noncopyable.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sys/types.h>

namespace lemo::buffer {

struct BufferRegion;

/**
 * @brief 链式字节缓冲区，面向长连接流式协议解析（如 Redis RESP）。
 *
 * 与 RingBuffer（单块环形）不同，ChainBuffer 按固定大小 chunk 链表追加数据：
 * - 已解析前缀 consume 后释放空 chunk，长连接上不会整段 compact
 * - 未收齐的半包自然留在链尾，无需 shift
 * - readable_regions / readFd / writeFd 对接 scatter-gather IO
 *
 * 单连接单实例，无内置锁。
 */
class ChainBuffer : lemo::utils::NonCopyable {
 public:
  static constexpr size_t kDefaultChunkSize = 4096;
  static constexpr size_t kMaxChunkSize = 64 * 1024;
  static constexpr size_t kMaxRegions = 32;

  explicit ChainBuffer(size_t chunk_size = kDefaultChunkSize);
  ~ChainBuffer();

  ChainBuffer(ChainBuffer&& other) noexcept;
  ChainBuffer& operator=(ChainBuffer&& other) noexcept;

  size_t chunkSize() const { return chunk_size_; }
  size_t readable() const { return total_readable_; }
  size_t writable() const;
  bool empty() const { return total_readable_ == 0; }

  /** 追加数据到链尾，必要时分配新 chunk。 */
  size_t append(const void* src, size_t len);
  /** 读出并消费 len 字节。 */
  size_t read(void* dst, size_t len);
  /** 窥视（不消费）。 */
  size_t peek(void* dst, size_t len) const;
  /** 消费已解析前缀。 */
  void consume(size_t len);

  /**
   * 填充可读区视图（跨 chunk，最多 max_regions 段）。
   * @return 实际段数
   */
  size_t readable_regions(BufferRegion* out, size_t max_regions,
                          size_t skip = 0) const;

  /** 从 skip 起第一段的连续可读指针与长度。 */
  const uint8_t* peek_ptr(size_t skip = 0) const;
  size_t peek_contiguous(size_t skip = 0) const;

  /** 从 fd 读入并 append 到链尾。 */
  ssize_t readFd(int fd, size_t max_bytes = SIZE_MAX,
                 int* saved_errno = nullptr);
  /** 向 fd 写出可读前缀并 consume。 */
  ssize_t writeFd(int fd, size_t max_bytes = SIZE_MAX,
                  int* saved_errno = nullptr);

  const uint8_t* find_byte(uint8_t byte, size_t skip = 0) const;
  const uint8_t* find(const void* pattern, size_t pattern_len,
                      size_t skip = 0) const;

  void clear();
  /** 保证链尾至少 min_writable 字节可写空间。 */
  void reserve(size_t min_writable);
  /** 释放已读空的 head chunk（consume 后自动调用，也可手动）。 */
  void trim();

 private:
  struct Chunk {
    std::unique_ptr<uint8_t[]> data;
    size_t capacity = 0;
    size_t read_pos = 0;
    size_t write_pos = 0;
    std::unique_ptr<Chunk> next;
  };

  Chunk* tailChunk();
  const Chunk* headChunk() const;
  void ensureWritable(size_t min_writable);
  Chunk* allocChunk(size_t capacity);

  size_t chunk_size_ = kDefaultChunkSize;
  size_t total_readable_ = 0;
  std::unique_ptr<Chunk> head_;
  Chunk* tail_ = nullptr;
};

}  // namespace lemo::buffer
