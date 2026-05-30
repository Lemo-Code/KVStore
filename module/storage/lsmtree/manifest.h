#ifndef LSTL_LSM_MANIFEST_H
#define LSTL_LSM_MANIFEST_H

#include "detail/io.h"

#include <cstdio>
#include <string>
#include <vector>

namespace lstl {
namespace lsm {

class Manifest {
 public:
  typedef std::vector<std::vector<std::string> > level_files_type;

  Manifest() : next_file_id_(1), wal_checkpoint_(0), levels_() {}

  uint64_t next_file_id() const { return next_file_id_; }
  uint64_t wal_checkpoint() const { return wal_checkpoint_; }
  const level_files_type& levels() const { return levels_; }
  level_files_type& levels() { return levels_; }

  void set_wal_checkpoint(uint64_t seq) { wal_checkpoint_ = seq; }

  uint64_t allocate_file_id() { return next_file_id_++; }

  std::string make_sstable_name(uint64_t file_id) const {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "sst-%llu.sst",
                  static_cast<unsigned long long>(file_id));
    return std::string(buf);
  }

  bool save(const std::string& path) const {
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
      return false;
    }
    const uint32_t version = 1;
    const uint32_t level_count = static_cast<uint32_t>(levels_.size());
    bool ok = detail::write_magic(fp, detail::manifest_magic()) && detail::write_pod(fp, version) &&
              detail::write_pod(fp, next_file_id_) && detail::write_pod(fp, wal_checkpoint_) &&
              detail::write_pod(fp, level_count);

    for (uint32_t level = 0; ok && level < level_count; ++level) {
      const uint32_t file_count = static_cast<uint32_t>(levels_[level].size());
      ok = detail::write_pod(fp, file_count);
      for (uint32_t i = 0; ok && i < file_count; ++i) {
        const std::string& name = levels_[level][i];
        const uint32_t name_len = static_cast<uint32_t>(name.size());
        ok = detail::write_pod(fp, name_len);
        if (ok && name_len > 0) {
          ok = detail::write_bytes(fp, name.data(), name.size());
        }
      }
    }

    if (ok) {
      ok = (std::fflush(fp) == 0);
    }
    std::fclose(fp);
    return ok;
  }

  bool load(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
      return false;
    }

    uint32_t version = 0;
    uint32_t level_count = 0;
    bool ok = detail::read_magic(fp, detail::manifest_magic()) && detail::read_pod(fp, &version) &&
              detail::read_pod(fp, &next_file_id_) && detail::read_pod(fp, &wal_checkpoint_) &&
              detail::read_pod(fp, &level_count);

    levels_.clear();
    for (uint32_t level = 0; ok && level < level_count; ++level) {
      uint32_t file_count = 0;
      ok = detail::read_pod(fp, &file_count);
      std::vector<std::string> level_files;
      for (uint32_t i = 0; ok && i < file_count; ++i) {
        uint32_t name_len = 0;
        ok = detail::read_pod(fp, &name_len);
        std::string name;
        if (ok && name_len > 0) {
          name.resize(name_len);
          ok = detail::read_bytes(fp, &name[0], name_len);
        }
        if (ok) {
          level_files.push_back(name);
        }
      }
      if (ok) {
        levels_.push_back(level_files);
      }
    }

    std::fclose(fp);
    return ok;
  }

 private:
  uint64_t next_file_id_;
  uint64_t wal_checkpoint_;
  level_files_type levels_;
};

}  // namespace lsm
}  // namespace lstl

#endif  // LSTL_LSM_MANIFEST_H
