#ifndef LSTL_INTERNAL_DETAIL_ITERATOR_FACET_H
#define LSTL_INTERNAL_DETAIL_ITERATOR_FACET_H

#include <cstddef>

namespace lstl {
namespace detail {

struct input_iterator_tag {};
struct forward_iterator_tag : public input_iterator_tag {};
struct bidirectional_iterator_tag : public forward_iterator_tag {};
struct random_access_iterator_tag : public bidirectional_iterator_tag {};

template <typename Iterator>
struct iterator_traits {
  typedef typename Iterator::difference_type difference_type;
  typedef typename Iterator::value_type value_type;
  typedef typename Iterator::pointer pointer;
  typedef typename Iterator::reference reference;
  typedef typename Iterator::iterator_category iterator_category;
};

template <typename T>
struct iterator_traits<T*> {
  typedef ptrdiff_t difference_type;
  typedef T value_type;
  typedef T* pointer;
  typedef T& reference;
  typedef random_access_iterator_tag iterator_category;
};

template <typename T>
struct iterator_traits<const T*> {
  typedef ptrdiff_t difference_type;
  typedef T value_type;
  typedef const T* pointer;
  typedef const T& reference;
  typedef random_access_iterator_tag iterator_category;
};

template <typename Iterator>
inline typename iterator_traits<Iterator>::iterator_category
iterator_category(const Iterator&) {
  typedef typename iterator_traits<Iterator>::iterator_category category;
  return category();
}

template <typename Iterator>
inline typename iterator_traits<Iterator>::difference_type*
distance_type(const Iterator&) {
  return static_cast<typename iterator_traits<Iterator>::difference_type*>(0);
}

template <typename Iterator>
inline typename iterator_traits<Iterator>::value_type*
value_type_ptr(const Iterator&) {
  return static_cast<typename iterator_traits<Iterator>::value_type*>(0);
}

template <typename InputIterator>
inline typename iterator_traits<InputIterator>::difference_type
distance(InputIterator first, InputIterator last, input_iterator_tag) {
  typename iterator_traits<InputIterator>::difference_type n = 0;
  for (; first != last; ++first) {
    ++n;
  }
  return n;
}

template <typename RandomAccessIterator>
inline typename iterator_traits<RandomAccessIterator>::difference_type
distance(RandomAccessIterator first, RandomAccessIterator last,
         random_access_iterator_tag) {
  return last - first;
}

template <typename InputIterator>
inline typename iterator_traits<InputIterator>::difference_type
distance(InputIterator first, InputIterator last) {
  return distance(first, last,
                  typename iterator_traits<InputIterator>::iterator_category());
}

}  // namespace detail

using detail::iterator_traits;
using detail::distance;
using detail::input_iterator_tag;
using detail::forward_iterator_tag;
using detail::bidirectional_iterator_tag;
using detail::random_access_iterator_tag;

}  // namespace lstl

#endif  // LSTL_INTERNAL_DETAIL_ITERATOR_FACET_H
