/**
 * @file    heap.h
 * @brief   Binary heap algorithms: push_heap, pop_heap, make_heap, sort_heap.
 * @author  lstl team
 * @date    2025
 * @ingroup container_detail
 */
// Use of this source code is governed by a MIT-style license.
//
// heap.h - Binary heap algorithms (max-heap by default).
// Provides: push_heap, pop_heap, make_heap, sort_heap.

#pragma once

#include <iterator>

#include "../../memory/utility.h"
#include "../../memory/functional.h"

namespace lstl {
namespace detail {

////////////////////////////////////////////////////////////////////////////
// Heap index helpers
////////////////////////////////////////////////////////////////////////////
template <typename Distance>
inline Distance heap_parent(Distance i) { return (i - 1) / 2; }

template <typename Distance>
inline Distance heap_left_child(Distance i) { return 2 * i + 1; }

////////////////////////////////////////////////////////////////////////////
// push_heap - Insert element at position [last-1] into heap [first, last-1)
//
// The range [first, last-1) must already be a valid heap.
// After the call, [first, last) is a valid heap.
////////////////////////////////////////////////////////////////////////////
template <typename RandomAccessIterator, typename Compare>
void push_heap(RandomAccessIterator first, RandomAccessIterator last,
               Compare comp) {
    typedef typename std::iterator_traits<RandomAccessIterator>::difference_type distance_type;
    distance_type i = (last - first) - 1;
    distance_type parent = heap_parent(i);

    auto value = lstl::move(*(first + i));

    // Sift up
    while (i > 0 && comp(*(first + parent), value)) {
        *(first + i) = lstl::move(*(first + parent));
        i = parent;
        parent = heap_parent(i);
    }
    *(first + i) = lstl::move(value);
}

template <typename RandomAccessIterator>
void push_heap(RandomAccessIterator first, RandomAccessIterator last) {
    lstl::detail::push_heap(first, last,
        lstl::less<typename std::iterator_traits<RandomAccessIterator>::value_type>());
}

////////////////////////////////////////////////////////////////////////////
// pop_heap - Move the top element to position [last-1] and restore heap
// for [first, last-1).
////////////////////////////////////////////////////////////////////////////
template <typename RandomAccessIterator, typename Compare>
void pop_heap(RandomAccessIterator first, RandomAccessIterator last,
              Compare comp) {
    typedef typename std::iterator_traits<RandomAccessIterator>::difference_type distance_type;

    if (last - first <= 1) return;

    --last;
    auto value = lstl::move(*last);
    *last = lstl::move(*first);

    distance_type i = 0;
    distance_type n = last - first;
    distance_type child = heap_left_child(i);

    // Sift down
    while (child < n) {
        // Pick larger child
        if (child + 1 < n && comp(*(first + child), *(first + child + 1))) {
            ++child;
        }
        if (!comp(value, *(first + child))) break;
        *(first + i) = lstl::move(*(first + child));
        i = child;
        child = heap_left_child(i);
    }
    *(first + i) = lstl::move(value);
}

template <typename RandomAccessIterator>
void pop_heap(RandomAccessIterator first, RandomAccessIterator last) {
    lstl::detail::pop_heap(first, last,
        lstl::less<typename std::iterator_traits<RandomAccessIterator>::value_type>());
}

////////////////////////////////////////////////////////////////////////////
// make_heap - Transform [first, last) into a heap
////////////////////////////////////////////////////////////////////////////
template <typename RandomAccessIterator, typename Compare>
void make_heap(RandomAccessIterator first, RandomAccessIterator last,
               Compare comp) {
    typedef typename std::iterator_traits<RandomAccessIterator>::difference_type distance_type;
    distance_type n = last - first;
    if (n <= 1) return;

    // Start from last non-leaf parent (0-based: (n-2)/2) and sift down each node.
    // Must include index 0 (the root). Use underflow-safe unsigned loop.
    for (distance_type i = (n - 2) / 2; ; --i) {
        auto value = lstl::move(*(first + i));
        distance_type parent = i;
        distance_type child = heap_left_child(parent);

        while (child < n) {
            if (child + 1 < n && comp(*(first + child), *(first + child + 1))) {
                ++child;
            }
            if (!comp(value, *(first + child))) break;
            *(first + parent) = lstl::move(*(first + child));
            parent = child;
            child = heap_left_child(parent);
        }
        *(first + parent) = lstl::move(value);
        if (i == 0) break;
    }
}

template <typename RandomAccessIterator>
void make_heap(RandomAccessIterator first, RandomAccessIterator last) {
    lstl::detail::make_heap(first, last,
        lstl::less<typename std::iterator_traits<RandomAccessIterator>::value_type>());
}

////////////////////////////////////////////////////////////////////////////
// sort_heap - Sort a heap range [first, last)
// After call, [first, last) is sorted in ascending order.
////////////////////////////////////////////////////////////////////////////
template <typename RandomAccessIterator, typename Compare>
void sort_heap(RandomAccessIterator first, RandomAccessIterator last,
               Compare comp) {
    while (last - first > 1) {
        lstl::detail::pop_heap(first, last--, comp);
    }
}

template <typename RandomAccessIterator>
void sort_heap(RandomAccessIterator first, RandomAccessIterator last) {
    lstl::detail::sort_heap(first, last,
        lstl::less<typename std::iterator_traits<RandomAccessIterator>::value_type>());
}

} // namespace detail
} // namespace lstl
