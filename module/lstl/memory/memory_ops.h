#ifndef LSTL_MEMORY_OPS_H
#define LSTL_MEMORY_OPS_H

#include <cstddef>

namespace lstl {

inline void* memcpy(void* dest, const void* src, size_t n) {
  char* d = static_cast<char*>(dest);
  const char* s = static_cast<const char*>(src);
  for (size_t i = 0; i < n; ++i) {
    d[i] = s[i];
  }
  return dest;
}

inline void* memset(void* s, int c, size_t n) {
  unsigned char* p = static_cast<unsigned char*>(s);
  const unsigned char v = static_cast<unsigned char>(c);
  for (size_t i = 0; i < n; ++i) {
    p[i] = v;
  }
  return s;
}

}  // namespace lstl

#endif  // LSTL_MEMORY_OPS_H
