#ifndef LSTL_LSM_WAL_H
#define LSTL_LSM_WAL_H

#include "detail/io.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace lstl {
namespace lsm {

enum WalOp : unsigned char {
  kWalPut = 1,
  kWalDelete = 2,
  kWalCheckpoint = 3
};

template <typename Key, typename T>
class WalWriter {
 public:
  WalWriter() : fp_(0), path_(), seq_(0) {}

  ~WalWriter() { close(); }

  bool open(const std::string& path, bool truncate) {
    close();
    path_ = path;
    const char* mode = truncate ? "wb" : "ab";
    fp_ = std::fopen(path.c_str(), mode);
    if (!fp_) {
      return false;
    }
    if (truncate) {
      if (!detail::write_magic(fp_, detail::wal_magic())) {
        close();
        return false;
      }
      seq_ = 0;
      if (std::fflush(fp_) != 0) {
        close();
        return false;
      }
    } else {
      seq_ = file_record_count(path);
    }
    return true;
  }

  void close() {
    if (fp_) {
      std::fflush(fp_);
      std::fclose(fp_);
      fp_ = 0;
    }
  }

  bool append_put(const Key& k, const T& v) {
    return append_record(kWalPut, k, v);
  }

  bool append_delete(const Key& k) {
    T dummy = T();
    return append_record(kWalDelete, k, dummy);
  }

  bool append_checkpoint(uint64_t checkpoint_seq) {
    if (!fp_) {
      return false;
    }
    const WalOp op = kWalCheckpoint;
    if (!detail::write_pod(fp_, op)) {
      return false;
    }
    if (!detail::write_pod(fp_, checkpoint_seq)) {
      return false;
    }
    ++seq_;
    return true;
  }

  bool sync() {
    return fp_ && std::fflush(fp_) == 0;
  }

  uint64_t sequence() const { return seq_; }

  const std::string& path() const { return path_; }

 private:
  bool append_record(WalOp op, const Key& k, const T& v) {
    if (!fp_) {
      return false;
    }
    if (!detail::write_pod(fp_, op)) {
      return false;
    }
    if (!detail::write_pod(fp_, k)) {
      return false;
    }
    if (!detail::write_pod(fp_, v)) {
      return false;
    }
    ++seq_;
    return true;
  }

  static uint64_t file_record_count(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
      return 0;
    }
    if (!detail::read_magic(fp, detail::wal_magic())) {
      std::fclose(fp);
      return 0;
    }
    uint64_t count = 0;
    for (;;) {
      WalOp op = kWalPut;
      if (!detail::read_pod(fp, &op)) {
        break;
      }
      if (op == kWalCheckpoint) {
        uint64_t ignored = 0;
        if (!detail::read_pod(fp, &ignored)) {
          break;
        }
      } else {
        Key k = Key();
        T v = T();
        if (!detail::read_pod(fp, &k) || !detail::read_pod(fp, &v)) {
          break;
        }
      }
      ++count;
    }
    std::fclose(fp);
    return count;
  }

  FILE* fp_;
  std::string path_;
  uint64_t seq_;
};

template <typename Key, typename T, typename Handler>
class WalReplayer {
 public:
  static bool replay(const std::string& path, uint64_t skip_after_seq, Handler* handler) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
      return true;
    }
    if (!detail::read_magic(fp, detail::wal_magic())) {
      std::fclose(fp);
      return false;
    }
    uint64_t seq = 0;
    bool ok = true;
    while (ok) {
      WalOp op = kWalPut;
      if (!detail::read_pod(fp, &op)) {
        break;
      }
      ++seq;
      if (seq <= skip_after_seq) {
        if (op == kWalCheckpoint) {
          uint64_t ignored = 0;
          ok = detail::read_pod(fp, &ignored);
        } else {
          Key k = Key();
          T v = T();
          ok = detail::read_pod(fp, &k) && detail::read_pod(fp, &v);
        }
        continue;
      }
      if (op == kWalPut) {
        Key k = Key();
        T v = T();
        ok = detail::read_pod(fp, &k) && detail::read_pod(fp, &v);
        if (ok) {
          handler->on_put(k, v);
        }
      } else if (op == kWalDelete) {
        Key k = Key();
        T v = T();
        ok = detail::read_pod(fp, &k) && detail::read_pod(fp, &v);
        if (ok) {
          handler->on_delete(k);
        }
      } else if (op == kWalCheckpoint) {
        uint64_t cp = 0;
        ok = detail::read_pod(fp, &cp);
        if (ok) {
          handler->on_checkpoint(cp);
        }
      } else {
        ok = false;
      }
    }
    std::fclose(fp);
    return ok;
  }
};

}  // namespace lsm
}  // namespace lstl

#endif  // LSTL_LSM_WAL_H
