#ifndef LSTL_UNORDERED_MULTISET_H
#define LSTL_UNORDERED_MULTISET_H

#include "detail/hashtable.h"

namespace lstl {

template <typename Key, typename Hash = hash<Key>, typename Pred = equal_to<Key>,
          typename Alloc = allocator<Key> >
class unordered_multiset {
 public:
  typedef Key key_type;
  typedef Key value_type;
  typedef Hash hasher;
  typedef Pred key_equal;
  typedef Alloc allocator_type;

 protected:
  typedef detail::hashtable<value_type, hasher, key_equal, Alloc> rep_type;
  rep_type table_;

 public:
  typedef typename rep_type::iterator iterator;
  typedef typename rep_type::const_iterator const_iterator;
  typedef typename rep_type::size_type size_type;

  unordered_multiset(size_type n = 0, const Hash& hf = Hash(), const Pred& eq = Pred())
      : table_(n, hf, eq) {}

  template <typename InputIterator>
  unordered_multiset(InputIterator first, InputIterator last, size_type n = 0,
                     const Hash& hf = Hash(), const Pred& eq = Pred())
      : table_(n, hf, eq) {
    table_.insert_equal(first, last);
  }

  iterator begin() { return table_.begin(); }
  const_iterator begin() const { return table_.begin(); }
  iterator end() { return table_.end(); }
  const_iterator end() const { return table_.end(); }

  size_type size() const { return table_.size(); }
  bool empty() const { return table_.empty(); }

  iterator insert(const value_type& obj) { return table_.insert_equal(obj); }

  template <typename InputIterator>
  void insert(InputIterator first, InputIterator last) {
    table_.insert_equal(first, last);
  }

  iterator find(const key_type& k) { return table_.find(k); }
  size_type count(const key_type& k) const {
    size_type n = 0;
    for (iterator it = table_.find(k); it != end() && table_.key_eq()(*it, k); it = ++it) {
      ++n;
    }
    return n;
  }

  size_type erase(const key_type& k) { return table_.erase(k); }
  void clear() { table_.clear(); }
  void swap(unordered_multiset& other) { table_.swap(other.table_); }
};

}  // namespace lstl

#endif  // LSTL_UNORDERED_MULTISET_H
