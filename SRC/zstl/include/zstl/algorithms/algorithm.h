// zstl general algorithm suite — mutating, non-mutating, sorting-related, set operations, permutations
#pragma once

#include "zstl/iterators/iterator_traits.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/type_traits.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

namespace zstl {

// ============================================================
// Non-mutating sequence operations
// ============================================================

// for_each — apply function to each element in order
// O(n) function calls. Returns the function (moved).
template<typename InputIterator, typename Function>
constexpr Function for_each(InputIterator first, InputIterator last, Function f) {
    for (; first != last; ++first) {
        f(*first);
    }
    return f;
}

// for_each_n — apply function to first n elements
// O(n) function calls. Returns first + n.
template<typename InputIterator, typename Size, typename Function>
constexpr InputIterator for_each_n(InputIterator first, Size n, Function f) {
    for (Size i = 0; i < n; ++i, ++first) {
        f(*first);
    }
    return first;
}

// ============================================================
// Copy operations — with POD memmove optimization
// ============================================================

// copy — copy [first, last) to result
// O(n). Uses memmove for POD contiguous types.
template<typename InputIterator, typename OutputIterator>
constexpr OutputIterator copy(InputIterator first, InputIterator last, OutputIterator result) {
    using T = iterator_value_t<OutputIterator>;
    using InputT = iterator_value_t<InputIterator>;

    // Fast path: both iterators are contiguous pointers to trivially copyable types
    if constexpr (std::is_same_v<T, InputT> &&
                  is_pod_v<T> &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<InputIterator>> &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<OutputIterator>>) {
        if (first != last) {
            auto n = static_cast<size_t>(zstl::distance(first, last));
            __builtin_memmove(&*result, &*first, n * sizeof(T));
            return result + n;
        }
        return result;
    }
    for (; first != last; ++first, ++result) {
        *result = *first;
    }
    return result;
}

// copy_n — copy exactly n elements
// O(n)
template<typename InputIterator, typename Size, typename OutputIterator>
constexpr OutputIterator copy_n(InputIterator first, Size n, OutputIterator result) {
    if constexpr (is_pod_v<iterator_value_t<OutputIterator>>) {
        using T = iterator_value_t<OutputIterator>;
        __builtin_memmove(&*result, &*first, static_cast<size_t>(n) * sizeof(T));
        return result + n;
    }
    for (Size i = 0; i < n; ++i, ++first, ++result) {
        *result = *first;
    }
    return result;
}

// copy_if — copy elements satisfying predicate
// O(n)
template<typename InputIterator, typename OutputIterator, typename Predicate>
constexpr OutputIterator copy_if(InputIterator first, InputIterator last,
                                  OutputIterator result, Predicate pred) {
    for (; first != last; ++first) {
        if (pred(*first)) {
            *result = *first;
            ++result;
        }
    }
    return result;
}

// copy_backward — copy range backwards (safe for overlapping ranges)
// O(n). Uses memmove for POD contiguous types.
template<typename BidirectionalIterator1, typename BidirectionalIterator2>
constexpr BidirectionalIterator2 copy_backward(BidirectionalIterator1 first,
                                                BidirectionalIterator1 last,
                                                BidirectionalIterator2 result) {
    using T = iterator_value_t<BidirectionalIterator2>;
    using InputT = iterator_value_t<BidirectionalIterator1>;

    if constexpr (std::is_same_v<T, InputT> &&
                  is_pod_v<T> &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<BidirectionalIterator1>> &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<BidirectionalIterator2>>) {
        if (first != last) {
            auto n = static_cast<size_t>(zstl::distance(first, last));
            __builtin_memmove(&*(result - n), &*first, n * sizeof(T));
            return result - n;
        }
        return result;
    }
    while (first != last) {
        --last;
        --result;
        *result = *last;
    }
    return result;
}

// ============================================================
// Move operations
// ============================================================

// move — move [first, last) to result (destructive copy)
// O(n). Uses memmove for trivially relocatable types.
template<typename InputIterator, typename OutputIterator>
constexpr OutputIterator move(InputIterator first, InputIterator last, OutputIterator result) {
    using T = iterator_value_t<OutputIterator>;
    using InputT = iterator_value_t<InputIterator>;

    if constexpr (std::is_same_v<T, InputT> &&
                  is_trivially_relocatable_v<T> &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<InputIterator>> &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<OutputIterator>>) {
        if (first != last) {
            auto n = static_cast<size_t>(zstl::distance(first, last));
            __builtin_memmove(&*result, &*first, n * sizeof(T));
            return result + n;
        }
        return result;
    }
    for (; first != last; ++first, ++result) {
        *result = zstl::move(*first);
    }
    return result;
}

// move_backward — move range backwards
// O(n)
template<typename BidirectionalIterator1, typename BidirectionalIterator2>
constexpr BidirectionalIterator2 move_backward(BidirectionalIterator1 first,
                                                BidirectionalIterator1 last,
                                                BidirectionalIterator2 result) {
    using T = iterator_value_t<BidirectionalIterator2>;
    using InputT = iterator_value_t<BidirectionalIterator1>;

    if constexpr (std::is_same_v<T, InputT> &&
                  is_trivially_relocatable_v<T> &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<BidirectionalIterator1>> &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<BidirectionalIterator2>>) {
        if (first != last) {
            auto n = static_cast<size_t>(zstl::distance(first, last));
            __builtin_memmove(&*(result - n), &*first, n * sizeof(T));
            return result - n;
        }
        return result;
    }
    while (first != last) {
        --last;
        --result;
        *result = zstl::move(*last);
    }
    return result;
}

// ============================================================
// Fill operations
// ============================================================

// fill — assign value to every element in range
// O(n)
template<typename ForwardIterator, typename T>
constexpr void fill(ForwardIterator first, ForwardIterator last, const T& value) {
    using VT = iterator_value_t<ForwardIterator>;
    if constexpr (is_pod_v<VT> && sizeof(VT) == 1 &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<ForwardIterator>>) {
        std::memset(&*first, static_cast<unsigned char>(static_cast<int>(value)),
                    static_cast<size_t>(zstl::distance(first, last)));
        return;
    }
    for (; first != last; ++first) {
        *first = value;
    }
}

// fill_n — assign value to n elements starting at first
// O(n)
template<typename OutputIterator, typename Size, typename T>
constexpr OutputIterator fill_n(OutputIterator first, Size n, const T& value) {
    using VT = iterator_value_t<OutputIterator>;
    if constexpr (is_pod_v<VT> && sizeof(VT) == 1 &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<OutputIterator>>) {
        std::memset(&*first, static_cast<unsigned char>(static_cast<int>(value)),
                    static_cast<size_t>(n));
        return first + n;
    }
    for (Size i = 0; i < n; ++i, ++first) {
        *first = value;
    }
    return first;
}

// ============================================================
// Transform — apply function to each element, store results
// ============================================================

// transform (unary) — result = f(*first)
// O(n)
template<typename InputIterator, typename OutputIterator, typename UnaryOperation>
constexpr OutputIterator transform(InputIterator first, InputIterator last,
                                    OutputIterator result, UnaryOperation op) {
    for (; first != last; ++first, ++result) {
        *result = op(*first);
    }
    return result;
}

// transform (binary) — result = f(*first1, *first2)
// O(n)
template<typename InputIterator1, typename InputIterator2,
         typename OutputIterator, typename BinaryOperation>
constexpr OutputIterator transform(InputIterator1 first1, InputIterator1 last1,
                                    InputIterator2 first2, OutputIterator result,
                                    BinaryOperation op) {
    for (; first1 != last1; ++first1, ++first2, ++result) {
        *result = op(*first1, *first2);
    }
    return result;
}

// ============================================================
// Generate — assign results of generator to range
// ============================================================

// generate — fill range with gen()
// O(n) calls to gen
template<typename ForwardIterator, typename Generator>
constexpr void generate(ForwardIterator first, ForwardIterator last, Generator gen) {
    for (; first != last; ++first) {
        *first = gen();
    }
}

// generate_n — fill n elements with gen()
// O(n)
template<typename OutputIterator, typename Size, typename Generator>
constexpr OutputIterator generate_n(OutputIterator first, Size n, Generator gen) {
    for (Size i = 0; i < n; ++i, ++first) {
        *first = gen();
    }
    return first;
}

// ============================================================
// Remove — reorder elements, removing those matching value/predicate
// ============================================================

// remove — move non-removed elements to front, return new end
// O(n). Stable: relative order of kept elements preserved.
template<typename ForwardIterator, typename T>
constexpr ForwardIterator remove(ForwardIterator first, ForwardIterator last, const T& value) {
    first = find(first, last, value);
    if (first == last) return first;
    ForwardIterator result = first;
    ++first;
    for (; first != last; ++first) {
        if (!(*first == value)) {
            *result = zstl::move(*first);
            ++result;
        }
    }
    return result;
}

// remove_if — remove elements satisfying predicate
// O(n). Stable.
template<typename ForwardIterator, typename Predicate>
constexpr ForwardIterator remove_if(ForwardIterator first, ForwardIterator last, Predicate pred) {
    first = find_if(first, last, pred);
    if (first == last) return first;
    ForwardIterator result = first;
    ++first;
    for (; first != last; ++first) {
        if (!pred(*first)) {
            *result = zstl::move(*first);
            ++result;
        }
    }
    return result;
}

// remove_copy — copy non-removed elements to result
// O(n)
template<typename InputIterator, typename OutputIterator, typename T>
constexpr OutputIterator remove_copy(InputIterator first, InputIterator last,
                                      OutputIterator result, const T& value) {
    for (; first != last; ++first) {
        if (!(*first == value)) {
            *result = *first;
            ++result;
        }
    }
    return result;
}

// remove_copy_if — copy elements not satisfying predicate
// O(n)
template<typename InputIterator, typename OutputIterator, typename Predicate>
constexpr OutputIterator remove_copy_if(InputIterator first, InputIterator last,
                                         OutputIterator result, Predicate pred) {
    for (; first != last; ++first) {
        if (!pred(*first)) {
            *result = *first;
            ++result;
        }
    }
    return result;
}

// ============================================================
// Replace
// ============================================================

// replace — replace all occurrences of old_value with new_value
// O(n)
template<typename ForwardIterator, typename T>
constexpr void replace(ForwardIterator first, ForwardIterator last,
                        const T& old_value, const T& new_value) {
    for (; first != last; ++first) {
        if (*first == old_value) *first = new_value;
    }
}

// replace_if — replace elements satisfying predicate
// O(n)
template<typename ForwardIterator, typename Predicate, typename T>
constexpr void replace_if(ForwardIterator first, ForwardIterator last,
                           Predicate pred, const T& new_value) {
    for (; first != last; ++first) {
        if (pred(*first)) *first = new_value;
    }
}

// replace_copy — copy range, replacing old_value with new_value
// O(n)
template<typename InputIterator, typename OutputIterator, typename T>
constexpr OutputIterator replace_copy(InputIterator first, InputIterator last,
                                       OutputIterator result,
                                       const T& old_value, const T& new_value) {
    for (; first != last; ++first, ++result) {
        *result = (*first == old_value) ? new_value : *first;
    }
    return result;
}

// replace_copy_if — copy range, replacing satisfying elements
// O(n)
template<typename InputIterator, typename OutputIterator,
         typename Predicate, typename T>
constexpr OutputIterator replace_copy_if(InputIterator first, InputIterator last,
                                          OutputIterator result,
                                          Predicate pred, const T& new_value) {
    for (; first != last; ++first, ++result) {
        *result = pred(*first) ? new_value : *first;
    }
    return result;
}

// ============================================================
// Reverse
// ============================================================

// reverse — reverse elements in-place
// O(n/2) swaps
template<typename BidirectionalIterator>
constexpr void reverse(BidirectionalIterator first, BidirectionalIterator last) {
    while (first != last && first != --last) {
        zstl::swap(*first, *last);
        ++first;
    }
}

// reverse_copy — copy range in reverse order
// O(n)
template<typename BidirectionalIterator, typename OutputIterator>
constexpr OutputIterator reverse_copy(BidirectionalIterator first,
                                       BidirectionalIterator last,
                                       OutputIterator result) {
    while (first != last) {
        --last;
        *result = *last;
        ++result;
    }
    return result;
}

// ============================================================
// Rotate — left-rotate so middle becomes first
// ============================================================

namespace detail {

// GCD for rotate
constexpr ptrdiff_t rotate_gcd(ptrdiff_t a, ptrdiff_t b) noexcept {
    while (b != 0) {
        ptrdiff_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

} // namespace detail

// rotate — left-rotate by (middle - first) positions
// O(n) swaps using the juggling algorithm
template<typename ForwardIterator>
constexpr ForwardIterator rotate(ForwardIterator first,
                                  ForwardIterator middle,
                                  ForwardIterator last) {
    if (first == middle) return last;
    if (middle == last) return first;

    using difference_type = iterator_difference_t<ForwardIterator>;

    // For random-access, use the GCD juggling algorithm
    if constexpr (std::is_base_of_v<random_access_iterator_tag,
                                     iterator_category_t<ForwardIterator>>) {
        difference_type n = last - first;
        difference_type k = middle - first;
        difference_type g = detail::rotate_gcd(n, k);

        for (difference_type i = 0; i < g; ++i) {
            auto tmp = zstl::move(*(first + i));
            difference_type curr = i;
            while (true) {
                difference_type next = curr + k;
                if (next >= n) next -= n;
                if (next == i) break;
                *(first + curr) = zstl::move(*(first + next));
                curr = next;
            }
            *(first + curr) = zstl::move(tmp);
        }
        return first + (n - k);
    }

    // For forward iterators, use the simpler three-reverse method:
    // reverse [first, middle), reverse [middle, last), reverse [first, last)
    // This requires bidirectional iterators, so fall back to a shift-based approach
    // Actually, three-reverse needs bidirectional.
    // For forward-only, use a shift:
    ForwardIterator next = middle;
    while (first != next) {
        zstl::swap(*first++, *next++);
        if (next == last) next = middle;
        else if (first == middle) middle = next;
    }
    return first;
}

// rotate_copy — copy rotated range to result
// O(n)
template<typename ForwardIterator, typename OutputIterator>
constexpr OutputIterator rotate_copy(ForwardIterator first,
                                      ForwardIterator middle,
                                      ForwardIterator last,
                                      OutputIterator result) {
    result = copy(middle, last, result);
    return copy(first, middle, result);
}

// ============================================================
// Unique — remove consecutive duplicates
// ============================================================

// unique — remove consecutive duplicates in-place
// O(n). Stable.
template<typename ForwardIterator, typename BinaryPredicate>
constexpr ForwardIterator unique(ForwardIterator first, ForwardIterator last,
                                  BinaryPredicate pred) {
    if (first == last) return last;
    ForwardIterator result = first;
    ++first;
    for (; first != last; ++first) {
        if (!pred(*result, *first)) {
            ++result;
            if (result != first) *result = zstl::move(*first);
        }
    }
    return ++result;
}

template<typename ForwardIterator>
constexpr ForwardIterator unique(ForwardIterator first, ForwardIterator last) {
    return unique(first, last, equal_to<void>());
}

// unique_copy — copy range, skipping consecutive duplicates
// O(n)
template<typename InputIterator, typename OutputIterator, typename BinaryPredicate>
constexpr OutputIterator unique_copy(InputIterator first, InputIterator last,
                                      OutputIterator result, BinaryPredicate pred) {
    if (first == last) return result;
    *result = *first;
    ++first;
    while (first != last) {
        if (!pred(*result, *first)) {
            ++result;
            *result = *first;
        }
        ++first;
    }
    return ++result;
}

template<typename InputIterator, typename OutputIterator>
constexpr OutputIterator unique_copy(InputIterator first, InputIterator last,
                                      OutputIterator result) {
    return unique_copy(first, last, result, equal_to<void>());
}

// ============================================================
// Partition
// ============================================================

// partition — reorder elements so pred(e) == true come first
// O(n) swaps. Not stable.
template<typename ForwardIterator, typename Predicate>
constexpr ForwardIterator partition(ForwardIterator first, ForwardIterator last, Predicate pred) {
    // Find first false element
    first = find_if_not(first, last, pred);
    if (first == last) return first;

    // Scan forward, swapping true elements to the front
    for (ForwardIterator i = zstl::next(first); i != last; ++i) {
        if (pred(*i)) {
            zstl::swap(*first, *i);
            ++first;
        }
    }
    return first;
}

// stable_partition — reorder preserving relative order
// O(n) with buffer, O(n log n) in-place worst case.
template<typename BidirectionalIterator, typename Predicate>
BidirectionalIterator stable_partition(BidirectionalIterator first,
                                        BidirectionalIterator last, Predicate pred) {
    using difference_type = iterator_difference_t<BidirectionalIterator>;
    using value_type = iterator_value_t<BidirectionalIterator>;
    difference_type n = zstl::distance(first, last);
    if (n <= 1) return pred(*first) ? last : first;

    // Use a temporary buffer for O(n) stable partition
    value_type* buf = static_cast<value_type*>(
        ::operator new(static_cast<size_t>(n) * sizeof(value_type)));
    value_type* buf_true = buf;
    value_type* buf_false = buf;

    // First pass: count false elements to place them correctly
    BidirectionalIterator it = first;
    while (it != last) {
        if (pred(*it)) {
            *buf_true++ = zstl::move(*it);
        }
        ++it;
    }
    difference_type true_count = buf_true - buf;
    buf_false = buf + true_count;
    it = first;
    while (it != last) {
        if (!pred(*it)) {
            *buf_false++ = zstl::move(*it);
        }
        ++it;
    }

    // Copy back
    zstl::move(buf, buf + static_cast<size_t>(n), first);
    ::operator delete(buf);

    return first + true_count;
}

// partition_point — find partition point in partitioned range
// O(log n)
template<typename ForwardIterator, typename Predicate>
constexpr ForwardIterator partition_point(ForwardIterator first, ForwardIterator last,
                                           Predicate pred) {
    using difference_type = iterator_difference_t<ForwardIterator>;
    difference_type n = zstl::distance(first, last);
    while (n > 0) {
        difference_type half = n / 2;
        ForwardIterator mid = first;
        zstl::advance(mid, half);
        if (pred(*mid)) {
            first = ++mid;
            n -= half + 1;
        } else {
            n = half;
        }
    }
    return first;
}

// partition_copy — copy true elements to out_true, false to out_false
// O(n)
template<typename InputIterator, typename OutputIterator1,
         typename OutputIterator2, typename Predicate>
constexpr pair<OutputIterator1, OutputIterator2>
partition_copy(InputIterator first, InputIterator last,
               OutputIterator1 out_true, OutputIterator2 out_false, Predicate pred) {
    for (; first != last; ++first) {
        if (pred(*first)) {
            *out_true = *first;
            ++out_true;
        } else {
            *out_false = *first;
            ++out_false;
        }
    }
    return {out_true, out_false};
}

// is_partitioned — check if range is partitioned by pred
// O(n)
template<typename InputIterator, typename Predicate>
constexpr bool is_partitioned(InputIterator first, InputIterator last, Predicate pred) {
    for (; first != last; ++first) {
        if (!pred(*first)) break;
    }
    for (; first != last; ++first) {
        if (pred(*first)) return false;
    }
    return true;
}

// ============================================================
// Merge (sorted range operations)
// ============================================================

// merge — merge two sorted ranges into one sorted range
// O(n + m). Stable: preserves order of equivalent elements.
template<typename InputIterator1, typename InputIterator2,
         typename OutputIterator, typename Compare>
constexpr OutputIterator merge(InputIterator1 first1, InputIterator1 last1,
                                InputIterator2 first2, InputIterator2 last2,
                                OutputIterator result, Compare comp) {
    while (first1 != last1 && first2 != last2) {
        if (comp(*first2, *first1)) {
            *result = zstl::move(*first2);
            ++first2;
        } else {
            *result = zstl::move(*first1);
            ++first1;
        }
        ++result;
    }
    result = zstl::move(first1, last1, result);
    result = zstl::move(first2, last2, result);
    return result;
}

template<typename InputIterator1, typename InputIterator2, typename OutputIterator>
constexpr OutputIterator merge(InputIterator1 first1, InputIterator1 last1,
                                InputIterator2 first2, InputIterator2 last2,
                                OutputIterator result) {
    return merge(first1, last1, first2, last2, result,
                 less<iterator_value_t<InputIterator1>>());
}

// inplace_merge — merge two consecutive sorted subranges in-place
// [first, middle) and [middle, last) are both sorted.
// O(n) with buffer, O(n log n) worst case.
template<typename BidirectionalIterator, typename Compare>
void inplace_merge(BidirectionalIterator first,
                    BidirectionalIterator middle,
                    BidirectionalIterator last, Compare comp) {
    if (first == middle || middle == last) return;

    using value_type = iterator_value_t<BidirectionalIterator>;
    using difference_type = iterator_difference_t<BidirectionalIterator>;
    difference_type n = zstl::distance(first, last);

    // Use buffer-based merge for simplicity and performance
    difference_type len1 = zstl::distance(first, middle);
    value_type* buf = static_cast<value_type*>(
        ::operator new(static_cast<size_t>(len1) * sizeof(value_type)));

    // Copy left half to buffer
    zstl::move(first, middle, buf);

    value_type* buf_cur = buf;
    value_type* buf_end = buf + len1;
    BidirectionalIterator out = first;
    BidirectionalIterator right = middle;

    while (buf_cur != buf_end && right != last) {
        if (comp(*right, *buf_cur)) {
            *out = zstl::move(*right);
            ++right;
        } else {
            *out = zstl::move(*buf_cur);
            ++buf_cur;
        }
        ++out;
    }
    zstl::move(buf_cur, buf_end, out);
    ::operator delete(buf);
}

template<typename BidirectionalIterator>
void inplace_merge(BidirectionalIterator first,
                   BidirectionalIterator middle,
                   BidirectionalIterator last) {
    inplace_merge(first, middle, last,
                  less<iterator_value_t<BidirectionalIterator>>());
}

// ============================================================
// Set operations (on sorted ranges)
// ============================================================

// includes — test if all elements of [first2, last2) are in [first1, last1)
// O(n + m)
template<typename InputIterator1, typename InputIterator2, typename Compare>
constexpr bool includes(InputIterator1 first1, InputIterator1 last1,
                         InputIterator2 first2, InputIterator2 last2, Compare comp) {
    while (first2 != last2) {
        if (first1 == last1 || comp(*first2, *first1)) return false;
        if (!comp(*first1, *first2)) ++first2;
        ++first1;
    }
    return true;
}

template<typename InputIterator1, typename InputIterator2>
constexpr bool includes(InputIterator1 first1, InputIterator1 last1,
                         InputIterator2 first2, InputIterator2 last2) {
    return includes(first1, last1, first2, last2, less<void>());
}

// set_union — union of two sorted ranges
// O(n + m). Stable.
template<typename InputIterator1, typename InputIterator2,
         typename OutputIterator, typename Compare>
constexpr OutputIterator set_union(InputIterator1 first1, InputIterator1 last1,
                                    InputIterator2 first2, InputIterator2 last2,
                                    OutputIterator result, Compare comp) {
    while (first1 != last1 && first2 != last2) {
        if (comp(*first1, *first2)) {
            *result = *first1; ++first1;
        } else if (comp(*first2, *first1)) {
            *result = *first2; ++first2;
        } else {
            *result = *first1; ++first1; ++first2;
        }
        ++result;
    }
    result = copy(first1, last1, result);
    result = copy(first2, last2, result);
    return result;
}

template<typename InputIterator1, typename InputIterator2, typename OutputIterator>
constexpr OutputIterator set_union(InputIterator1 first1, InputIterator1 last1,
                                    InputIterator2 first2, InputIterator2 last2,
                                    OutputIterator result) {
    return set_union(first1, last1, first2, last2, result,
                     less<iterator_value_t<InputIterator1>>());
}

// set_intersection — intersection of two sorted ranges
// O(n + m)
template<typename InputIterator1, typename InputIterator2,
         typename OutputIterator, typename Compare>
constexpr OutputIterator set_intersection(InputIterator1 first1, InputIterator1 last1,
                                           InputIterator2 first2, InputIterator2 last2,
                                           OutputIterator result, Compare comp) {
    while (first1 != last1 && first2 != last2) {
        if (comp(*first1, *first2)) {
            ++first1;
        } else if (comp(*first2, *first1)) {
            ++first2;
        } else {
            *result = *first1; ++first1; ++first2;
            ++result;
        }
    }
    return result;
}

template<typename InputIterator1, typename InputIterator2, typename OutputIterator>
constexpr OutputIterator set_intersection(InputIterator1 first1, InputIterator1 last1,
                                           InputIterator2 first2, InputIterator2 last2,
                                           OutputIterator result) {
    return set_intersection(first1, last1, first2, last2, result,
                            less<iterator_value_t<InputIterator1>>());
}

// set_difference — elements in [first1, last1) but not in [first2, last2)
// O(n + m)
template<typename InputIterator1, typename InputIterator2,
         typename OutputIterator, typename Compare>
constexpr OutputIterator set_difference(InputIterator1 first1, InputIterator1 last1,
                                         InputIterator2 first2, InputIterator2 last2,
                                         OutputIterator result, Compare comp) {
    while (first1 != last1 && first2 != last2) {
        if (comp(*first1, *first2)) {
            *result = *first1; ++first1; ++result;
        } else if (comp(*first2, *first1)) {
            ++first2;
        } else {
            ++first1; ++first2;
        }
    }
    return copy(first1, last1, result);
}

template<typename InputIterator1, typename InputIterator2, typename OutputIterator>
constexpr OutputIterator set_difference(InputIterator1 first1, InputIterator1 last1,
                                         InputIterator2 first2, InputIterator2 last2,
                                         OutputIterator result) {
    return set_difference(first1, last1, first2, last2, result,
                          less<iterator_value_t<InputIterator1>>());
}

// set_symmetric_difference — elements in exactly one of the two ranges
// O(n + m)
template<typename InputIterator1, typename InputIterator2,
         typename OutputIterator, typename Compare>
constexpr OutputIterator set_symmetric_difference(InputIterator1 first1, InputIterator1 last1,
                                                   InputIterator2 first2, InputIterator2 last2,
                                                   OutputIterator result, Compare comp) {
    while (first1 != last1 && first2 != last2) {
        if (comp(*first1, *first2)) {
            *result = *first1; ++first1; ++result;
        } else if (comp(*first2, *first1)) {
            *result = *first2; ++first2; ++result;
        } else {
            ++first1; ++first2;
        }
    }
    result = copy(first1, last1, result);
    result = copy(first2, last2, result);
    return result;
}

template<typename InputIterator1, typename InputIterator2, typename OutputIterator>
constexpr OutputIterator set_symmetric_difference(InputIterator1 first1, InputIterator1 last1,
                                                   InputIterator2 first2, InputIterator2 last2,
                                                   OutputIterator result) {
    return set_symmetric_difference(first1, last1, first2, last2, result,
                                    less<iterator_value_t<InputIterator1>>());
}

// ============================================================
// Binary search (on sorted ranges)
// ============================================================

// lower_bound — first position where element >= value (or would be inserted)
// O(log n) for random-access, O(n) for non-random-access
template<typename ForwardIterator, typename T, typename Compare>
constexpr ForwardIterator lower_bound(ForwardIterator first, ForwardIterator last,
                                       const T& value, Compare comp) {
    using difference_type = iterator_difference_t<ForwardIterator>;
    difference_type len = zstl::distance(first, last);
    while (len > 0) {
        difference_type half = len / 2;
        ForwardIterator mid = first;
        zstl::advance(mid, half);
        if (comp(*mid, value)) {
            first = ++mid;
            len -= half + 1;
        } else {
            len = half;
        }
    }
    return first;
}

template<typename ForwardIterator, typename T>
constexpr ForwardIterator lower_bound(ForwardIterator first, ForwardIterator last,
                                       const T& value) {
    return lower_bound(first, last, value, less<void>());
}

// upper_bound — first position where element > value
// O(log n)
template<typename ForwardIterator, typename T, typename Compare>
constexpr ForwardIterator upper_bound(ForwardIterator first, ForwardIterator last,
                                       const T& value, Compare comp) {
    using difference_type = iterator_difference_t<ForwardIterator>;
    difference_type len = zstl::distance(first, last);
    while (len > 0) {
        difference_type half = len / 2;
        ForwardIterator mid = first;
        zstl::advance(mid, half);
        if (comp(value, *mid)) {
            len = half;
        } else {
            first = ++mid;
            len -= half + 1;
        }
    }
    return first;
}

template<typename ForwardIterator, typename T>
constexpr ForwardIterator upper_bound(ForwardIterator first, ForwardIterator last,
                                       const T& value) {
    return upper_bound(first, last, value, less<void>());
}

// binary_search — test if value exists in sorted range
// O(log n)
template<typename ForwardIterator, typename T, typename Compare>
constexpr bool binary_search(ForwardIterator first, ForwardIterator last,
                              const T& value, Compare comp) {
    first = lower_bound(first, last, value, comp);
    return first != last && !comp(value, *first);
}

template<typename ForwardIterator, typename T>
constexpr bool binary_search(ForwardIterator first, ForwardIterator last,
                              const T& value) {
    return binary_search(first, last, value, less<void>());
}

// equal_range — returns [lower_bound, upper_bound) pair
// O(log n)
template<typename ForwardIterator, typename T, typename Compare>
constexpr pair<ForwardIterator, ForwardIterator>
equal_range(ForwardIterator first, ForwardIterator last,
            const T& value, Compare comp) {
    return {lower_bound(first, last, value, comp),
            upper_bound(first, last, value, comp)};
}

template<typename ForwardIterator, typename T>
constexpr pair<ForwardIterator, ForwardIterator>
equal_range(ForwardIterator first, ForwardIterator last, const T& value) {
    return equal_range(first, last, value, less<void>());
}

// ============================================================
// Min/max element
// ============================================================

// min_element — iterator to smallest element
// O(n). Returns last if empty.
template<typename ForwardIterator, typename Compare>
constexpr ForwardIterator min_element(ForwardIterator first, ForwardIterator last, Compare comp) {
    if (first == last) return last;
    ForwardIterator result = first;
    ++first;
    for (; first != last; ++first) {
        if (comp(*first, *result)) result = first;
    }
    return result;
}

template<typename ForwardIterator>
constexpr ForwardIterator min_element(ForwardIterator first, ForwardIterator last) {
    return min_element(first, last, less<iterator_value_t<ForwardIterator>>());
}

// max_element — iterator to largest element
// O(n)
template<typename ForwardIterator, typename Compare>
constexpr ForwardIterator max_element(ForwardIterator first, ForwardIterator last, Compare comp) {
    if (first == last) return last;
    ForwardIterator result = first;
    ++first;
    for (; first != last; ++first) {
        if (comp(*result, *first)) result = first;
    }
    return result;
}

template<typename ForwardIterator>
constexpr ForwardIterator max_element(ForwardIterator first, ForwardIterator last) {
    return max_element(first, last, less<iterator_value_t<ForwardIterator>>());
}

// minmax_element — returns pair {min, max} iterators
// O(3n/2) comparisons
template<typename ForwardIterator, typename Compare>
constexpr pair<ForwardIterator, ForwardIterator>
minmax_element(ForwardIterator first, ForwardIterator last, Compare comp) {
    if (first == last) return {last, last};
    ForwardIterator min_it = first, max_it = first;
    ++first;
    while (first != last) {
        ForwardIterator next = first;
        ++next;
        if (next == last) {
            // Only one element left
            if (comp(*first, *min_it)) min_it = first;
            if (!comp(*first, *max_it)) max_it = first;
            break;
        }
        // Compare pair: first vs next
        if (comp(*first, *next)) {
            if (comp(*first, *min_it)) min_it = first;
            if (!comp(*next, *max_it)) max_it = next;
        } else {
            if (comp(*next, *min_it)) min_it = next;
            if (!comp(*first, *max_it)) max_it = first;
        }
        first = ++next;
    }
    return {min_it, max_it};
}

template<typename ForwardIterator>
constexpr pair<ForwardIterator, ForwardIterator>
minmax_element(ForwardIterator first, ForwardIterator last) {
    return minmax_element(first, last, less<iterator_value_t<ForwardIterator>>());
}

// ============================================================
// Permutations
// ============================================================

// is_permutation — test if two ranges contain the same elements (possibly reordered)
// O(n^2) worst case without additional memory, O(n log n) with.
template<typename ForwardIterator1, typename ForwardIterator2,
         typename BinaryPredicate>
constexpr bool is_permutation(ForwardIterator1 first1, ForwardIterator1 last1,
                               ForwardIterator2 first2, BinaryPredicate pred) {
    // Fast path: skip common prefix
    auto [m1, m2] = mismatch(first1, last1, first2, pred);
    first1 = m1; first2 = m2;

    if (first1 == last1) return true;

    // Check if remaining elements are a permutation
    ForwardIterator2 last2 = first2;
    zstl::advance(last2, zstl::distance(first1, last1));

    for (ForwardIterator1 it = first1; it != last1; ++it) {
        // Count occurrences of *it in [first1, last1)
        auto count_in_1 = [&](const auto& val) {
            decltype(zstl::distance(first1, last1)) c = 0;
            for (auto i = first1; i != last1; ++i)
                if (pred(*i, val)) ++c;
            return c;
        };
        // Count occurrences in [first2, last2)
        auto count_in_2 = [&](const auto& val) {
            decltype(zstl::distance(first2, last2)) c = 0;
            for (auto i = first2; i != last2; ++i)
                if (pred(*i, val)) ++c;
            return c;
        };
        if (count_in_1(*it) != count_in_2(*it)) return false;
    }
    return true;
}

template<typename ForwardIterator1, typename ForwardIterator2>
constexpr bool is_permutation(ForwardIterator1 first1, ForwardIterator1 last1,
                               ForwardIterator2 first2) {
    return is_permutation(first1, last1, first2, equal_to<void>());
}

// next_permutation — transform range to lexicographically next permutation
// O(n) worst case. Returns false if already at last permutation (wraps to first).
template<typename BidirectionalIterator, typename Compare>
constexpr bool next_permutation(BidirectionalIterator first,
                                 BidirectionalIterator last, Compare comp) {
    if (first == last) return false;
    BidirectionalIterator i = last;
    if (first == --i) return false; // single element

    while (true) {
        BidirectionalIterator i1 = i;
        --i;
        if (comp(*i, *i1)) {
            BidirectionalIterator i2 = last;
            --i2;
            while (!comp(*i, *i2)) --i2;
            zstl::swap(*i, *i2);
            zstl::reverse(i1, last);
            return true;
        }
        if (i == first) {
            zstl::reverse(first, last);
            return false;
        }
    }
}

template<typename BidirectionalIterator>
constexpr bool next_permutation(BidirectionalIterator first, BidirectionalIterator last) {
    return next_permutation(first, last,
                            less<iterator_value_t<BidirectionalIterator>>());
}

// prev_permutation — transform range to lexicographically previous permutation
// O(n)
template<typename BidirectionalIterator, typename Compare>
constexpr bool prev_permutation(BidirectionalIterator first,
                                 BidirectionalIterator last, Compare comp) {
    if (first == last) return false;
    BidirectionalIterator i = last;
    if (first == --i) return false;

    while (true) {
        BidirectionalIterator i1 = i;
        --i;
        if (comp(*i1, *i)) {
            BidirectionalIterator i2 = last;
            --i2;
            while (!comp(*i2, *i)) --i2;
            zstl::swap(*i, *i2);
            zstl::reverse(i1, last);
            return true;
        }
        if (i == first) {
            zstl::reverse(first, last);
            return false;
        }
    }
}

template<typename BidirectionalIterator>
constexpr bool prev_permutation(BidirectionalIterator first, BidirectionalIterator last) {
    return prev_permutation(first, last,
                            less<iterator_value_t<BidirectionalIterator>>());
}

// ============================================================
// random_shuffle — Fisher-Yates shuffle
// O(n). Uses rand() by default.
// ============================================================
namespace detail {
inline unsigned shuffle_rand() {
    return static_cast<unsigned>(std::rand());
}
} // namespace detail

template<typename RandomIterator, typename RandomNumberGenerator>
void random_shuffle(RandomIterator first, RandomIterator last, RandomNumberGenerator&& gen) {
    using difference_type = iterator_difference_t<RandomIterator>;
    difference_type n = last - first;
    if (n <= 1) return;
    for (difference_type i = n - 1; i > 0; --i) {
        difference_type j = static_cast<difference_type>(gen() % static_cast<unsigned>(i + 1));
        zstl::swap(*(first + i), *(first + j));
    }
}

template<typename RandomIterator>
void random_shuffle(RandomIterator first, RandomIterator last) {
    random_shuffle(first, last, detail::shuffle_rand);
}

// ============================================================
// sample — select n random elements from [first, last), O(n)
// Reservoir sampling (Algorithm R).
// ============================================================
template<typename InputIterator, typename RandomIterator, typename Distance,
         typename RandomNumberGenerator>
RandomIterator sample(InputIterator first, InputIterator last,
                      RandomIterator out, Distance k, RandomNumberGenerator&& gen) {
    using diff = iterator_difference_t<InputIterator>;
    Distance count = 0;
    // First k elements always selected
    for (; first != last && count < k; ++first, ++out, ++count) {
        *out = *first;
    }
    if (count < k) return out;

    // For each remaining element, select with probability k/(n+1)
    diff n = count;
    for (; first != last; ++first, ++n) {
        diff j = static_cast<diff>(gen() % static_cast<unsigned>(n + 1));
        if (j < static_cast<diff>(k)) {
            *(out - k + j) = *first;
        }
    }
    return out;
}

template<typename InputIterator, typename RandomIterator, typename Distance>
RandomIterator sample(InputIterator first, InputIterator last,
                      RandomIterator out, Distance k) {
    return sample(first, last, out, k, detail::shuffle_rand);
}

// ============================================================
// iota — fill range with sequentially increasing values
// O(n)
// ============================================================
template<typename ForwardIterator, typename T>
constexpr void iota(ForwardIterator first, ForwardIterator last, T value) {
    for (; first != last; ++first, ++value) {
        *first = value;
    }
}

// ============================================================
// lexicographical_compare — dictionary-order comparison
// O(min(n1, n2))
// ============================================================
template<typename InputIterator1, typename InputIterator2, typename Compare>
constexpr bool lexicographical_compare(InputIterator1 first1, InputIterator1 last1,
                                        InputIterator2 first2, InputIterator2 last2,
                                        Compare comp) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (comp(*first1, *first2)) return true;
        if (comp(*first2, *first1)) return false;
    }
    return first1 == last1 && first2 != last2;
}

template<typename InputIterator1, typename InputIterator2>
constexpr bool lexicographical_compare(InputIterator1 first1, InputIterator1 last1,
                                        InputIterator2 first2, InputIterator2 last2) {
    return lexicographical_compare(first1, last1, first2, last2, less<void>());
}

} // namespace zstl
