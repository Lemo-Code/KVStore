#ifndef LSTL_BSET_H
#define LSTL_BSET_H

#include "detail/bplus_tree.h"
#include "detail/rb_tree.h"

namespace lstl {

template <typename Key, typename Compare = less<Key>, typename Alloc = allocator<Key>,
          size_t Order = 8>
class bset {
 public:
  typedef Key key_type;
  typedef Key value_type;
  typedef Compare key_compare;
  typedef Compare value_compare;
  typedef Alloc allocator_type;
  typedef typename detail::bplus_tree<key_type, value_type, detail::identity<value_type>,
                                    key_compare, Alloc, Order>::iterator iterator;
  typedef typename detail::bplus_tree<key_type, value_type, detail::identity<value_type>,
                                    key_compare, Alloc, Order>::const_iterator const_iterator;
  typedef typename detail::bplus_tree<key_type, value_type, detail::identity<value_type>,
                                    key_compare, Alloc, Order>::size_type size_type;
  typedef typename detail::bplus_tree<key_type, value_type, detail::identity<value_type>,
                                    key_compare, Alloc, Order>::difference_type difference_type;

 protected:
  typedef detail::bplus_tree<key_type, value_type, detail::identity<value_type>, key_compare,
                             Alloc, Order> rep_type;
  rep_type tree_;

 public:
  bset() : tree_() {}
  explicit bset(const Compare& comp) : tree_(comp) {}

  template <typename InputIterator>
  bset(InputIterator first, InputIterator last) : tree_() {
    tree_.insert_unique(first, last);
  }

  bset(const bset& other) : tree_(other.tree_) {}
  bset(bset&& other) throw() : tree_(lstl::move(other.tree_)) {}

  bset& operator=(const bset& other) {
    tree_ = other.tree_;
    return *this;
  }

  bset& operator=(bset&& other) throw() {
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

  pair<iterator, bool> insert(const value_type& x) { return tree_.insert_unique(x); }
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

  void swap(bset& other) { tree_.swap(other.tree_); }
};

template <typename Key, typename Compare, typename Alloc, size_t Order>
inline bool operator==(const bset<Key, Compare, Alloc, Order>& a,
                       const bset<Key, Compare, Alloc, Order>& b) {
  return a.size() == b.size() && equal(a.begin(), a.end(), b.begin());
}

template <typename Key, typename Compare, typename Alloc, size_t Order>
inline bool operator!=(const bset<Key, Compare, Alloc, Order>& a,
                       const bset<Key, Compare, Alloc, Order>& b) {
  return !(a == b);
}

template <typename Key, typename Compare, typename Alloc, size_t Order>
inline void swap(bset<Key, Compare, Alloc, Order>& a, bset<Key, Compare, Alloc, Order>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_BSET_H
