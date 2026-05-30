#ifndef LSTL_REVERSE_ITERATOR_H
#define LSTL_REVERSE_ITERATOR_H

#include "internal/detail/iterator_facet.h"

namespace lstl {
namespace detail {

template <typename Iterator>
class reverse_iterator {
 protected:
  Iterator current;

 public:
  typedef typename detail::iterator_traits<Iterator>::difference_type difference_type;
  typedef typename detail::iterator_traits<Iterator>::value_type value_type;
  typedef typename detail::iterator_traits<Iterator>::reference reference;
  typedef typename detail::iterator_traits<Iterator>::pointer pointer;
  typedef typename detail::iterator_traits<Iterator>::iterator_category iterator_category;
  typedef Iterator iterator_type;

  reverse_iterator() : current() {}
  explicit reverse_iterator(Iterator x) : current(x) {}

  Iterator base() const { return current; }

  reference operator*() const {
    Iterator tmp = current;
    return *--tmp;
  }

  pointer operator->() const { return &(operator*()); }

  reverse_iterator& operator++() {
    --current;
    return *this;
  }

  reverse_iterator operator++(int) {
    reverse_iterator tmp = *this;
    --current;
    return tmp;
  }

  reverse_iterator& operator--() {
    ++current;
    return *this;
  }

  reverse_iterator operator--(int) {
    reverse_iterator tmp = *this;
    ++current;
    return tmp;
  }
};

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_REVERSE_ITERATOR_H
