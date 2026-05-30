#ifndef LSTL_LSM_DETAIL_IO_H
#define LSTL_LSM_DETAIL_IO_H

#include <cstdio>
#include <cstring>
#include <string>

namespace lstl {
namespace lsm {
namespace detail {

inline const char* wal_magic() { return "WAL1"; }
inline const char* sst_magic() { return "SST1"; }
inline const char* manifest_magic() { return "MAN1"; }

inline bool write_bytes(FILE* fp, const void* data, size_t n) {
  return std::fwrite(data, 1, n, fp) == n;
}

inline bool read_bytes(FILE* fp, void* data, size_t n) {
  return std::fread(data, 1, n, fp) == n;
}

template <typename T>
inline bool write_pod(FILE* fp, const T& v) {
  return write_bytes(fp, &v, sizeof(T));
}

template <typename T>
inline bool read_pod(FILE* fp, T* v) {
  return read_bytes(fp, v, sizeof(T));
}

inline bool write_magic(FILE* fp, const char* magic) {
  return write_bytes(fp, magic, 4);
}

inline bool read_magic(FILE* fp, const char* expected) {
  char buf[4] = {0};
  if (!read_bytes(fp, buf, 4)) {
    return false;
  }
  return std::memcmp(buf, expected, 4) == 0;
}

inline std::string join_path(const std::string& dir, const std::string& name) {
  if (dir.empty()) {
    return name;
  }
  if (dir[dir.size() - 1] == '/') {
    return dir + name;
  }
  return dir + "/" + name;
}

}  // namespace detail
}  // namespace lsm
}  // namespace lstl

#endif  // LSTL_LSM_DETAIL_IO_H
