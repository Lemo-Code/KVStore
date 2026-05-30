#ifndef LSTL_DEQUE_ITERATOR_H
#define LSTL_DEQUE_ITERATOR_H

#include "internal/detail/iterator_facet.h"

namespace lstl {
namespace detail {

inline size_t deque_buffer_size(size_t type_size) {
  return type_size < 512 ? 512 / type_size : size_t(1);
}

}  // namespace detail

template <typename T, typename Ref, typename Ptr>
struct deque_iterator {
  typedef deque_iterator<T, T&, T*> iterator;
  typedef deque_iterator<T, const T&, const T*> const_iterator;
  typedef detail::random_access_iterator_tag iterator_category;
  typedef T value_type;
  typedef Ptr pointer;
  typedef Ref reference;
  typedef ptrdiff_t difference_type;

  static size_t buffer_size() { return detail::deque_buffer_size(sizeof(T)); }

  T* cur;
  T* first;
  T* last;
  T** node;

  deque_iterator() : cur(0), first(0), last(0), node(0) {}
  deque_iterator(T* c, T* f, T* l, T** n) : cur(c), first(f), last(l), node(n) {}

  deque_iterator(const iterator& x) : cur(x.cur), first(x.first), last(x.last), node(x.node) {}

  void set(T* c, T* f, T* l, T** n) {
    cur = c;
    first = f;
    last = l;
    node = n;
  }

  reference operator*() const { return *cur; }
  pointer operator->() const { return cur; }

  deque_iterator& operator++() {
    if (++cur == last) {
      cur = first = *(++node);
      last = first + buffer_size();
    }
    return *this;
  }

  deque_iterator operator++(int) {
    deque_iterator tmp = *this;
    ++*this;
    return tmp;
  }

  deque_iterator& operator--() {
    if (cur == first) {
      cur = last = *(node - 1);
      first = last - buffer_size();
    }
    --cur;
    return *this;
  }

  deque_iterator operator--(int) {
    deque_iterator tmp = *this;
    --*this;
    return tmp;
  }

  deque_iterator& operator+=(difference_type n) {
    const difference_type offset = n + (cur - first);
    if (offset >= 0 && offset < static_cast<difference_type>(buffer_size())) {
      cur += n;
    } else {
      const difference_type buf = static_cast<difference_type>(buffer_size());
      difference_type node_offset =
          offset > 0 ? offset / buf : -static_cast<difference_type>(
                                           (-offset - 1) / buf + 1);
      node += node_offset;
      cur = *(node) + (offset - node_offset * buf);
      first = *(node);
      last = first + buf;
    }
    return *this;
  }

  deque_iterator operator+(difference_type n) const {
    deque_iterator tmp = *this;
    return tmp += n;
  }

  deque_iterator& operator-=(difference_type n) { return *this += -n; }

  deque_iterator operator-(difference_type n) const {
    deque_iterator tmp = *this;
    return tmp -= n;
  }

  reference operator[](difference_type n) const { return *(*this + n); }

  bool operator==(const deque_iterator& x) const { return cur == x.cur; }
  bool operator!=(const deque_iterator& x) const { return cur != x.cur; }
  bool operator<(const deque_iterator& x) const {
    return (node == x.node) ? (cur < x.cur) : (node < x.node);
  }
};

template <typename T, typename Ref, typename Ptr>
inline deque_iterator<T, Ref, Ptr> operator+(ptrdiff_t n,
                                             deque_iterator<T, Ref, Ptr> it) {
  return it + n;
}

template <typename T, typename Ref, typename Ptr>
inline ptrdiff_t operator-(const deque_iterator<T, Ref, Ptr>& a,
                           const deque_iterator<T, Ref, Ptr>& b) {
  const ptrdiff_t buf = static_cast<ptrdiff_t>(deque_iterator<T, Ref, Ptr>::buffer_size());
  return buf * (a.node - b.node - 1) + (a.cur - a.first) + (b.last - b.cur);
}

}  // namespace lstl

#endif  // LSTL_DEQUE_ITERATOR_H
