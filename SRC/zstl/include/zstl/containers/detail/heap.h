// zstl heap algorithms — binary heap for priority_queue / container::sort
//
// All functions operate on random-access ranges [first, last).
// Default comparator: less<> (max-heap).
// Complexity:
//   make_heap: O(n)
//   push_heap: O(log n)
//   pop_heap:  O(log n)
//   sort_heap: O(n log n)
//   is_heap:   O(n)
#pragma once

#include "zstl/iterators/iterator_traits.h"
#include "zstl/memory/utility.h"

namespace zstl {
namespace detail {

// ============================================================
// Helper: sift-down / heapify at position idx
// Used by make_heap (bottom-up) and pop_heap (top-down).
// ============================================================
template<typename RandomIterator, typename Compare, typename DifferenceType>
void sift_down(RandomIterator first, DifferenceType len,
               DifferenceType idx, Compare comp) {
    while (true) {
        DifferenceType left = 2 * idx + 1;
        DifferenceType right = 2 * idx + 2;
        DifferenceType largest = idx;

        if (left < len && comp(*(first + largest), *(first + left)))
            largest = left;
        if (right < len && comp(*(first + largest), *(first + right)))
            largest = right;

        if (largest == idx) break;

        zstl::swap(*(first + idx), *(first + largest));
        idx = largest;
    }
}

// ============================================================
// Helper: sift-up from position idx
// Used by push_heap.
// ============================================================
template<typename RandomIterator, typename Compare, typename DifferenceType>
void sift_up(RandomIterator first, DifferenceType idx, Compare comp) {
    while (idx > 0) {
        DifferenceType parent = (idx - 1) / 2;
        if (!comp(*(first + parent), *(first + idx))) break;
        zstl::swap(*(first + parent), *(first + idx));
        idx = parent;
    }
}

} // namespace detail

// ============================================================
// push_heap — insert element at position (last - 1) into heap [first, last-1]
// Precondition: [first, last-1) is a valid heap.
// Postcondition: [first, last) is a valid heap.
// O(log n)
// ============================================================
template<typename RandomIterator, typename Compare>
void push_heap(RandomIterator first, RandomIterator last, Compare comp) {
    using difference_type = iterator_difference_t<RandomIterator>;
    difference_type idx = (last - first) - 1;
    detail::sift_up(first, idx, comp);
}

template<typename RandomIterator>
void push_heap(RandomIterator first, RandomIterator last) {
    push_heap(first, last, less<iterator_value_t<RandomIterator>>());
}

// ============================================================
// pop_heap — move top element to end and restore heap property
// Swaps *first and *(last-1), then sifts down.
// Postcondition: [first, last-1) is a valid heap, *(last-1) is the old top.
// O(log n)
// ============================================================
template<typename RandomIterator, typename Compare>
void pop_heap(RandomIterator first, RandomIterator last, Compare comp) {
    if (last - first <= 1) return;
    --last;
    zstl::swap(*first, *last);
    using difference_type = iterator_difference_t<RandomIterator>;
    difference_type len = last - first;
    detail::sift_down(first, len, difference_type(0), comp);
}

template<typename RandomIterator>
void pop_heap(RandomIterator first, RandomIterator last) {
    pop_heap(first, last, less<iterator_value_t<RandomIterator>>());
}

// ============================================================
// make_heap — build a heap from an unordered range [first, last)
// Uses Floyd's bottom-up heap construction.
// O(n)
// ============================================================
template<typename RandomIterator, typename Compare>
void make_heap(RandomIterator first, RandomIterator last, Compare comp) {
    using difference_type = iterator_difference_t<RandomIterator>;
    difference_type len = last - first;
    if (len <= 1) return;
    // Start from last non-leaf node and sift down
    for (difference_type i = (len / 2) - 1; i >= 0; --i) {
        detail::sift_down(first, len, i, comp);
    }
}

template<typename RandomIterator>
void make_heap(RandomIterator first, RandomIterator last) {
    make_heap(first, last, less<iterator_value_t<RandomIterator>>());
}

// ============================================================
// sort_heap — convert a heap [first, last) into a sorted range
// Repeatedly calls pop_heap, shrinking the heap.
// O(n log n)
// ============================================================
template<typename RandomIterator, typename Compare>
void sort_heap(RandomIterator first, RandomIterator last, Compare comp) {
    while (last - first > 1) {
        pop_heap(first, last--, comp);
    }
}

template<typename RandomIterator>
void sort_heap(RandomIterator first, RandomIterator last) {
    sort_heap(first, last, less<iterator_value_t<RandomIterator>>());
}

// ============================================================
// is_heap — check if [first, last) is a valid heap
// O(n)
// ============================================================
template<typename RandomIterator, typename Compare>
bool is_heap(RandomIterator first, RandomIterator last, Compare comp) {
    using difference_type = iterator_difference_t<RandomIterator>;
    difference_type len = last - first;
    for (difference_type i = 1; i < len; ++i) {
        difference_type parent = (i - 1) / 2;
        if (comp(*(first + parent), *(first + i)))
            return false;
    }
    return true;
}

template<typename RandomIterator>
bool is_heap(RandomIterator first, RandomIterator last) {
    return is_heap(first, last, less<iterator_value_t<RandomIterator>>());
}

// ============================================================
// is_heap_until — find first element violating heap property
// O(n)
// ============================================================
template<typename RandomIterator, typename Compare>
RandomIterator is_heap_until(RandomIterator first, RandomIterator last, Compare comp) {
    using difference_type = iterator_difference_t<RandomIterator>;
    difference_type len = last - first;
    for (difference_type i = 1; i < len; ++i) {
        difference_type parent = (i - 1) / 2;
        if (comp(*(first + parent), *(first + i)))
            return first + i;
    }
    return last;
}

template<typename RandomIterator>
RandomIterator is_heap_until(RandomIterator first, RandomIterator last) {
    return is_heap_until(first, last, less<iterator_value_t<RandomIterator>>());
}

} // namespace zstl
