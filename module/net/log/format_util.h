#ifndef NET_LOG_FORMAT_UTIL_H
#define NET_LOG_FORMAT_UTIL_H

#include <cstdint>
#include <cstring>
#include <string>

namespace net {
namespace format_util {

inline void AppendInt(std::string& out, int64_t v) {
  if (v == 0) {
    out.push_back('0');
    return;
  }
  if (v < 0) {
    out.push_back('-');
    v = -v;
  }
  char buf[24];
  size_t n = 0;
  while (v > 0) {
    buf[n++] = static_cast<char>('0' + (v % 10));
    v /= 10;
  }
  while (n > 0) {
    out.push_back(buf[--n]);
  }
}

inline void AppendUInt(std::string& out, uint64_t v) {
  if (v == 0) {
    out.push_back('0');
    return;
  }
  char buf[24];
  size_t n = 0;
  while (v > 0) {
    buf[n++] = static_cast<char>('0' + (v % 10));
    v /= 10;
  }
  while (n > 0) {
    out.push_back(buf[--n]);
  }
}

}  // namespace format_util
}  // namespace net

#endif  // NET_LOG_FORMAT_UTIL_H
