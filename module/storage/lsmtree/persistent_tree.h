#ifndef LSTL_LSM_PERSISTENT_TREE_H
#define LSTL_LSM_PERSISTENT_TREE_H

#include "disk_sstable.h"
#include "lsm_tree.h"
#include "manifest.h"
#include "wal.h"

#include <cstdio>
#include <string>
#include <vector>

namespace lstl {
namespace lsm {

template <typename Key, typename T, typename Compare = lstl::less<Key> >
class PersistentLsmTree {
 public:
  typedef LsmTree<Key, T, Compare> tree_type;
  typedef typename tree_type::table_type table_type;
  typedef DiskSstable<Key, T, Compare> disk_table_type;

  PersistentLsmTree(const std::string& dir,
                    typename tree_type::memtable_type::size_type memtable_limit = 64,
                    typename tree_type::level_type::size_type l0_file_limit = 4)
      : dir_(dir),
        tree_(memtable_limit, l0_file_limit),
        wal_(),
        manifest_(),
        disk_levels_(),
        wal_checkpoint_(0),
        persisted_l0_files_(0),
        opened_(false) {}

  ~PersistentLsmTree() { close(); }

  const std::string& directory() const { return dir_; }

  bool open(bool create_if_missing = true) {
    if (opened_) {
      return true;
    }
    if (!create_if_missing) {
      FILE* probe = std::fopen(manifest_path().c_str(), "rb");
      if (!probe) {
        return false;
      }
      std::fclose(probe);
    }

    if (!recover_from_manifest()) {
      return false;
    }
    if (!recover_from_wal()) {
      return false;
    }
    if (!wal_.open(wal_path(), !manifest_exists())) {
      return false;
    }
    persisted_l0_files_ = tree_.l0_file_count();
    opened_ = true;
    return true;
  }

  void close() {
    if (!opened_) {
      return;
    }
    flush();
    save_manifest();
    wal_.sync();
    wal_.close();
    opened_ = false;
  }

  void put(const Key& k, const T& v) {
    ensure_open();
    wal_.append_put(k, v);
    tree_.put(k, v);
  }

  void erase(const Key& k) {
    ensure_open();
    wal_.append_delete(k);
    tree_.erase(k);
  }

  bool get(const Key& k, T* out) const {
    if (tree_.get(k, out)) {
      return true;
    }
    for (int level = 0; level < static_cast<int>(disk_levels_.size()); ++level) {
      const lstl::vector<disk_table_type>& files =
          disk_levels_[static_cast<typename lstl::vector<disk_table_type>::size_type>(level)];
      for (typename lstl::vector<disk_table_type>::size_type i = files.size(); i > 0; --i) {
        const disk_table_type& file = files[i - 1];
        if (!file.may_contain(k)) {
          continue;
        }
        if (!file.has_key(k)) {
          continue;
        }
        if (file.is_deleted(k)) {
          return false;
        }
        return file.get(k, out);
      }
    }
    return false;
  }

  bool contains(const Key& k) const {
    T ignored;
    return get(k, &ignored);
  }

  void flush() {
    ensure_open();
    tree_.flush();
    persist_new_tables();
    save_manifest();
    wal_checkpoint_ = wal_.sequence();
    wal_.append_checkpoint(wal_checkpoint_);
    wal_.sync();
  }

  const tree_type& memory_tree() const { return tree_; }
  tree_type& memory_tree() { return tree_; }

  typename tree_type::level_type::size_type level_count() const { return tree_.level_count(); }

 private:
  std::string manifest_path() const { return detail::join_path(dir_, "MANIFEST"); }

  std::string wal_path() const { return detail::join_path(dir_, "wal.log"); }

  bool manifest_exists() const {
    FILE* fp = std::fopen(manifest_path().c_str(), "rb");
    if (!fp) {
      return false;
    }
    std::fclose(fp);
    return true;
  }

  void ensure_open() {
    if (!opened_) {
      open(true);
    }
  }

  bool recover_from_manifest() {
    if (!manifest_exists()) {
      return true;
    }
    if (!manifest_.load(manifest_path())) {
      return false;
    }
    wal_checkpoint_ = manifest_.wal_checkpoint();
    tree_.clear_all_levels();
    disk_levels_.clear();

    const Manifest::level_files_type& files = manifest_.levels();
    for (size_t level = 0; level < files.size(); ++level) {
      disk_levels_.push_back(lstl::vector<disk_table_type>());
      for (size_t i = 0; i < files[level].size(); ++i) {
        const std::string full_path = detail::join_path(dir_, files[level][i]);
        disk_table_type disk_table(full_path);
        if (!disk_table.load()) {
          return false;
        }
        disk_levels_[level].push_back(disk_table);
        tree_.append_level_table(static_cast<int>(level), disk_table.table());
      }
    }
    persisted_l0_files_ = tree_.l0_file_count();
    return true;
  }

  struct WalRecoveryHandler {
    tree_type* tree;
    void on_put(const Key& k, const T& v) { tree->apply_put(k, v); }
    void on_delete(const Key& k) { tree->apply_erase(k); }
    void on_checkpoint(uint64_t) {}
  };

  bool recover_from_wal() {
    WalRecoveryHandler handler;
    handler.tree = &tree_;
    return WalReplayer<Key, T, WalRecoveryHandler>::replay(wal_path(), wal_checkpoint_, &handler);
  }

  void persist_new_tables() {
    while (tree_.l0_file_count() > persisted_l0_files_) {
      const table_type& src = tree_.level_table(0, persisted_l0_files_);
      const uint64_t file_id = manifest_.allocate_file_id();
      const std::string name = manifest_.make_sstable_name(file_id);
      const std::string full_path = detail::join_path(dir_, name);
      if (!disk_table_type::write_file(full_path, src)) {
        return;
      }
      disk_table_type disk_table(full_path);
      if (!disk_table.load()) {
        return;
      }
      if (manifest_.levels().empty()) {
        manifest_.levels().push_back(std::vector<std::string>());
      }
      manifest_.levels()[0].push_back(name);
      if (disk_levels_.empty()) {
        disk_levels_.push_back(lstl::vector<disk_table_type>());
      }
      disk_levels_[0].push_back(disk_table);
      ++persisted_l0_files_;
    }
  }

  void save_manifest() {
    manifest_.set_wal_checkpoint(wal_checkpoint_);
    manifest_.save(manifest_path());
  }

  std::string dir_;
  tree_type tree_;
  WalWriter<Key, T> wal_;
  Manifest manifest_;
  lstl::vector<lstl::vector<disk_table_type> > disk_levels_;
  uint64_t wal_checkpoint_;
  typename tree_type::level_type::size_type persisted_l0_files_;
  bool opened_;
};

}  // namespace lsm
}  // namespace lstl

#endif  // LSTL_LSM_PERSISTENT_TREE_H
