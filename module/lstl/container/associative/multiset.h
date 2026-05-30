#ifndef LSTL_MULTISET_H
#define LSTL_MULTISET_H

#include "detail/rb_tree.h"

namespace lstl {

template <typename Key, typename Compare = less<Key>, typename Alloc = allocator<Key> >
class multiset {
 public:
  typedef Key key_type;
  typedef Key value_type;
  typedef Compare key_compare;
  typedef Compare value_compare;
  typedef Alloc allocator_type;
  typedef typename detail::rb_tree<key_type, value_type, detail::identity<value_type>,
                                   key_compare, Alloc>::iterator iterator;
  typedef typename detail::rb_tree<key_type, value_type, detail::identity<value_type>,
                                   key_compare, Alloc>::const_iterator const_iterator;
  typedef typename detail::rb_tree<key_type, value_type, detail::identity<value_type>,
                                   key_compare, Alloc>::size_type size_type;
  typedef typename detail::rb_tree<key_type, value_type, detail::identity<value_type>,
                                   key_compare, Alloc>::difference_type difference_type;

 protected:
  typedef detail::rb_tree<key_type, value_type, detail::identity<value_type>, key_compare,
                          Alloc> rep_type;
  rep_type tree_;

 public:
  multiset() : tree_() {}
  explicit multiset(const Compare& comp) : tree_(comp) {}

  template <typename InputIterator>
  multiset(InputIterator first, InputIterator last) : tree_() {
    tree_.insert_equal(first, last);
  }

  multiset(const multiset& other) : tree_(other.tree_) {}
  multiset(multiset&& other) throw() : tree_(lstl::move(other.tree_)) {}

  multiset& operator=(const multiset& other) {
    tree_ = other.tree_;
    return *this;
  }

  multiset& operator=(multiset&& other) throw() {
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

  key_compare key_comp() const { return tree_.key_comp(); }
  value_compare value_comp() const { return tree_.key_comp(); }

  iterator insert(const value_type& x) { return tree_.insert_equal(x); }
  iterator insert(iterator position, const value_type& x) {
    (void)position;
    return insert(x);
  }

  template <typename InputIterator>
  void insert(InputIterator first, InputIterator last) {
    tree_.insert_equal(first, last);
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

  void swap(multiset& other) { tree_.swap(other.tree_); }
};

template <typename Key, typename Compare, typename Alloc>
inline bool operator==(const multiset<Key, Compare, Alloc>& a,
                       const multiset<Key, Compare, Alloc>& b) {
  return a.size() == b.size() && equal(a.begin(), a.end(), b.begin());
}

template <typename Key, typename Compare, typename Alloc>
inline bool operator!=(const multiset<Key, Compare, Alloc>& a,
                       const multiset<Key, Compare, Alloc>& b) {
  return !(a == b);
}

template <typename Key, typename Compare, typename Alloc>
inline void swap(multiset<Key, Compare, Alloc>& a, multiset<Key, Compare, Alloc>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_MULTISET_H
