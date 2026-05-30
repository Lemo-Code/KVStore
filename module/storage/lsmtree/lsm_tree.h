#ifndef LSTL_LSM_TREE_H
#define LSTL_LSM_TREE_H

#include "memory.h"
#include "sequence/vector.h"

#include "memtable.h"
#include "sorted_table.h"

namespace lstl {
namespace lsm {

template <typename Key, typename T, typename Compare = lstl::less<Key> >
class LsmTree {
 public:
  typedef Key key_type;
  typedef T mapped_type;
  typedef Record<T> record_type;
  typedef MemTable<Key, T, Compare> memtable_type;
  typedef SortedTable<Key, T, Compare> table_type;
  typedef lstl::vector<table_type> level_type;
  typedef lstl::pair<Key, T> value_type;

  class const_iterator {
   public:
    const_iterator() : entries_(0), pos_(0) {}

    const value_type& operator*() const { return (*entries_)[pos_]; }
    const value_type* operator->() const { return &(*entries_)[pos_]; }

    const_iterator& operator++() {
      ++pos_;
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++pos_;
      return tmp;
    }

    bool operator==(const const_iterator& o) const {
      return entries_ == o.entries_ && pos_ == o.pos_;
    }

    bool operator!=(const const_iterator& o) const { return !(*this == o); }

   private:
    friend class LsmTree;
    const_iterator(const lstl::vector<value_type>* entries, typename lstl::vector<value_type>::size_type pos)
        : entries_(entries), pos_(pos) {}

    const lstl::vector<value_type>* entries_;
    typename lstl::vector<value_type>::size_type pos_;
  };

  explicit LsmTree(typename memtable_type::size_type memtable_limit = 64,
                   typename level_type::size_type l0_file_limit = 4)
      : active_(),
        imm_(),
        has_imm_(false),
        memtable_limit_(memtable_limit),
        l0_file_limit_(l0_file_limit),
        levels_(),
        snapshot_(),
        snapshot_built_(false) {}

  void put(const Key& k, const T& v) {
    maybe_rotate_imm();
    active_.put(k, v);
    invalidate_snapshot();
    maybe_flush_memtable();
  }

  void erase(const Key& k) {
    maybe_rotate_imm();
    active_.erase(k);
    invalidate_snapshot();
    maybe_flush_memtable();
  }

