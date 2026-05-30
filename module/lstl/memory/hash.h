#ifndef LSTL_HASH_H
#define LSTL_HASH_H

#include <cstddef>
#include <cstring>

namespace lstl {

inline size_t hash_bytes(const void* ptr, size_t len) {
  const unsigned char* p = static_cast<const unsigned char*>(ptr);
  size_t h = 0;
  for (size_t i = 0; i < len; ++i) {
    h = h * 131 + p[i];
  }
  return h;
}

template <typename Key>
struct hash {
  size_t operator()(const Key& key) const {
    return hash_bytes(&key, sizeof(key));
  }
};

template <>
struct hash<bool> {
  size_t operator()(bool v) const { return v ? 1 : 0; }
};

template <>
struct hash<char> {
  size_t operator()(char v) const { return static_cast<size_t>(v); }
};

template <>
struct hash<signed char> {
  size_t operator()(signed char v) const { return static_cast<size_t>(v); }
};

template <>
struct hash<unsigned char> {
  size_t operator()(unsigned char v) const { return static_cast<size_t>(v); }
};

template <>
struct hash<short> {
  size_t operator()(short v) const { return static_cast<size_t>(v); }
};

template <>
struct hash<unsigned short> {
  size_t operator()(unsigned short v) const { return static_cast<size_t>(v); }
};

template <>
struct hash<int> {
  size_t operator()(int v) const { return static_cast<size_t>(v); }
};

template <>
struct hash<unsigned int> {
  size_t operator()(unsigned int v) const { return static_cast<size_t>(v); }
};

template <>
struct hash<long> {
  size_t operator()(long v) const { return static_cast<size_t>(v); }
};

template <>
struct hash<unsigned long> {
  size_t operator()(unsigned long v) const { return static_cast<size_t>(v); }
};

template <>
struct hash<long long> {
  size_t operator()(long long v) const { return static_cast<size_t>(v); }
};

template <>
struct hash<unsigned long long> {
  size_t operator()(unsigned long long v) const { return static_cast<size_t>(v); }
};

template <typename T>
struct hash<T*> {
  size_t operator()(T* p) const {
    return hash_bytes(&p, sizeof(p));
  }
};

template <>
struct hash<const char*> {
  size_t operator()(const char* s) const {
    size_t h = 0;
    if (s) {
      for (; *s; ++s) {
        h = h * 131 + static_cast<unsigned char>(*s);
      }
    }
    return h;
  }
};

}  // namespace lstl

#endif  // LSTL_HASH_H
