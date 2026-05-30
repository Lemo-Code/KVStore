#ifndef LSTL_LSM_DISK_SSTABLE_H
#define LSTL_LSM_DISK_SSTABLE_H

#include "bloom_filter.h"
#include "detail/io.h"
#include "sorted_table.h"

#include <cstdio>
#include <string>

namespace lstl {
namespace lsm {

template <typename Key, typename T, typename Compare = lstl::less<Key> >
class DiskSstable {
 public:
  typedef SortedTable<Key, T, Compare> table_type;
  typedef typename table_type::value_type value_type;

  DiskSstable() : path_(), table_(), bloom_(), loaded_(false) {}

  explicit DiskSstable(const std::string& path) : path_(path), table_(), bloom_(), loaded_(false) {}

  const std::string& path() const { return path_; }

  bool loaded() const { return loaded_; }

  const BloomFilter& bloom() const { return bloom_; }

  const table_type& table() const { return table_; }

  static bool write_file(const std::string& path, const table_type& src) {
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
      return false;
    }

    BloomFilter bloom;
    bloom.init(src.size());
    for (typename table_type::const_iterator it = src.begin(); it != src.end(); ++it) {
      bloom.add(it->first);
    }

    const uint32_t version = 1;
    const uint32_t entry_count = static_cast<uint32_t>(src.size());
    const uint32_t bloom_bytes = static_cast<uint32_t>(bloom.data_size());
    const uint32_t bloom_bits = static_cast<uint32_t>(bloom.bit_count());
    const uint32_t bloom_hashes = static_cast<uint32_t>(bloom.hash_count());

    bool ok = detail::write_magic(fp, detail::sst_magic()) && detail::write_pod(fp, version) &&
              detail::write_pod(fp, entry_count) && detail::write_pod(fp, bloom_bits) &&
              detail::write_pod(fp, bloom_hashes) && detail::write_pod(fp, bloom_bytes);

    if (ok && bloom_bytes > 0) {
      ok = detail::write_bytes(fp, bloom.data(), bloom_bytes);
    }

    for (typename table_type::const_iterator it = src.begin(); ok && it != src.end(); ++it) {
      const unsigned char deleted = it->second.deleted ? 1 : 0;
      ok = detail::write_pod(fp, it->first) && detail::write_pod(fp, deleted) &&
           detail::write_pod(fp, it->second.value);
    }

    if (ok) {
      ok = (std::fflush(fp) == 0);
    }
    std::fclose(fp);
    return ok;
  }

  bool load() {
    if (path_.empty()) {
      return false;
    }
    FILE* fp = std::fopen(path_.c_str(), "rb");
    if (!fp) {
      return false;
    }

    uint32_t version = 0;
    uint32_t entry_count = 0;
    uint32_t bloom_bits = 0;
    uint32_t bloom_hashes = 0;
    uint32_t bloom_bytes = 0;

    bool ok = detail::read_magic(fp, detail::sst_magic()) && detail::read_pod(fp, &version) &&
              detail::read_pod(fp, &entry_count) && detail::read_pod(fp, &bloom_bits) &&
              detail::read_pod(fp, &bloom_hashes) && detail::read_pod(fp, &bloom_bytes);

    lstl::vector<unsigned char> bloom_data;
    if (ok && bloom_bytes > 0) {
      bloom_data.resize(bloom_bytes);
      ok = detail::read_bytes(fp, &bloom_data[0], bloom_bytes);
    }

    lstl::vector<value_type> entries;
    entries.reserve(entry_count);
    for (uint32_t i = 0; ok && i < entry_count; ++i) {
      Key k = Key();
      unsigned char deleted = 0;
      T v = T();
      ok = detail::read_pod(fp, &k) && detail::read_pod(fp, &deleted) && detail::read_pod(fp, &v);
      if (ok) {
        Record<T> rec;
        rec.deleted = (deleted != 0);
        rec.value = v;
        entries.push_back(value_type(k, rec));
      }
    }

    std::fclose(fp);
    if (!ok) {
      return false;
    }

    bloom_.load(bloom_data.empty() ? 0 : &bloom_data[0], bloom_data.size(), bloom_bits, bloom_hashes);
    table_ = table_type(entries.begin(), entries.end());
    loaded_ = true;
    return true;
  }

  bool may_contain(const Key& k) const {
    if (!loaded_) {
      return true;
    }
    return bloom_.may_contain(k);
  }

  bool get(const Key& k, T* out) const {
    if (!loaded_) {
      return false;
    }
    if (!may_contain(k)) {
      return false;
    }
    return table_.get(k, out);
  }

  bool has_key(const Key& k) const { return loaded_ && table_.has_key(k); }

  bool is_deleted(const Key& k) const { return loaded_ && table_.is_deleted(k); }

 private:
  std::string path_;
  table_type table_;
  BloomFilter bloom_;
  bool loaded_;
};

}  // namespace lsm
}  // namespace lstl

#endif  // LSTL_LSM_DISK_SSTABLE_H
