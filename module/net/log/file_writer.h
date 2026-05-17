#ifndef NET_LOG_FILE_WRITER_H
#define NET_LOG_FILE_WRITER_H

#include "log/byte_buffer.h"

#include <cstddef>
#include <string>

namespace net {

/**
 * @brief 单路径异步写盘：长连接 fd + 固定容量 ByteBuffer 聚合 write。
 *
 * 仅允许后台刷盘线程访问（由 AsyncLogManager 持有）。
 */
class FileWriter {
 public:
  explicit FileWriter(size_t buf_capacity, size_t flush_threshold);

  ~FileWriter();

  FileWriter(const FileWriter&) = delete;
  FileWriter& operator=(const FileWriter&) = delete;

  const std::string& path() const { return path_; }

  /** 确保 fd 已打开（路径变化时 reopen） */
  bool open(const std::string& path);

  /** 追加字节；缓冲满则自动 flush_buffer */
  void append(const char* data, size_t len);
  void append(const std::string& s) { append(s.data(), s.size()); }

  /** 将缓冲区内数据 write 到 fd */
  void flush_buffer();

  /** flush_buffer + fdatasync（进程退出等场景） */
  void flush_all();

  void close();

 private:
  void append_large(const char* data, size_t len);

  std::string path_;
  int fd_;
  ByteBuffer buf_;
  size_t flush_threshold_;
};

/**
 * @brief 标准输出聚合写（无 fd，缓冲满或 flush 时一次性 cout）。
 */
class StdoutWriter {
 public:
  explicit StdoutWriter(size_t buf_capacity, size_t flush_threshold);

  void append(const char* data, size_t len);
  void append(const std::string& s) { append(s.data(), s.size()); }

  void flush_buffer();

 private:
  ByteBuffer buf_;
  size_t flush_threshold_;
};

}  // namespace net

#endif  // NET_LOG_FILE_WRITER_H
