#ifndef LSTL_MAP_H
#define LSTL_MAP_H

#include "detail/rb_tree.h"

namespace lstl {

template <typename Key, typename T, typename Compare = less<Key>,
          typename Alloc = allocator<pair<const Key, T> > >
class map {
 public:
  typedef Key key_type;
  typedef T mapped_type;
  typedef pair<const Key, T> value_type;
  typedef Compare key_compare;
  typedef Alloc allocator_type;

  struct value_compare {
    Compare comp;
    value_compare() : comp() {}
    explicit value_compare(Compare c) : comp(c) {}
    bool operator()(const value_type& a, const value_type& b) const { return comp(a.first, b.first); }
  };

 protected:
  typedef detail::rb_tree<key_type, value_type, detail::select1st<value_type>, key_compare,
                          Alloc> rep_type;
  rep_type tree_;

 public:
  typedef typename rep_type::iterator iterator;
  typedef typename rep_type::const_iterator const_iterator;
  typedef typename rep_type::size_type size_type;
  typedef typename rep_type::difference_type difference_type;
  typedef typename rep_type::reference reference;
  typedef typename rep_type::const_reference const_reference;

  map() : tree_() {}
  explicit map(const Compare& comp) : tree_(comp) {}

  template <typename InputIterator>
  map(InputIterator first, InputIterator last) : tree_() {
    tree_.insert_unique(first, last);
  }

  map(const map& other) : tree_(other.tree_) {}
  map(map&& other) throw() : tree_(lstl::move(other.tree_)) {}

  map& operator=(const map& other) {
    tree_ = other.tree_;
    return *this;
  }

  map& operator=(map&& other) throw() {
    tree_ = lstl::move(other.tree_);
    return *this;
  }

  iterator begin() { return tree_.begin(); }
  const_iterator begin() const { return tree_.begin(); }
  iterator end() { return tree_.end(); }
  const_iterator end() const { return tree_.end(); }

  size_type size() const { return tree_.size(); }
  size_type max_size() const { return tree_.max_size(); }
  bool empty() const { return tree_.empty(); }

  mapped_type& operator[](const key_type& k) {
    iterator i = lower_bound(k);
    if (i == end() || key_comp()(k, i->first)) {
      i = insert(value_type(k, mapped_type())).first;
    }
    return i->second;
  }

  key_compare key_comp() const { return tree_.key_comp(); }
  value_compare value_comp() const { return value_compare(tree_.key_comp()); }

  pair<iterator, bool> insert(const value_type& x) { return tree_.insert_unique(x); }

  template <typename Pair>
  pair<iterator, bool> insert(const Pair& x) {
    return tree_.insert_unique(value_type(x.first, x.second));
  }

  iterator insert(iterator position, const value_type& x) {
    (void)position;
    return insert(x).first;
  }

  template <typename InputIterator>
  void insert(InputIterator first, InputIterator last) {
    tree_.insert_unique(first, last);
  }

  void erase(iterator position) { tree_.erase(position); }
  size_type erase(const key_type& x) { return tree_.erase(x); }
  iterator erase(iterator first, iterator last) { return tree_.erase(first, last); }
  void clear() { tree_.clear(); }

  iterator find(const key_type& x) { return tree_.find(x); }
  const_iterator find(const key_type& x) const { return tree_.find(x); }
  size_type count(const key_type& x) const { return tree_.count(x); }
  iterator lower_bound(const key_type& x) { return tree_.lower_bound(x); }
  const_iterator lower_bound(const key_type& x) const { return tree_.lower_bound(x); }
  iterator upper_bound(const key_type& x) { return tree_.upper_bound(x); }
  const_iterator upper_bound(const key_type& x) const { return tree_.upper_bound(x); }
  pair<iterator, iterator> equal_range(const key_type& x) { return tree_.equal_range(x); }
  pair<const_iterator, const_iterator> equal_range(const key_type& x) const {
    return tree_.equal_range(x);
  }

  void swap(map& other) { tree_.swap(other.tree_); }
};

template <typename Key, typename T, typename Compare, typename Alloc>
inline bool operator==(const map<Key, T, Compare, Alloc>& a,
                       const map<Key, T, Compare, Alloc>& b) {
  return a.size() == b.size() && equal(a.begin(), a.end(), b.begin());
}

template <typename Key, typename T, typename Compare, typename Alloc>
inline bool operator!=(const map<Key, T, Compare, Alloc>& a,
                       const map<Key, T, Compare, Alloc>& b) {
  return !(a == b);
}

template <typename Key, typename T, typename Compare, typename Alloc>
inline void swap(map<Key, T, Compare, Alloc>& a, map<Key, T, Compare, Alloc>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_MAP_H
