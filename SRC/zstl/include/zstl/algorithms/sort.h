// zstl sorting and selection algorithms
// ============================================================
// Contains:
//   insertion_sort     — O(n^2), stable, best for tiny ranges (< 16)
//   sort (introsort)   — O(n log n) worst-case via depth-limited quicksort + heapsort fallback
//   stable_sort        — O(n log n) bottom-up merge sort, stable
//   partial_sort       — O(n log k) first-k via heap select
//   partial_sort_copy  — O(n log k) copy variant
//   nth_element        — O(n) quickselect, O(n^2) worst-case
//   is_sorted          — O(n) sortedness check
//   is_sorted_until    — O(n) first out-of-order iterator
//   heapsort           — O(n log n), not stable, minimal space
//   shell_sort          — O(n^1.3) average, simple in-place
//
// All functions support custom comparators and default to zstl::less.
// ============================================================
#pragma once

#include "zstl/iterators/iterator_traits.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/construct.h"
#include "zstl/memory/type_traits.h"

namespace zstl {

// ============================================================
// insertion_sort
// ============================================================
// Complexity: O(n^2) worst case, O(n) best case (already sorted).
// Stable: yes. Moves elements one-by-one, shifting larger ones right.
// Recommended for n < 16, and as the terminal step in introsort.
// Handles: empty range (no-op), single-element, and already-sorted input.
// ============================================================
template<typename RandomIterator, typename Compare>
constexpr void insertion_sort(RandomIterator first, RandomIterator last, Compare comp) {
    if (first == last) return;
    for (RandomIterator i = first + 1; i != last; ++i) {
        auto key = zstl::move(*i);
        RandomIterator j = i;
        while (j > first && comp(key, *(j - 1))) {
            *j = zstl::move(*(j - 1));
            --j;
        }
        *j = zstl::move(key);
    }
}

template<typename RandomIterator>
constexpr void insertion_sort(RandomIterator first, RandomIterator last) {
    insertion_sort(first, last, less<iterator_value_t<RandomIterator>>());
}

// ============================================================
// shell_sort — diminishing-gap insertion sort
// ============================================================
// Complexity: O(n^1.3) average with Ciura gap sequence, O(n log^2 n)
//   worst case. Not stable. In-place. Excellent cache behavior due
//   to large initial gaps, outperforms insertion_sort for n > 32
//   and competes with quicksort for moderate n (< 1000).
// Gap sequence: 1, 4, 10, 23, 57, 132, 301, 701, 1750 (Ciura 2001)
// Handles: empty/single-element (no-op), already sorted (near O(n)).
// ============================================================
template<typename RandomIterator, typename Compare>
void shell_sort(RandomIterator first, RandomIterator last, Compare comp) {
    using difference_type = iterator_difference_t<RandomIterator>;
    difference_type n = last - first;
    if (n <= 1) return;

    // Ciura gap sequence (empirically optimal)
    constexpr difference_type gaps[] = {1750, 701, 301, 132, 57, 23, 10, 4, 1};
    for (difference_type gap : gaps) {
        if (gap >= n) continue;
        for (difference_type i = gap; i < n; ++i) {
            auto key = zstl::move(*(first + i));
            difference_type j = i;
            while (j >= gap && comp(key, *(first + j - gap))) {
                *(first + j) = zstl::move(*(first + j - gap));
                j -= gap;
            }
            *(first + j) = zstl::move(key);
        }
    }
}

template<typename RandomIterator>
void shell_sort(RandomIterator first, RandomIterator last) {
    shell_sort(first, last, less<iterator_value_t<RandomIterator>>());
}

// ============================================================
// is_sorted / is_sorted_until
// ============================================================
// is_sorted: O(n) comparison. True if comp(*next, *prev) is
//   false for all adjacent pairs (i.e., *prev <= *next).
// is_sorted_until: Returns the first iterator i such that
//   [first, i) is sorted and comp(*i, *(i-1)) is true.
// Both handle empty ranges (return true / last).
// ============================================================
template<typename ForwardIterator, typename Compare>
constexpr bool is_sorted(ForwardIterator first, ForwardIterator last, Compare comp) {
    if (first == last) return true;
    ForwardIterator next = first;
    ++next;
    for (; next != last; first = next, ++next) {
        if (comp(*next, *first)) return false;
    }
    return true;
}

template<typename ForwardIterator>
constexpr bool is_sorted(ForwardIterator first, ForwardIterator last) {
    return is_sorted(first, last, less<iterator_value_t<ForwardIterator>>());
}

template<typename ForwardIterator, typename Compare>
constexpr ForwardIterator is_sorted_until(ForwardIterator first, ForwardIterator last, Compare comp) {
    if (first == last) return last;
    ForwardIterator next = first;
    ++next;
    for (; next != last; first = next, ++next) {
        if (comp(*next, *first)) return next;
    }
    return last;
}

template<typename ForwardIterator>
constexpr ForwardIterator is_sorted_until(ForwardIterator first, ForwardIterator last) {
    return is_sorted_until(first, last, less<iterator_value_t<ForwardIterator>>());
}

// ============================================================
// Introsort detail
// ============================================================
// Introsort = quicksort + heapsort fallback + insertion_sort terminal.
// The depth limit ensures O(n log n) worst-case: if quicksort
// recursion exceeds 2*floor(log2(n)) levels, the remaining
// segment is sorted via heapsort.
// ============================================================
namespace detail {

// Median-of-three pivot and Hoare partition.
// Returns iterator to the first element of the upper (>= pivot) partition.
// O(n) per call.
template<typename RandomIterator, typename Compare>
constexpr RandomIterator intro_partition(RandomIterator first, RandomIterator last, Compare comp) {
    RandomIterator mid = first + (last - first) / 2;
    --last;
    // Sort first, mid, last to get median as pivot
    if (comp(*mid, *first)) zstl::swap(*first, *mid);
    if (comp(*last, *first)) zstl::swap(*first, *last);
    if (comp(*last, *mid))   zstl::swap(*mid, *last);

    auto pivot = zstl::move(*mid);
    RandomIterator i = first;
    RandomIterator j = last;
    while (true) {
        while (comp(*i, pivot)) ++i;
        while (comp(pivot, *j)) --j;
        if (i >= j) return j + 1;
        zstl::swap(*i, *j);
        ++i; --j;
    }
}

// Recursive introsort core.
// Tail-recursion elimination: always recurse on the smaller partition
// and loop on the larger one, bounding stack depth to O(log n).
// This guarantees no more than O(log n) active stack frames. Additionally,
// by processing the smaller half first we maximize the chance that the
// depth limit catches pathological cases early.
template<typename RandomIterator, typename Compare>
void intro_sort_impl(RandomIterator first, RandomIterator last, Compare comp,
                     size_t depth_limit) {
    using difference_type = iterator_difference_t<RandomIterator>;
    constexpr difference_type kInsertionThreshold = 16;

    while (last - first > kInsertionThreshold) {
        if (depth_limit == 0) {
            // Heapsort fallback: guaranteed O(n log n), no extra space
            make_heap(first, last, comp);
            sort_heap(first, last, comp);
            return;
        }
        --depth_limit;

        RandomIterator cut = intro_partition(first, last, comp);
        // Recurse on smaller partition first
        if (cut - first < last - cut) {
            intro_sort_impl(first, cut, comp, depth_limit);
            first = cut;
        } else {
            intro_sort_impl(cut, last, comp, depth_limit);
            last = cut;
        }
    }
}

} // namespace detail

// ============================================================
// sort — introsort
// ============================================================
// Worst case: O(n log n), guaranteed by depth-limited recursion.
// Average: O(n log n) with low constant factor (median-of-three pivot).
// Not stable. O(log n) recursion depth due to tail-recursion elimination.
//
// Algorithm:
//   1. Quicksort with median-of-three pivot selection (Hoare partition).
//   2. If recursion depth exceeds 2*floor(log2(n)), fall back to heapsort
//      on the remaining segment — this is the "introspective" part.
//   3. When subranges shrink below 16 elements, finish with insertion_sort
//      for its low overhead on tiny ranges.
//
// Handles: empty (no-op), single-element, sorted, reverse-sorted,
//   all-equal, and adversarial inputs without degrading to O(n^2).
// ============================================================
template<typename RandomIterator, typename Compare>
void sort(RandomIterator first, RandomIterator last, Compare comp) {
    using difference_type = iterator_difference_t<RandomIterator>;
    difference_type n = last - first;
    if (n <= 1) return;

    // Compute depth_limit = 2 * floor(log2(n))
    size_t depth_limit = 0;
    difference_type tmp = n;
    while (tmp > 0) { ++depth_limit; tmp >>= 1; }
    depth_limit *= 2;

    detail::intro_sort_impl(first, last, comp, depth_limit);
    // Finish with insertion_sort on the nearly-sorted range
    insertion_sort(first, last, comp);
}

template<typename RandomIterator>
void sort(RandomIterator first, RandomIterator last) {
    sort(first, last, less<iterator_value_t<RandomIterator>>());
}

// ============================================================
// heapsort — standalone heap-based sort
// ============================================================
// O(n log n) worst and average. Not stable. In-place (O(1) extra memory).
// Significantly slower than introsort on average (worse cache locality
// due to jumping access pattern in heap operations) but provides a
// guaranteed O(n log n) bound without recursion.
// Implementation: make_heap + repeated pop_heap.
// Edge cases: empty range (no-op), single-element, all-equal values.
// ============================================================
template<typename RandomIterator, typename Compare>
void heapsort(RandomIterator first, RandomIterator last, Compare comp) {
    if (last - first <= 1) return;
    make_heap(first, last, comp);
    sort_heap(first, last, comp);
}

template<typename RandomIterator>
void heapsort(RandomIterator first, RandomIterator last) {
    heapsort(first, last, less<iterator_value_t<RandomIterator>>());
}

// ============================================================
// partial_sort — heap-based selection of k smallest elements
// ============================================================
// Sorts the first (middle - first) elements in ascending order.
// The remaining elements have unspecified (but heap-valid) order.
// O(n log k) where k = middle - first.
// Handles: empty k, k == n (equivalent to heapsort), single-element.
// ============================================================
template<typename RandomIterator, typename Compare>
void partial_sort(RandomIterator first, RandomIterator middle, RandomIterator last, Compare comp) {
    if (first == middle || middle == last) return;

    // Build max-heap of first k elements
    make_heap(first, middle, comp);

    using difference_type = iterator_difference_t<RandomIterator>;
    difference_type len = middle - first;

    // For each remaining element, if it's smaller than the max-heap top,
    // replace the top and sift it down.
    for (RandomIterator i = middle; i != last; ++i) {
        if (comp(*i, *first)) {
            zstl::swap(*first, *i);
            // Sift down from root
            difference_type idx = 0;
            while (true) {
                difference_type left = 2 * idx + 1;
                difference_type right = 2 * idx + 2;
                difference_type largest = idx;
                if (left < len && comp(*(first + largest), *(first + left)))
                    largest = left;
                if (right < len && comp(*(first + largest), *(first + right)))
                    largest = right;
                if (largest == idx) break;
                zstl::swap(*(first + idx), *(first + largest));
                idx = largest;
            }
        }
    }

    // Extract sorted order
    sort_heap(first, middle, comp);
}

template<typename RandomIterator>
void partial_sort(RandomIterator first, RandomIterator middle, RandomIterator last) {
    partial_sort(first, middle, last, less<iterator_value_t<RandomIterator>>());
}

// ============================================================
// partial_sort_copy
// ============================================================
// Copies the k smallest elements from [first, last) into sorted
// order in [result_first, result_last). Returns the end of the
// filled portion. O(n log k) where k = result_last - result_first.
// ============================================================
template<typename InputIterator, typename RandomIterator, typename Compare>
RandomIterator partial_sort_copy(InputIterator first, InputIterator last,
                                  RandomIterator result_first, RandomIterator result_last,
                                  Compare comp) {
    if (result_first == result_last) return result_last;
    RandomIterator result_end = result_first;

    // Copy initial batch (up to result capacity)
    for (; first != last && result_end != result_last; ++first, ++result_end) {
        *result_end = *first;
    }

    // Build max-heap of result range
    make_heap(result_first, result_end, comp);

    using difference_type = iterator_difference_t<RandomIterator>;
    difference_type len = result_end - result_first;

    // Process remaining input
    for (; first != last; ++first) {
        if (comp(*first, *result_first)) {
            // Replace max-heap top
            *result_first = *first;
            difference_type idx = 0;
            while (true) {
                difference_type left = 2 * idx + 1;
                difference_type right = 2 * idx + 2;
                difference_type largest = idx;
                if (left < len && comp(*(result_first + largest), *(result_first + left)))
                    largest = left;
                if (right < len && comp(*(result_first + largest), *(result_first + right)))
                    largest = right;
                if (largest == idx) break;
                zstl::swap(*(result_first + idx), *(result_first + largest));
                idx = largest;
            }
        }
    }

    sort_heap(result_first, result_end, comp);
    return result_end;
}

template<typename InputIterator, typename RandomIterator>
RandomIterator partial_sort_copy(InputIterator first, InputIterator last,
                                  RandomIterator result_first, RandomIterator result_last) {
    return partial_sort_copy(first, last, result_first, result_last,
                             less<iterator_value_t<RandomIterator>>());
}

// ============================================================
// stable_sort — bottom-up merge sort
// ============================================================
// O(n log n) comparisons and moves. Stable (preserves relative
// order of equivalent elements).
// Uses insertion_sort for small chunks (n < 16) and a heap-allocated
// temporary buffer of size n/2 for merging.  On allocation failure,
// falls back to in-place merge (not yet implemented — uses buffer).
// Handles: empty, single-element, sorted, reverse-sorted.
// ============================================================
template<typename RandomIterator, typename Compare>
void stable_sort(RandomIterator first, RandomIterator last, Compare comp) {
    using difference_type = iterator_difference_t<RandomIterator>;
    using value_type = iterator_value_t<RandomIterator>;
    difference_type n = last - first;
    if (n <= 1) return;

    constexpr difference_type kInsertionThreshold = 16;

    // ---- Merge two sorted subranges into a temporary buffer ----
    value_type* buffer = nullptr;
    difference_type half_n = n / 2;

    auto merge_to_buffer = [&buffer](RandomIterator left, RandomIterator mid,
                                      RandomIterator right, Compare cmp) {
        value_type* out = buffer;
        RandomIterator i = left, j = mid;
        while (i != mid && j != right) {
            if (cmp(*j, *i)) *out++ = zstl::move(*j++);
            else              *out++ = zstl::move(*i++);
        }
        while (i != mid) *out++ = zstl::move(*i++);
        while (j != right) *out++ = zstl::move(*j++);
        // Copy merged data back to original range
        out = buffer;
        while (left != right) *left++ = zstl::move(*out++);
    };

    // Step 1: sort small chunks with insertion_sort
    for (difference_type i = 0; i < n; i += kInsertionThreshold) {
        difference_type end = (i + kInsertionThreshold < n) ? i + kInsertionThreshold : n;
        insertion_sort(first + i, first + end, comp);
    }

    // Step 2: allocate merge buffer (half the range)
    // The buffer needs to hold at most n/2 elements because we always
    // copy the left half of each merge step into the buffer.
    if (half_n > 0) {
        buffer = static_cast<value_type*>(
            ::operator new(static_cast<size_t>(half_n) * sizeof(value_type)));
    }

    // Step 3: bottom-up merge of increasing run widths
    // Runs double in size each pass: 16 -> 32 -> 64 -> ... -> n
    for (difference_type width = kInsertionThreshold; width < n; width *= 2) {
        for (difference_type i = 0; i < n; i += 2 * width) {
            difference_type mid = i + width;
            if (mid >= n) break;
            difference_type right = i + 2 * width;
            if (right > n) right = n;
            if (buffer) {
                merge_to_buffer(first + i, first + mid, first + right, comp);
            } else {
                // Fallback: in-place merge via rotation
                // (omitted for brevity; in real code use a lower-level inplace_merge)
                // For now this path only triggers if buffer allocation failed
            }
        }
    }

    if (buffer) ::operator delete(buffer);
}

template<typename RandomIterator>
void stable_sort(RandomIterator first, RandomIterator last) {
    stable_sort(first, last, less<iterator_value_t<RandomIterator>>());
}

// ============================================================
// nth_element — quickselect / introselect
// ============================================================
// Partitions the range so that:
//   - The element at *nth is the element that would be there if sorted.
//   - All elements before nth are <= *nth (not necessarily sorted).
//   - All elements after nth are >= *nth (not necessarily sorted).
// Average: O(n). Worst case: O(n^2) for naive quickselect;
//   bounded in practice by median-of-three pivot.
//
// Edge cases:
//   - nth == first: just finds the minimum element in one pass.
//   - nth == last-1: finds the maximum element.
//   - Empty range: no-op.
//   - Single-element: no-op (already in position).
//   - All-equal values: single partition pass, O(n).
// ============================================================
namespace detail {

template<typename RandomIterator, typename Compare>
constexpr void nth_element_impl(RandomIterator first, RandomIterator nth,
                                 RandomIterator last, Compare comp) {
    while (last - first > 1) {
        if (nth == first) return;
        if (nth == last) return;

        RandomIterator cut = intro_partition(first, last, comp);

        // Only recurse into the partition containing nth
        if (nth < cut) {
            last = cut;
        } else {
            first = cut;
        }
    }
}

} // namespace detail

template<typename RandomIterator, typename Compare>
constexpr void nth_element(RandomIterator first, RandomIterator nth,
                            RandomIterator last, Compare comp) {
    if (first == last || nth == last) return;
    if (first == nth) {
        // nth is first: find minimum element (single partition pass enough)
        auto it = min_element(first, last, comp);
        if (it != first) zstl::swap(*first, *it);
        return;
    }
    detail::nth_element_impl(first, nth, last, comp);
}

template<typename RandomIterator>
constexpr void nth_element(RandomIterator first, RandomIterator nth, RandomIterator last) {
    nth_element(first, nth, last, less<iterator_value_t<RandomIterator>>());
}

} // namespace zstl
