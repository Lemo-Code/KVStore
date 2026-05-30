#ifndef LSTL_SKIP_MULTIMAP_H
#define LSTL_SKIP_MULTIMAP_H

#include "detail/key_of_value.h"
#include "detail/skip_list.h"

namespace lstl {

template <typename Key, typename T, typename Compare = less<Key>,
          typename Alloc = allocator<pair<const Key, T> > >
class skip_multimap {
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
  typedef detail::skip_list<key_type, value_type, detail::select1st<value_type>, key_compare,
                            Alloc> rep_type;
  rep_type list_;

 public:
  typedef typename rep_type::iterator iterator;
  typedef typename rep_type::const_iterator const_iterator;
  typedef typename rep_type::size_type size_type;
  typedef typename rep_type::difference_type difference_type;
  typedef typename rep_type::reference reference;
  typedef typename rep_type::const_reference const_reference;

  skip_multimap() : list_() {}
  explicit skip_multimap(const Compare& comp) : list_(comp) {}

  template <typename InputIterator>
  skip_multimap(InputIterator first, InputIterator last) : list_() {
    list_.insert_equal(first, last);
  }

  skip_multimap(const skip_multimap& other) : list_(other.list_) {}
  skip_multimap(skip_multimap&& other) throw() : list_(lstl::move(other.list_)) {}

  skip_multimap& operator=(const skip_multimap& other) {
    list_ = other.list_;
    return *this;
  }

  skip_multimap& operator=(skip_multimap&& other) throw() {
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
  value_compare value_comp() const { return value_compare(list_.key_comp()); }

  iterator insert(const value_type& x) { return list_.insert_equal(x); }

  template <typename Pair>
  iterator insert(const Pair& x) {
    return list_.insert_equal(value_type(x.first, x.second));
  }

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

  void swap(skip_multimap& other) { list_.swap(other.list_); }
};

template <typename Key, typename T, typename Compare, typename Alloc>
inline bool operator==(const skip_multimap<Key, T, Compare, Alloc>& a,
                       const skip_multimap<Key, T, Compare, Alloc>& b) {
  return a.size() == b.size() && equal(a.begin(), a.end(), b.begin());
}

template <typename Key, typename T, typename Compare, typename Alloc>
inline bool operator!=(const skip_multimap<Key, T, Compare, Alloc>& a,
                       const skip_multimap<Key, T, Compare, Alloc>& b) {
  return !(a == b);
}

template <typename Key, typename T, typename Compare, typename Alloc>
inline void swap(skip_multimap<Key, T, Compare, Alloc>& a,
                 skip_multimap<Key, T, Compare, Alloc>& b) {
  a.swap(b);
}

}  // namespace lstl

#endif  // LSTL_SKIP_MULTIMAP_H
