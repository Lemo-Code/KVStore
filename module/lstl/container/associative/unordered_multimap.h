#ifndef LSTL_UNORDERED_MULTIMAP_H
#define LSTL_UNORDERED_MULTIMAP_H

#include "detail/hash_functors.h"
#include "detail/hashtable.h"

namespace lstl {

template <typename Key, typename T, typename Hash = hash<Key>, typename Pred = equal_to<Key>,
          typename Alloc = allocator<pair<const Key, T> > >
class unordered_multimap {
 public:
  typedef Key key_type;
  typedef T mapped_type;
  typedef pair<const Key, T> value_type;
  typedef Hash hasher;
  typedef Pred key_equal;
  typedef Alloc allocator_type;

 protected:
  typedef detail::hashtable_hash_key<key_type, hasher> table_hash;
  typedef detail::hashtable_equal_key<key_type, key_equal> table_pred;
  typedef detail::hashtable<value_type, table_hash, table_pred, Alloc> rep_type;
  rep_type table_;

  value_type make_probe(const key_type& k) const { return value_type(k, mapped_type()); }

 public:
  typedef typename rep_type::iterator iterator;
  typedef typename rep_type::const_iterator const_iterator;
  typedef typename rep_type::size_type size_type;

  unordered_multimap(size_type n = 0, const Hash& hf = Hash(), const Pred& eq = Pred())
      : table_(n, table_hash(hf), table_pred(eq)) {}

  template <typename InputIterator>
  unordered_multimap(InputIterator first, InputIterator last, size_type n = 0,
                     const Hash& hf = Hash(), const Pred& eq = Pred())
      : table_(n, table_hash(hf), table_pred(eq)) {
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

  iterator find(const key_type& k) { return table_.find(make_probe(k)); }
  size_type erase(const key_type& k) { return table_.erase(make_probe(k)); }
  void clear() { table_.clear(); }
  void swap(unordered_multimap& other) { table_.swap(other.table_); }
};

}  // namespace lstl

#endif  // LSTL_UNORDERED_MULTIMAP_H
