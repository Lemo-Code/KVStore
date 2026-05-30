#ifndef LSTL_UNORDERED_SET_H
#define LSTL_UNORDERED_SET_H

#include "detail/hashtable.h"

namespace lstl {

template <typename Key, typename Hash = hash<Key>, typename Pred = equal_to<Key>,
          typename Alloc = allocator<Key> >
class unordered_set {
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
  typedef typename rep_type::difference_type difference_type;

  unordered_set(size_type n = 0, const Hash& hf = Hash(), const Pred& eq = Pred())
      : table_(n, hf, eq) {}

  template <typename InputIterator>
  unordered_set(InputIterator first, InputIterator last, size_type n = 0,
                const Hash& hf = Hash(), const Pred& eq = Pred())
      : table_(n, hf, eq) {
    table_.insert_unique(first, last);
  }

  unordered_set(const unordered_set& other) : table_(other.table_) {}
  unordered_set(unordered_set&& other) throw() : table_(lstl::move(other.table_)) {}

  unordered_set& operator=(const unordered_set& other) {
    table_ = other.table_;
    return *this;
  }

  unordered_set& operator=(unordered_set&& other) throw() {
    table_ = lstl::move(other.table_);
    return *this;
  }

  iterator begin() { return table_.begin(); }
  const_iterator begin() const { return table_.begin(); }
  iterator end() { return table_.end(); }
  const_iterator end() const { return table_.end(); }

  size_type size() const { return table_.size(); }
  bool empty() const { return table_.empty(); }
  size_type bucket_count() const { return table_.bucket_count(); }
  float max_load_factor() const { return table_.max_load_factor(); }
  void max_load_factor(float ml) { table_.max_load_factor(ml); }
  void rehash(size_type n) { table_.rehash(n); }

  hasher hash_function() const { return table_.hash_funct(); }
  key_equal key_eq() const { return table_.key_eq(); }

  pair<iterator, bool> insert(const value_type& obj) {
    iterator it = find(obj);
    if (it != end()) {
      return pair<iterator, bool>(it, false);
    }
    return pair<iterator, bool>(table_.insert_unique(obj), true);
  }

  template <typename InputIterator>
  void insert(InputIterator first, InputIterator last) {
    table_.insert_unique(first, last);
  }

  iterator find(const key_type& k) { return table_.find(k); }
  const_iterator find(const key_type& k) const { return table_.find(k); }
  size_type count(const key_type& k) const { return find(k) == end() ? 0 : 1; }

  size_type erase(const key_type& k) { return table_.erase(k); }
  iterator erase(iterator it) { return table_.erase(it); }
  void clear() { table_.clear(); }
  void swap(unordered_set& other) { table_.swap(other.table_); }
};

template <typename Key, typename Hash, typename Pred, typename Alloc>
inline bool operator==(const unordered_set<Key, Hash, Pred, Alloc>& a,
                       const unordered_set<Key, Hash, Pred, Alloc>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (typename unordered_set<Key, Hash, Pred, Alloc>::const_iterator it = a.begin();
       it != a.end(); ++it) {
    if (b.find(*it) == b.end()) {
      return false;
    }
  }
  return true;
}

template <typename Key, typename Hash, typename Pred, typename Alloc>
inline bool operator!=(const unordered_set<Key, Hash, Pred, Alloc>& a,
                       const unordered_set<Key, Hash, Pred, Alloc>& b) {
  return !(a == b);
}

template <typename Key, typename Hash, typename Pred, typename Alloc>
inline void swap(unordered_set<Key, Hash, Pred, Alloc>& a,
                 unordered_set<Key, Hash, Pred, Alloc>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_UNORDERED_SET_H
