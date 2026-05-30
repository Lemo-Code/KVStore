#ifndef LSTL_HEAP_H
#define LSTL_HEAP_H

#include "functional.h"
#include "internal/detail/iterator_facet.h"
#include "utility.h"

namespace lstl {

namespace detail {

template <typename RandomAccessIterator, typename Distance, typename T, typename Compare>
inline void push_heap_impl(RandomAccessIterator first, Distance hole, Distance top, T value,
                           Compare comp) {
  Distance parent = (hole - 1) / 2;
  while (hole > top && comp(*(first + parent), value)) {
    *(first + hole) = lstl::move(*(first + parent));
    hole = parent;
    parent = (hole - 1) / 2;
  }
  *(first + hole) = lstl::move(value);
}

template <typename RandomAccessIterator, typename Distance, typename T, typename Compare>
inline void adjust_heap(RandomAccessIterator first, Distance hole, Distance len, T value,
                        Compare comp) {
  const Distance top = hole;
  Distance second_child = 2 * hole + 2;
  while (second_child < len) {
    if (comp(*(first + second_child), *(first + (second_child - 1)))) {
      --second_child;
    }
    *(first + hole) = lstl::move(*(first + second_child));
    hole = second_child;
    second_child = 2 * hole + 2;
  }
  if (second_child == len) {
    *(first + hole) = lstl::move(*(first + (second_child - 1)));
    hole = second_child - 1;
  }
  push_heap_impl(first, hole, top, lstl::move(value), comp);
}

}  // namespace detail

template <typename RandomAccessIterator, typename Compare>
inline void push_heap(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
  if (last - first > 1) {
    typedef typename detail::iterator_traits<RandomAccessIterator>::difference_type distance_type;
    detail::push_heap_impl(first, (last - first) - 1, distance_type(0),
                           lstl::move(*(last - 1)), comp);
  }
}

template <typename RandomAccessIterator>
inline void push_heap(RandomAccessIterator first, RandomAccessIterator last) {
  typedef typename detail::iterator_traits<RandomAccessIterator>::value_type value_type;
  push_heap(first, last, less<value_type>());
}

template <typename RandomAccessIterator, typename Compare>
inline void pop_heap(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
  if (last - first > 1) {
    typedef typename detail::iterator_traits<RandomAccessIterator>::difference_type distance_type;
    --last;
    swap(*first, *last);
    detail::adjust_heap(first, distance_type(0), last - first, lstl::move(*first), comp);
  }
}

template <typename RandomAccessIterator>
inline void pop_heap(RandomAccessIterator first, RandomAccessIterator last) {
  typedef typename detail::iterator_traits<RandomAccessIterator>::value_type value_type;
  pop_heap(first, last, less<value_type>());
}

template <typename RandomAccessIterator, typename Compare>
inline void make_heap(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
  if (last - first < 2) {
    return;
  }
  typedef typename detail::iterator_traits<RandomAccessIterator>::difference_type distance_type;
  const distance_type len = last - first;
  distance_type parent = (len - 2) / 2;
  for (;;) {
    detail::adjust_heap(first, parent, len, lstl::move(*(first + parent)), comp);
    if (parent == 0) {
      break;
    }
    --parent;
  }
}

template <typename RandomAccessIterator>
inline void make_heap(RandomAccessIterator first, RandomAccessIterator last) {
  typedef typename detail::iterator_traits<RandomAccessIterator>::value_type value_type;
  make_heap(first, last, less<value_type>());
}

template <typename RandomAccessIterator, typename Compare>
inline void sort_heap(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
  while (last - first > 1) {
    pop_heap(first, last, comp);
    --last;
  }
}

template <typename RandomAccessIterator>
inline void sort_heap(RandomAccessIterator first, RandomAccessIterator last) {
  typedef typename detail::iterator_traits<RandomAccessIterator>::value_type value_type;
  sort_heap(first, last, less<value_type>());
}

}  // namespace lstl

#endif  // LSTL_HEAP_H
