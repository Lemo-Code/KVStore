#ifndef NET_LOG_EVENT_STREAM_H
#define NET_LOG_EVENT_STREAM_H

#include "log/format_util.h"

#include <sstream>
#include <string>
#include <type_traits>

namespace net {

/** 将 << 追加到 std::string；常见类型走快速路径 */
class LogMessageStream {
 public:
  explicit LogMessageStream(std::string& out) : out_(out) {}

  LogMessageStream& operator<<(const char* s) {
    if (s) {
      out_ += s;
    }
    return *this;
  }

  LogMessageStream& operator<<(char c) {
    out_.push_back(c);
    return *this;
  }

  LogMessageStream& operator<<(const std::string& s) {
    out_ += s;
    return *this;
  }

  LogMessageStream& operator<<(std::string&& s) {
    out_ += std::move(s);
    return *this;
  }

  LogMessageStream& operator<<(int v) {
    format_util::AppendInt(out_, v);
    return *this;
  }

  LogMessageStream& operator<<(unsigned int v) {
    format_util::AppendUInt(out_, v);
    return *this;
  }

  LogMessageStream& operator<<(long v) {
    format_util::AppendInt(out_, v);
    return *this;
  }

  LogMessageStream& operator<<(unsigned long v) {
    format_util::AppendUInt(out_, v);
    return *this;
  }

  LogMessageStream& operator<<(long long v) {
    format_util::AppendInt(out_, v);
    return *this;
  }

  LogMessageStream& operator<<(unsigned long long v) {
    format_util::AppendUInt(out_, v);
    return *this;
  }

  template <typename T>
  typename std::enable_if<std::is_floating_point<T>::value, LogMessageStream&>::type
  operator<<(T v) {
    std::ostringstream oss;
    oss << v;
    out_ += oss.str();
    return *this;
  }

  template <typename T>
  typename std::enable_if<!std::is_floating_point<T>::value &&
                              !std::is_integral<T>::value &&
                              !std::is_same<T, char>::value &&
                              !std::is_same<T, const char*>::value &&
                              !std::is_same<T, std::string>::value,
                          LogMessageStream&>::type
  operator<<(const T& v) {
    std::ostringstream oss;
    oss << v;
    out_ += oss.str();
    return *this;
  }

 private:
  std::string& out_;
};

}  // namespace net

#endif  // NET_LOG_EVENT_STREAM_H
