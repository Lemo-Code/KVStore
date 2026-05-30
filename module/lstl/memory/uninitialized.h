#ifndef LSTL_UNINITIALIZED_H
#define LSTL_UNINITIALIZED_H

#include "construct.h"
#include "memory_ops.h"
#include "internal/detail/iterator_facet.h"
#include "type_traits.h"

namespace lstl {
namespace detail {

template <typename InputIterator, typename ForwardIterator>
inline ForwardIterator uninitialized_copy_impl(InputIterator first, InputIterator last,
                                               ForwardIterator result,
                                               input_iterator_tag) {
  for (; first != last; ++first, ++result) {
    lstl::construct(&*result, *first);
  }
  return result;
}

template <typename InputIterator, typename ForwardIterator>
inline ForwardIterator uninitialized_copy_impl(InputIterator first, InputIterator last,
                                               ForwardIterator result,
                                               forward_iterator_tag) {
  return uninitialized_copy_impl(first, last, result, input_iterator_tag());
}

template <typename InputIterator, typename ForwardIterator>
inline ForwardIterator uninitialized_copy_impl(InputIterator first, InputIterator last,
                                               ForwardIterator result,
                                               bidirectional_iterator_tag) {
  return uninitialized_copy_impl(first, last, result, input_iterator_tag());
}

template <typename InputIterator, typename ForwardIterator>
inline ForwardIterator uninitialized_copy_aux(InputIterator first, InputIterator last,
                                              ForwardIterator result, __true_type) {
  typedef typename iterator_traits<ForwardIterator>::value_type value_type;
  const ptrdiff_t n = last - first;
  if (n > 0) {
    lstl::memcpy(static_cast<void*>(&*result), static_cast<const void*>(&*first),
                 static_cast<size_t>(n) * sizeof(value_type));
  }
  return result + n;
}

template <typename InputIterator, typename ForwardIterator>
inline ForwardIterator uninitialized_copy_aux(InputIterator first, InputIterator last,
                                              ForwardIterator result, __false_type) {
  return uninitialized_copy_impl(first, last, result, input_iterator_tag());
}

template <typename InputIterator, typename ForwardIterator>
inline ForwardIterator uninitialized_copy_impl(InputIterator first, InputIterator last,
                                               ForwardIterator result,
                                               random_access_iterator_tag) {
  typedef typename iterator_traits<ForwardIterator>::value_type value_type;
  typedef typename __type_traits<value_type>::this_type_is_POD_type is_POD;
  return uninitialized_copy_aux(first, last, result, is_POD());
}

}  // namespace detail

template <typename InputIterator, typename ForwardIterator>
inline ForwardIterator uninitialized_copy(InputIterator first, InputIterator last,
                                          ForwardIterator result) {
  return detail::uninitialized_copy_impl(
      first, last, result,
      typename detail::iterator_traits<InputIterator>::iterator_category());
}

template <typename ForwardIterator, typename T>
inline void uninitialized_fill_aux(ForwardIterator first, ForwardIterator last, const T& x,
                                   __true_type) {
  for (; first != last; ++first) {
    *first = x;
  }
}

template <typename ForwardIterator, typename T>
inline void uninitialized_fill_aux(ForwardIterator first, ForwardIterator last, const T& x,
                                   __false_type) {
  for (; first != last; ++first) {
    construct(&*first, x);
  }
}

template <typename ForwardIterator, typename T>
inline void uninitialized_fill(ForwardIterator first, ForwardIterator last, const T& x) {
  typedef typename detail::iterator_traits<ForwardIterator>::value_type value_type;
  typedef typename __type_traits<value_type>::this_type_is_POD_type is_POD;
  uninitialized_fill_aux(first, last, x, is_POD());
}

template <typename ForwardIterator, typename Size, typename T>
inline ForwardIterator uninitialized_fill_n_aux(ForwardIterator first, Size n, const T& x,
                                                __true_type) {
  for (; n > 0; --n, ++first) {
    *first = x;
  }
  return first;
}

template <typename ForwardIterator, typename Size, typename T>
inline ForwardIterator uninitialized_fill_n_aux(ForwardIterator first, Size n, const T& x,
                                                __false_type) {
  for (; n > 0; --n, ++first) {
    construct(&*first, x);
  }
  return first;
}

template <typename ForwardIterator, typename Size, typename T>
inline ForwardIterator uninitialized_fill_n(ForwardIterator first, Size n, const T& x) {
  typedef typename detail::iterator_traits<ForwardIterator>::value_type value_type;
  typedef typename __type_traits<value_type>::this_type_is_POD_type is_POD;
  return uninitialized_fill_n_aux(first, n, x, is_POD());
}

}  // namespace lstl

#endif  // LSTL_UNINITIALIZED_H
