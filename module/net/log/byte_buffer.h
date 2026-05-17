#ifndef NET_LOG_BYTE_BUFFER_H
#define NET_LOG_BYTE_BUFFER_H

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace net {

/**
 * @brief 固定容量字节缓冲（构造时一次分配，运行期不扩容）。
 *
 * 用于异步刷盘线程按块聚合 write；满时由 FileWriter 先落盘再 clear。
 */
class ByteBuffer {
 public:
  explicit ByteBuffer(size_t capacity);

  size_t capacity() const { return storage_.size(); }
  size_t size() const { return used_; }
  bool empty() const { return used_ == 0; }

  const char* data() const { return storage_.data(); }
  void clear() { used_ = 0; }

  /** 若 used + len 超过 capacity 返回 false（调用方应先 flush） */
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

}  // namespace net

#endif  // NET_LOG_BYTE_BUFFER_H
