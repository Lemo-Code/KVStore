#ifndef LSTL_ALGORITHM_H
#define LSTL_ALGORITHM_H

#include "functional.h"
#include "heap.h"
#include "internal/detail/iterator_facet.h"
#include "utility.h"

namespace lstl {

template <typename ForwardIterator1, typename ForwardIterator2>
inline bool equal(ForwardIterator1 first1, ForwardIterator1 last1, ForwardIterator2 first2) {
  for (; first1 != last1; ++first1, ++first2) {
    if (!(*first1 == *first2)) {
      return false;
    }
  }
  return true;
}

template <typename InputIterator1, typename InputIterator2>
inline bool lexicographical_compare(InputIterator1 first1, InputIterator1 last1,
                                     InputIterator2 first2, InputIterator2 last2) {
  for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
    if (*first1 < *first2) {
      return true;
    }
    if (*first2 < *first1) {
      return false;
    }
  }
  return first1 == last1 && first2 != last2;
}

template <typename InputIterator1, typename InputIterator2, typename Compare>
inline bool lexicographical_compare(InputIterator1 first1, InputIterator1 last1,
                                     InputIterator2 first2, InputIterator2 last2,
                                     Compare comp) {
  for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
    if (comp(*first1, *first2)) {
      return true;
    }
    if (comp(*first2, *first1)) {
      return false;
    }
  }
  return first1 == last1 && first2 != last2;
}

template <typename ForwardIterator>
inline void iter_swap(ForwardIterator a, ForwardIterator b) {
  swap(*a, *b);
}

template <typename BidirectionalIterator>
inline void reverse(BidirectionalIterator first, BidirectionalIterator last) {
  for (; first != last && first != --last; ++first) {
    iter_swap(first, last);
  }
}

namespace detail {

template <typename RandomAccessIterator, typename Compare>
inline void sort_insertion(RandomAccessIterator first, RandomAccessIterator last,
                           Compare comp) {
  for (RandomAccessIterator it = first + 1; it != last; ++it) {
    typedef typename iterator_traits<RandomAccessIterator>::value_type value_type;
    value_type val = lstl::move(*it);
    RandomAccessIterator hole = it;
    while (hole != first && comp(val, *(hole - 1))) {
      *hole = lstl::move(*(hole - 1));
      --hole;
    }
    *hole = lstl::move(val);
  }
}

template <typename RandomAccessIterator, typename Compare>
inline RandomAccessIterator sort_partition(RandomAccessIterator first,
                                           RandomAccessIterator last, Compare comp) {
  RandomAccessIterator pivot = first + (last - first) / 2;
  iter_swap(pivot, last - 1);
  RandomAccessIterator store = first;
  for (RandomAccessIterator it = first; it < last - 1; ++it) {
    if (comp(*it, *(last - 1))) {
      iter_swap(store, it);
      ++store;
    }
  }
  iter_swap(store, last - 1);
  return store;
}

template <typename RandomAccessIterator, typename Compare>
inline void sort_quick(RandomAccessIterator first, RandomAccessIterator last,
                       Compare comp) {
  while (last - first > 16) {
    RandomAccessIterator mid = sort_partition(first, last, comp);
    if (mid - first < last - (mid + 1)) {
      sort_quick(first, mid, comp);
      first = mid + 1;
    } else {
      sort_quick(mid + 1, last, comp);
      last = mid;
    }
  }
  if (first < last) {
    sort_insertion(first, last, comp);
  }
}

template <typename InputIterator1, typename InputIterator2, typename OutputIterator,
          typename Compare>
inline OutputIterator merge_copy(InputIterator1 first1, InputIterator1 last1,
                                 InputIterator2 first2, InputIterator2 last2,
                                 OutputIterator result, Compare comp) {
  for (; first1 != last1 && first2 != last2; ++result) {
    if (comp(*first2, *first1)) {
      *result = lstl::move(*first2);
      ++first2;
    } else {
      *result = lstl::move(*first1);
      ++first1;
    }
  }
  for (; first1 != last1; ++first1, ++result) {
    *result = lstl::move(*first1);
  }
  for (; first2 != last2; ++first2, ++result) {
    *result = lstl::move(*first2);
  }
  return result;
}

}  // namespace detail

// 堆算法见 heap.h（push_heap / pop_heap / make_heap / sort_heap）

template <typename RandomAccessIterator, typename Compare>
inline void sort(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
  if (first < last) {
    detail::sort_quick(first, last, comp);
  }
}

template <typename RandomAccessIterator>
inline void sort(RandomAccessIterator first, RandomAccessIterator last) {
  typedef typename detail::iterator_traits<RandomAccessIterator>::value_type value_type;
  sort(first, last, less<value_type>());
}

}  // namespace lstl

#endif  // LSTL_ALGORITHM_H
