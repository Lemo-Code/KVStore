#ifndef LSTL_SKIP_MULTISET_H
#define LSTL_SKIP_MULTISET_H

#include "detail/key_of_value.h"
#include "detail/skip_list.h"

namespace lstl {

template <typename Key, typename Compare = less<Key>, typename Alloc = allocator<Key> >
class skip_multiset {
 public:
  typedef Key key_type;
  typedef Key value_type;
  typedef Compare key_compare;
  typedef Compare value_compare;
  typedef Alloc allocator_type;
  typedef typename detail::skip_list<key_type, value_type, detail::identity<value_type>,
                                     key_compare, Alloc>::iterator iterator;
  typedef typename detail::skip_list<key_type, value_type, detail::identity<value_type>,
                                     key_compare, Alloc>::const_iterator const_iterator;
  typedef typename detail::skip_list<key_type, value_type, detail::identity<value_type>,
                                     key_compare, Alloc>::size_type size_type;
  typedef typename detail::skip_list<key_type, value_type, detail::identity<value_type>,
                                     key_compare, Alloc>::difference_type difference_type;

 protected:
  typedef detail::skip_list<key_type, value_type, detail::identity<value_type>, key_compare,
                            Alloc> rep_type;
  rep_type list_;

 public:
  skip_multiset() : list_() {}
  explicit skip_multiset(const Compare& comp) : list_(comp) {}

  template <typename InputIterator>
  skip_multiset(InputIterator first, InputIterator last) : list_() {
    list_.insert_equal(first, last);
  }

  skip_multiset(const skip_multiset& other) : list_(other.list_) {}
  skip_multiset(skip_multiset&& other) throw() : list_(lstl::move(other.list_)) {}

  skip_multiset& operator=(const skip_multiset& other) {
    list_ = other.list_;
    return *this;
  }

  skip_multiset& operator=(skip_multiset&& other) throw() {
    list_ = lstl::move(other.list_);
    return *this;
  }

  iterator begin() { return list_.begin(); }
  const_iterator begin() const { return list_.begin(); }
  iterator end() { return list_.end(); }
  const_iterator end() const { return list_.end(); }

  size_type size() const { return list_.size(); }
  size_type max_size() const { return list_.max_size(); }
  bool empty() const { return list_.empty(); }

  key_compare key_comp() const { return list_.key_comp(); }
  value_compare value_comp() const { return list_.key_comp(); }

  iterator insert(const value_type& x) { return list_.insert_equal(x); }
  iterator insert(iterator position, const value_type& x) {
    (void)position;
    return insert(x);
  }

  template <typename InputIterator>
  void insert(InputIterator first, InputIterator last) {
    list_.insert_equal(first, last);
  }

  void erase(iterator position) { list_.erase(position); }
  size_type erase(const key_type& x) { return list_.erase(x); }
  iterator erase(iterator first, iterator last) { return list_.erase(first, last); }
  void clear() { list_.clear(); }

  iterator find(const key_type& x) { return list_.find(x); }
  const_iterator find(const key_type& x) const { return list_.find(x); }
  size_type count(const key_type& x) const { return list_.count(x); }
  iterator lower_bound(const key_type& x) { return list_.lower_bound(x); }
  const_iterator lower_bound(const key_type& x) const { return list_.lower_bound(x); }
  iterator upper_bound(const key_type& x) { return list_.upper_bound(x); }
  const_iterator upper_bound(const key_type& x) const { return list_.upper_bound(x); }
  pair<iterator, iterator> equal_range(const key_type& x) { return list_.equal_range(x); }
  pair<const_iterator, const_iterator> equal_range(const key_type& x) const {
    return list_.equal_range(x);
  }

  void swap(skip_multiset& other) { list_.swap(other.list_); }
};

template <typename Key, typename Compare, typename Alloc>
inline bool operator==(const skip_multiset<Key, Compare, Alloc>& a,
                       const skip_multiset<Key, Compare, Alloc>& b) {
  return a.size() == b.size() && equal(a.begin(), a.end(), b.begin());
}

template <typename Key, typename Compare, typename Alloc>
inline bool operator!=(const skip_multiset<Key, Compare, Alloc>& a,
                       const skip_multiset<Key, Compare, Alloc>& b) {
  return !(a == b);
}

template <typename Key, typename Compare, typename Alloc>
inline void swap(skip_multiset<Key, Compare, Alloc>& a, skip_multiset<Key, Compare, Alloc>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_SKIP_MULTISET_H
