#ifndef LSTL_SLIST_ITERATOR_H
#define LSTL_SLIST_ITERATOR_H

#include "detail/slist_node.h"
#include "internal/detail/iterator_facet.h"

namespace lstl {

template <typename T, typename Ref, typename Ptr>
struct slist_iterator {
  typedef slist_iterator<T, T&, T*> iterator;
  typedef slist_iterator<T, const T&, const T*> const_iterator;
  typedef detail::slist_node_base* base_ptr;
  typedef detail::slist_node<T> link_type;
  typedef detail::forward_iterator_tag iterator_category;
  typedef T value_type;
  typedef Ptr pointer;
  typedef Ref reference;
  typedef ptrdiff_t difference_type;

  base_ptr cur;

  slist_iterator() : cur(0) {}
  explicit slist_iterator(base_ptr n) : cur(n) {}

  slist_iterator(const iterator& x) : cur(x.cur) {}

  reference operator*() const { return static_cast<link_type*>(cur)->data; }
  pointer operator->() const { return &(operator*()); }

  slist_iterator& operator++() {
    cur = cur->next;
    return *this;
  }

  slist_iterator operator++(int) {
    slist_iterator tmp = *this;
    cur = cur->next;
    return tmp;
  }

  bool operator==(const slist_iterator& x) const { return cur == x.cur; }
  bool operator!=(const slist_iterator& x) const { return cur != x.cur; }
};

}  // namespace lstl

#endif  // LSTL_SLIST_ITERATOR_H
