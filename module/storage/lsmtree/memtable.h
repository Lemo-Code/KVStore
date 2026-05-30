#ifndef LSTL_LSM_MEMTABLE_H
#define LSTL_LSM_MEMTABLE_H

#include "associative/skip_map.h"
#include "memory.h"

#include "record.h"

namespace lstl {
namespace lsm {

template <typename Key, typename T, typename Compare = lstl::less<Key> >
class MemTable {
 public:
  typedef Key key_type;
  typedef T mapped_type;
  typedef Record<T> record_type;
  typedef lstl::pair<const Key, record_type> value_type;
  typedef lstl::skip_map<Key, record_type, Compare> map_type;
  typedef typename map_type::iterator iterator;
  typedef typename map_type::const_iterator const_iterator;
  typedef typename map_type::size_type size_type;

  void put(const Key& k, const T& v) {
    typename map_type::iterator it = table_.find(k);
    if (it == table_.end()) {
      table_.insert(value_type(k, record_type(v)));
    } else {
      it->second.deleted = false;
      it->second.value = v;
    }
  }

  void erase(const Key& k) {
    typename map_type::iterator it = table_.find(k);
    if (it == table_.end()) {
      table_.insert(value_type(k, record_type::tombstone()));
    } else {
      it->second = record_type::tombstone();
    }
  }

  bool has_key(const Key& k) const { return table_.find(k) != table_.end(); }

  bool is_deleted(const Key& k) const {
    const_iterator it = table_.find(k);
    return it != table_.end() && it->second.deleted;
  }

  bool get(const Key& k, T* out) const {
    const_iterator it = table_.find(k);
    if (it == table_.end() || it->second.deleted) {
      return false;
    }
    *out = it->second.value;
    return true;
  }

  bool empty() const { return table_.empty(); }
  size_type size() const { return table_.size(); }

  const_iterator begin() const { return table_.begin(); }
  const_iterator end() const { return table_.end(); }

  const map_type& map() const { return table_; }
  void clear() { table_.clear(); }

 private:
  map_type table_;
};

}  // namespace lsm
}  // namespace lstl

#endif  // LSTL_LSM_MEMTABLE_H
