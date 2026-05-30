#ifndef LSTL_UNORDERED_MAP_H
#define LSTL_UNORDERED_MAP_H

#include "detail/hash_functors.h"
#include "detail/hashtable.h"

namespace lstl {

template <typename Key, typename T, typename Hash = hash<Key>, typename Pred = equal_to<Key>,
          typename Alloc = allocator<pair<const Key, T> > >
class unordered_map {
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
  typedef typename rep_type::difference_type difference_type;

  unordered_map(size_type n = 0, const Hash& hf = Hash(), const Pred& eq = Pred())
      : table_(n, table_hash(hf), table_pred(eq)) {}

  template <typename InputIterator>
  unordered_map(InputIterator first, InputIterator last, size_type n = 0,
                const Hash& hf = Hash(), const Pred& eq = Pred())
      : table_(n, table_hash(hf), table_pred(eq)) {
    insert(first, last);
  }

  unordered_map(const unordered_map& other) : table_(other.table_) {}
  unordered_map(unordered_map&& other) throw() : table_(lstl::move(other.table_)) {}

  unordered_map& operator=(const unordered_map& other) {
    table_ = other.table_;
    return *this;
  }

  unordered_map& operator=(unordered_map&& other) throw() {
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

  hasher hash_function() const { return table_.hash_funct().h; }
  key_equal key_eq() const { return table_.key_eq().p; }

  mapped_type& operator[](const key_type& k) {
    iterator it = find(k);
    if (it == end()) {
      it = insert(value_type(k, mapped_type())).first;
    }
    return it->second;
  }

  pair<iterator, bool> insert(const value_type& obj) {
    iterator it = find(obj.first);
    if (it != end()) {
      return pair<iterator, bool>(it, false);
    }
    return pair<iterator, bool>(table_.insert_unique(obj), true);
  }

  template <typename InputIterator>
  void insert(InputIterator first, InputIterator last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  iterator find(const key_type& k) { return table_.find(make_probe(k)); }
  const_iterator find(const key_type& k) const { return table_.find(make_probe(k)); }
  size_type count(const key_type& k) const { return find(k) == end() ? 0 : 1; }

  size_type erase(const key_type& k) { return table_.erase(make_probe(k)); }
  iterator erase(iterator it) { return table_.erase(it); }
  void clear() { table_.clear(); }
  void swap(unordered_map& other) { table_.swap(other.table_); }
};

template <typename Key, typename T, typename Hash, typename Pred, typename Alloc>
inline bool operator==(const unordered_map<Key, T, Hash, Pred, Alloc>& a,
                       const unordered_map<Key, T, Hash, Pred, Alloc>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (typename unordered_map<Key, T, Hash, Pred, Alloc>::const_iterator it = a.begin();
       it != a.end(); ++it) {
    typename unordered_map<Key, T, Hash, Pred, Alloc>::const_iterator j = b.find(it->first);
    if (j == b.end() || !(j->second == it->second)) {
      return false;
    }
  }
  return true;
}

template <typename Key, typename T, typename Hash, typename Pred, typename Alloc>
inline bool operator!=(const unordered_map<Key, T, Hash, Pred, Alloc>& a,
                       const unordered_map<Key, T, Hash, Pred, Alloc>& b) {
  return !(a == b);
}

template <typename Key, typename T, typename Hash, typename Pred, typename Alloc>
inline void swap(unordered_map<Key, T, Hash, Pred, Alloc>& a,
                 unordered_map<Key, T, Hash, Pred, Alloc>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_UNORDERED_MAP_H
