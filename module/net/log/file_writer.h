#ifndef NET_LOG_FILE_WRITER_H
#define NET_LOG_FILE_WRITER_H

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace net {

/** 固定容量字节缓冲（构造时一次分配，运行期不扩容） */
class ByteBuffer {
 public:
  explicit ByteBuffer(size_t capacity);

  size_t capacity() const { return storage_.size(); }
  size_t size() const { return used_; }
  bool empty() const { return used_ == 0; }
  const char* data() const { return storage_.data(); }
  void clear() { used_ = 0; }

  bool append(const char* data, size_t len);
  bool append(const std::string& s) { return append(s.data(), s.size()); }
  bool would_overflow(size_t len) const { return used_ + len > storage_.size(); }

 private:
  std::vector<char> storage_;
  size_t used_;
};

inline ByteBuffer::ByteBuffer(size_t capacity) : used_(0) {
  storage_.resize(capacity > 0 ? capacity : 4096);
}

inline bool ByteBuffer::append(const char* data, size_t len) {
  if (len == 0) {
    return true;
  }
  if (would_overflow(len)) {
    return false;
  }
  std::memcpy(storage_.data() + used_, data, len);
  used_ += len;
  return true;
}

/** 单路径：长连接 fd + ByteBuffer 批量 write */
class FileWriter {
 public:
  FileWriter(size_t buf_capacity, size_t flush_threshold);
  ~FileWriter();

  FileWriter(const FileWriter&) = delete;
  FileWriter& operator=(const FileWriter&) = delete;

  const std::string& path() const { return path_; }
  bool open(const std::string& path);
  void append(const char* data, size_t len);
  void append(const std::string& s) { append(s.data(), s.size()); }
  void flush_buffer();
  void close();

 private:
  void append_large(const char* data, size_t len);

  std::string path_;
  int fd_;
  ByteBuffer buf_;
  size_t flush_threshold_;
};

/** 标准输出聚合写 */
class StdoutWriter {
 public:
  StdoutWriter(size_t buf_capacity, size_t flush_threshold);

  void append(const char* data, size_t len);
  void append(const std::string& s) { append(s.data(), s.size()); }
  void flush_buffer();

 private:
  ByteBuffer buf_;
  size_t flush_threshold_;
};

}  // namespace net

#endif  // NET_LOG_FILE_WRITER_H