  bool get(const Key& k, T* out) const {
    if (active_.has_key(k)) {
      if (active_.is_deleted(k)) {
        return false;
      }
      return active_.get(k, out);
    }
    if (has_imm_) {
      if (imm_.has_key(k)) {
        if (imm_.is_deleted(k)) {
          return false;
        }
        return imm_.get(k, out);
      }
    }
    for (int level = 0; level < static_cast<int>(levels_.size()); ++level) {
      const level_type& files = levels_[static_cast<typename level_type::size_type>(level)];
      for (typename level_type::size_type i = files.size(); i > 0; --i) {
        const table_type& file = files[i - 1];
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

  typename memtable_type::size_type memtable_size() const { return active_.size(); }

  typename level_type::size_type level_count() const { return levels_.size(); }

  typename level_type::size_type level_file_count(typename level_type::size_type level) const {
    if (level >= levels_.size()) {
      return 0;
    }
    return levels_[level].size();
  }

  typename level_type::size_type l0_file_count() const { return level_file_count(0); }

  void flush() {
    if (active_.empty()) {
      return;
    }
    maybe_rotate_imm();
    flush_active_to_l0();
  }

  void compact_l0() { compact_level(0); }

  void compact_all() {
    for (typename level_type::size_type level = 0; level < levels_.size(); ++level) {
      if (levels_[level].size() >= 2) {
        compact_level(static_cast<int>(level));
      }
    }
  }

  const_iterator begin() const {
    ensure_snapshot();
    return const_iterator(&snapshot_, 0);
  }

  const_iterator end() const {
    ensure_snapshot();
    return const_iterator(&snapshot_, snapshot_.size());
  }

  typename lstl::vector<value_type>::size_type live_size() const {
    ensure_snapshot();
    return snapshot_.size();
  }

  // 持久化恢复：仅写 memtable，不触发 flush
  void apply_put(const Key& k, const T& v) {
    active_.put(k, v);
    invalidate_snapshot();
  }

  void apply_erase(const Key& k) {
    active_.erase(k);
    invalidate_snapshot();
  }

  void clear_all_levels() {
    levels_.clear();
    invalidate_snapshot();
  }

  void append_level_table(typename level_type::size_type level, const table_type& table) {
    while (levels_.size() <= level) {
      levels_.push_back(level_type());
    }
    levels_[level].push_back(table);
    invalidate_snapshot();
  }

  const table_type& level_table(typename level_type::size_type level,
                                typename level_type::size_type index) const {
    return levels_[level][index];
  }

 private:
  void invalidate_snapshot() {
    snapshot_.clear();
    snapshot_built_ = false;
  }

  void ensure_snapshot() const {
    if (snapshot_built_) {
      return;
    }
    build_snapshot();
    snapshot_built_ = true;
  }

  void build_snapshot() const {
    lstl::skip_map<Key, record_type, Compare> merged;
    for (int level = static_cast<int>(levels_.size()) - 1; level >= 0; --level) {
      const level_type& files = levels_[static_cast<typename level_type::size_type>(level)];
      for (typename level_type::size_type i = 0; i < files.size(); ++i) {
        apply_table(files[i], &merged);
      }
    }
    if (has_imm_) {
      apply_memtable(imm_, &merged);
    }
    apply_memtable(active_, &merged);

    snapshot_.clear();
    for (typename lstl::skip_map<Key, record_type, Compare>::const_iterator it = merged.begin();
         it != merged.end(); ++it) {
      if (!it->second.deleted) {
        snapshot_.push_back(value_type(it->first, it->second.value));
      }
    }
  }

  template <typename Map>
  void apply_memtable(const Map& src, lstl::skip_map<Key, record_type, Compare>* merged) const {
    for (typename Map::const_iterator it = src.begin(); it != src.end(); ++it) {
      typename lstl::skip_map<Key, record_type, Compare>::iterator found = merged->find(it->first);
      if (found == merged->end()) {
        merged->insert(typename lstl::pair<const Key, record_type>(it->first, it->second));
      } else {
        found->second = it->second;
      }
    }
  }

  void apply_table(const table_type& src, lstl::skip_map<Key, record_type, Compare>* merged) const {
    for (typename table_type::const_iterator it = src.begin(); it != src.end(); ++it) {
      typename lstl::skip_map<Key, record_type, Compare>::iterator found = merged->find(it->first);
      if (found == merged->end()) {
        merged->insert(typename lstl::pair<const Key, record_type>(it->first, it->second));
      } else {
        found->second = it->second;
      }
    }
  }

  typename level_type::size_type level_file_limit(typename level_type::size_type level) const {
    if (level == 0) {
      return l0_file_limit_;
    }
    return l0_file_limit_ * (level + 1);
  }

  void maybe_rotate_imm() {
    if (has_imm_) {
      flush_imm_to_l0();
    }
  }

  void maybe_flush_memtable() {
    if (active_.size() >= memtable_limit_) {
      flush_active_to_l0();
    }
    maybe_compact_level(0);
  }

  void flush_active_to_l0() {
    if (active_.empty()) {
      return;
    }
    if (has_imm_) {
      flush_imm_to_l0();
    }
    imm_ = active_;
    active_.clear();
    has_imm_ = true;
    flush_imm_to_l0();
  }

  void flush_imm_to_l0() {
    if (!has_imm_ || imm_.empty()) {
      has_imm_ = false;
      return;
    }
    if (levels_.empty()) {
      levels_.push_back(level_type());
    }
    levels_[0].push_back(table_type(imm_.map()));
    imm_.clear();
    has_imm_ = false;
    invalidate_snapshot();
    maybe_compact_level(0);
  }

  void maybe_compact_level(typename level_type::size_type level) {
    if (level >= levels_.size()) {
      return;
    }
    if (levels_[level].size() < level_file_limit(level)) {
      return;
    }
    compact_level(static_cast<int>(level));
    maybe_compact_level(level + 1);
  }

  void compact_level(int level) {
    if (level < 0 || level >= static_cast<int>(levels_.size())) {
      return;
    }
    level_type& files = levels_[static_cast<typename level_type::size_type>(level)];
    if (files.empty()) {
      return;
    }

    lstl::skip_map<Key, record_type, Compare> merged;
    for (typename level_type::size_type i = 0; i < files.size(); ++i) {
      for (typename table_type::const_iterator it = files[i].begin(); it != files[i].end(); ++it) {
        typename lstl::skip_map<Key, record_type, Compare>::iterator found = merged.find(it->first);
        if (found == merged.end()) {
          merged.insert(typename lstl::pair<const Key, record_type>(it->first, it->second));
        } else {
          found->second = it->second;
        }
      }
    }

    files.clear();
    while (levels_.size() <= static_cast<typename level_type::size_type>(level + 1)) {
      levels_.push_back(level_type());
    }
    levels_[static_cast<typename level_type::size_type>(level + 1)].push_back(table_type(merged));
    invalidate_snapshot();
  }

  memtable_type active_;
  memtable_type imm_;
  bool has_imm_;
  typename memtable_type::size_type memtable_limit_;
  typename level_type::size_type l0_file_limit_;
  lstl::vector<level_type> levels_;
  mutable lstl::vector<value_type> snapshot_;
  mutable bool snapshot_built_;
};

}  // namespace lsm
}  // namespace lstl

#endif  // LSTL_LSM_TREE_H
