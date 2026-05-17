#ifndef LSTL_CONSTRUCT_H
#define LSTL_CONSTRUCT_H

#include <new>

#include "internal/detail/iterator_facet.h"
#include "type_traits.h"

namespace lstl {

template <typename T>
inline void construct(T* p) {
  new (static_cast<void*>(p)) T();
}

template <typename T, typename V>
inline void construct(T* p, const V& value) {
  new (static_cast<void*>(p)) T(value);
}

template <typename T>
inline void construct(T* p, T&& value) {
  new (static_cast<void*>(p)) T(static_cast<T&&>(value));
}

template <typename T, typename... Args>
inline void construct(T* p, Args&&... args) {
  new (static_cast<void*>(p)) T(static_cast<Args&&>(args)...);
}

template <typename T>
inline void destroy(T* pointer) {
  pointer->~T();
}

namespace detail {

template <typename ForwardIterator>
inline void destroy_range(ForwardIterator first, ForwardIterator last,
                          detail::input_iterator_tag) {
  for (; first != last; ++first) {
    lstl::destroy(&*first);
  }
}

template <typename ForwardIterator>
inline void destroy_range(ForwardIterator first, ForwardIterator last,
                          detail::forward_iterator_tag) {
  destroy_range(first, last, detail::input_iterator_tag());
}

template <typename ForwardIterator>
inline void destroy_range(ForwardIterator first, ForwardIterator last,
                          detail::bidirectional_iterator_tag) {
  destroy_range(first, last, detail::forward_iterator_tag());
}

template <typename ForwardIterator>
inline void destroy_range(ForwardIterator first, ForwardIterator last,
                          detail::random_access_iterator_tag) {
  destroy_range(first, last, detail::input_iterator_tag());
}

template <typename ForwardIterator>
inline void destroy_range(ForwardIterator first, ForwardIterator last, __true_type) {
  (void)first;
  (void)last;
}

template <typename ForwardIterator>
inline void destroy_range(ForwardIterator first, ForwardIterator last, __false_type) {
  destroy_range(first, last,
                typename detail::iterator_traits<ForwardIterator>::iterator_category());
}

template <typename ForwardIterator, typename T>
inline void destroy_range(ForwardIterator first, ForwardIterator last, T*) {
  typedef typename __type_traits<T>::this_type_is_POD_type is_POD;
  destroy_range(first, last, is_POD());
}

}  // namespace detail

template <typename ForwardIterator>
inline void destroy(ForwardIterator first, ForwardIterator last) {
  typedef typename detail::iterator_traits<ForwardIterator>::value_type value_type;
  detail::destroy_range(first, last, static_cast<value_type*>(0));
}

}  // namespace lstl

#endif  // LSTL_CONSTRUCT_H
