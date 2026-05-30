#ifndef LSTL_LIST_ITERATOR_H
#define LSTL_LIST_ITERATOR_H

#include "internal/detail/iterator_facet.h"
#include "detail/list_node.h"

namespace lstl {

template <typename T, typename Ref, typename Ptr>
struct list_iterator {
  typedef list_iterator<T, T&, T*> iterator;
  typedef list_iterator<T, const T&, const T*> const_iterator;
  typedef detail::list_node_base* base_ptr;
  typedef detail::list_node<T> link_type;
  typedef detail::bidirectional_iterator_tag iterator_category;
  typedef T value_type;
  typedef Ptr pointer;
  typedef Ref reference;
  typedef ptrdiff_t difference_type;

  base_ptr cur;

  list_iterator() : cur(0) {}
  explicit list_iterator(base_ptr n) : cur(n) {}

  list_iterator(const iterator& x) : cur(x.cur) {}

  reference operator*() const { return static_cast<link_type*>(cur)->data; }
  pointer operator->() const { return &(operator*()); }

  list_iterator& operator++() {
    cur = cur->next;
    return *this;
  }

  list_iterator operator++(int) {
    list_iterator tmp = *this;
    cur = cur->next;
    return tmp;
  }

  list_iterator& operator--() {
    cur = cur->prev;
    return *this;
  }

  list_iterator operator--(int) {
    list_iterator tmp = *this;
    cur = cur->prev;
    return tmp;
  }

  bool operator==(const list_iterator& x) const { return cur == x.cur; }
  bool operator!=(const list_iterator& x) const { return cur != x.cur; }
};

template <typename T, typename Ref, typename Ptr>
inline list_iterator<T, Ref, Ptr> operator+(ptrdiff_t n, list_iterator<T, Ref, Ptr> it) {
  return it + n;
}

}  // namespace lstl

#endif  // LSTL_LIST_ITERATOR_H
