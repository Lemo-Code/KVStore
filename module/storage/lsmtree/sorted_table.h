#ifndef LSTL_LSM_SORTED_TABLE_H
#define LSTL_LSM_SORTED_TABLE_H

#include "associative/skip_map.h"
#include "memory.h"
#include "sequence/vector.h"

#include "record.h"

namespace lstl {
namespace lsm {

template <typename Key, typename T, typename Compare = lstl::less<Key> >
class SortedTable {
 public:
  typedef Key key_type;
  typedef T mapped_type;
  typedef lstl::pair<Key, Record<T> > value_type;
  typedef Compare key_compare;
  typedef lstl::vector<value_type> storage_type;
  typedef typename storage_type::const_iterator const_iterator;

  SortedTable() : entries_(), comp_() {}

  template <typename InputIterator>
  SortedTable(InputIterator first, InputIterator last, const Compare& comp = Compare())
      : entries_(), comp_(comp) {
    build_from_range(first, last);
  }

  template <typename Map>
  explicit SortedTable(const Map& source, const Compare& comp = Compare()) : entries_(), comp_(comp) {
    build_from_map(source);
  }

  bool empty() const { return entries_.empty(); }
  typename storage_type::size_type size() const { return entries_.size(); }

  const_iterator begin() const { return entries_.begin(); }
  const_iterator end() const { return entries_.end(); }

  bool get(const Key& k, T* out) const {
    const_iterator it = find_key(k);
    if (it == entries_.end() || it->second.deleted) {
      return false;
    }
    *out = it->second.value;
    return true;
  }

  bool contains_live(const Key& k) const {
    const_iterator it = find_key(k);
    return it != entries_.end() && !it->second.deleted;
  }

  bool has_key(const Key& k) const { return find_key(k) != entries_.end(); }

  bool is_deleted(const Key& k) const {
    const_iterator it = find_key(k);
    return it != entries_.end() && it->second.deleted;
  }

 private:
  template <typename InputIterator>
  void build_from_range(InputIterator first, InputIterator last) {
    for (; first != last; ++first) {
      entries_.push_back(value_type(first->first, first->second));
    }
    sort_and_dedupe();
  }

  template <typename Map>
  void build_from_map(const Map& source) {
    for (typename Map::const_iterator it = source.begin(); it != source.end(); ++it) {
      entries_.push_back(value_type(it->first, it->second));
    }
    sort_and_dedupe();
  }

  void sort_and_dedupe() {
    if (entries_.empty()) {
      return;
    }
    lstl::sort(entries_.begin(), entries_.end(), value_less(comp_));
    typename storage_type::size_type w = 1;
    for (typename storage_type::size_type r = 1; r < entries_.size(); ++r) {
      if (comp_(entries_[r].first, entries_[w - 1].first) ||
          comp_(entries_[w - 1].first, entries_[r].first)) {
        entries_[w++] = entries_[r];
      } else {
        entries_[w - 1] = entries_[r];
      }
    }
    entries_.resize(w);
  }

  const_iterator find_key(const Key& k) const {
    typename storage_type::size_type lo = 0;
    typename storage_type::size_type hi = entries_.size();
    while (lo < hi) {
      const typename storage_type::size_type mid = lo + (hi - lo) / 2;
      if (comp_(entries_[mid].first, k)) {
        lo = mid + 1;
      } else if (comp_(k, entries_[mid].first)) {
        hi = mid;
      } else {
        return entries_.begin() + static_cast<typename storage_type::difference_type>(mid);
      }
    }
    return entries_.end();
  }

  struct value_less {
    Compare comp;
    explicit value_less(const Compare& c) : comp(c) {}
    bool operator()(const value_type& a, const value_type& b) const { return comp(a.first, b.first); }
  };

  storage_type entries_;
  Compare comp_;
};

}  // namespace lsm
}  // namespace lstl

#endif  // LSTL_LSM_SORTED_TABLE_H
