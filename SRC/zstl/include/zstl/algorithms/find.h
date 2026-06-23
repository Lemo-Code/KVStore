// zstl find/search algorithms — find, search, count, mismatch, equal, etc.
#pragma once

#include "zstl/iterators/iterator_traits.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/type_traits.h"

#include <cstring>

namespace zstl {

// ============================================================
// find — linear scan for value
// O(n) comparisons
// ============================================================
template<typename InputIterator, typename T>
constexpr InputIterator find(InputIterator first, InputIterator last, const T& value) {
    for (; first != last; ++first) {
        if (*first == value) break;
    }
    return first;
}

// ============================================================
// find_if — linear scan for first element satisfying predicate
// O(n) predicate calls
// ============================================================
template<typename InputIterator, typename Predicate>
constexpr InputIterator find_if(InputIterator first, InputIterator last, Predicate pred) {
    for (; first != last; ++first) {
        if (pred(*first)) break;
    }
    return first;
}

// ============================================================
// find_if_not — linear scan for first element not satisfying predicate
// O(n) predicate calls
// ============================================================
template<typename InputIterator, typename Predicate>
constexpr InputIterator find_if_not(InputIterator first, InputIterator last, Predicate pred) {
    for (; first != last; ++first) {
        if (!pred(*first)) break;
    }
    return first;
}

// ============================================================
// find_end — last occurrence of subsequence [s_first, s_last) in [first, last)
// O(n * m) naive. Searches from the end backwards.
// ============================================================
template<typename ForwardIterator1, typename ForwardIterator2, typename BinaryPredicate>
ForwardIterator1 find_end(ForwardIterator1 first, ForwardIterator1 last,
                           ForwardIterator2 s_first, ForwardIterator2 s_last,
                           BinaryPredicate pred) {
    if (s_first == s_last) return last;
    ForwardIterator1 result = last;
    while (true) {
        ForwardIterator1 it = first;
        ForwardIterator2 sit = s_first;
        // Scan forward to find a match
        while (it != last && sit != s_last) {
            if (pred(*it, *sit)) {
                ++it; ++sit;
            } else {
                ++it;
                sit = s_first;
            }
        }
        if (sit == s_last) {
            result = first; // Found a match starting at 'first'
            ++first;
        } else {
            break;
        }
        if (first == last) break;
    }
    return result;
}

template<typename ForwardIterator1, typename ForwardIterator2>
ForwardIterator1 find_end(ForwardIterator1 first, ForwardIterator1 last,
                           ForwardIterator2 s_first, ForwardIterator2 s_last) {
    return find_end(first, last, s_first, s_last, equal_to<void>());
}

// ============================================================
// find_first_of — first element in [first, last) that matches any in [s_first, s_last)
// O(n * m)
// ============================================================
template<typename InputIterator, typename ForwardIterator, typename BinaryPredicate>
InputIterator find_first_of(InputIterator first, InputIterator last,
                             ForwardIterator s_first, ForwardIterator s_last,
                             BinaryPredicate pred) {
    for (; first != last; ++first) {
        for (ForwardIterator s = s_first; s != s_last; ++s) {
            if (pred(*first, *s)) return first;
        }
    }
    return last;
}

template<typename InputIterator, typename ForwardIterator>
InputIterator find_first_of(InputIterator first, InputIterator last,
                             ForwardIterator s_first, ForwardIterator s_last) {
    return find_first_of(first, last, s_first, s_last, equal_to<void>());
}

// ============================================================
// adjacent_find — first pair of adjacent equal elements (by predicate)
// O(n)
// ============================================================
template<typename ForwardIterator, typename BinaryPredicate>
constexpr ForwardIterator adjacent_find(ForwardIterator first, ForwardIterator last,
                                         BinaryPredicate pred) {
    if (first == last) return last;
    ForwardIterator next = first;
    ++next;
    for (; next != last; ++first, ++next) {
        if (pred(*first, *next)) return first;
    }
    return last;
}

template<typename ForwardIterator>
constexpr ForwardIterator adjacent_find(ForwardIterator first, ForwardIterator last) {
    return adjacent_find(first, last, equal_to<void>());
}

// ============================================================
// search — Boyer-Moore-Horspool for byte types, naive for others
// Finds first occurrence of [s_first, s_last) in [first, last)
// BMH: O(n/m) average, O(n*m) worst; Naive: O(n*m)
// ============================================================
namespace detail {

// Boyer-Moore-Horspool: efficient for single-byte types and small alphabets
template<typename ForwardIterator1, typename ForwardIterator2, typename BinaryPredicate>
ForwardIterator1 search_bmh(ForwardIterator1 first, ForwardIterator1 last,
                             ForwardIterator2 s_first, ForwardIterator2 s_last,
                             BinaryPredicate pred) {
    using diff1 = iterator_difference_t<ForwardIterator1>;
    using diff2 = iterator_difference_t<ForwardIterator2>;
    diff2 pat_len = zstl::distance(s_first, s_last);
    if (pat_len == 0) return first;

    // Build bad-character skip table
    constexpr size_t kTableSize = 256;
    diff2 bad_char[kTableSize];
    for (size_t i = 0; i < kTableSize; ++i) bad_char[i] = pat_len;

    {
        ForwardIterator2 p = s_first;
        diff2 offset = 0;
        for (; p != s_last; ++p, ++offset) {
            unsigned char c = static_cast<unsigned char>(*p);
            bad_char[c] = pat_len - 1 - offset;
        }
    }

    ForwardIterator1 cur = first;
    zstl::advance(cur, static_cast<diff1>(pat_len - 1));
    while (true) {
        if (zstl::distance(cur, last) < 0 || zstl::distance(first, last) < pat_len) break;

        ForwardIterator1 hay = cur;
        ForwardIterator2 needle = s_last;
        diff2 matched = 0;
        while (matched < pat_len && pred(*(--hay), *(--needle))) {
            ++matched;
        }
        if (matched == static_cast<size_t>(pat_len)) {
            ++hay;
            return hay;
        }

        // Bad-character shift
        unsigned char bc = static_cast<unsigned char>(*cur);
        diff2 shift = bad_char[bc];
        if (shift == 0) shift = 1;
        zstl::advance(cur, static_cast<diff1>(shift));
    }
    return last;
}

// Naive search for non-byte types
template<typename ForwardIterator1, typename ForwardIterator2, typename BinaryPredicate>
ForwardIterator1 search_naive(ForwardIterator1 first, ForwardIterator1 last,
                               ForwardIterator2 s_first, ForwardIterator2 s_last,
                               BinaryPredicate pred) {
    if (s_first == s_last) return first;

    using diff1 = iterator_difference_t<ForwardIterator1>;
    diff1 n = zstl::distance(first, last);
    diff1 m = zstl::distance(s_first, s_last);

    if (n < m) return last;

    ForwardIterator1 last_possible = first;
    zstl::advance(last_possible, n - m);

    for (; ; ++first) {
        ForwardIterator1 it = first;
        ForwardIterator2 sit = s_first;
        while (sit != s_last && pred(*it, *sit)) {
            ++it; ++sit;
        }
        if (sit == s_last) return first;
        if (first == last_possible) break;
    }
    return last;
}

} // namespace detail

template<typename ForwardIterator1, typename ForwardIterator2, typename BinaryPredicate>
ForwardIterator1 search(ForwardIterator1 first, ForwardIterator1 last,
                         ForwardIterator2 s_first, ForwardIterator2 s_last,
                         BinaryPredicate pred) {
    if (s_first == s_last) return first;

    // Use BMH for byte-sized value types
    using T1 = iterator_value_t<ForwardIterator1>;
    using T2 = iterator_value_t<ForwardIterator2>;
    if constexpr (sizeof(T1) == 1 && sizeof(T2) == 1 &&
                  std::is_integral_v<T1> && std::is_integral_v<T2>) {
        return detail::search_bmh(first, last, s_first, s_last, pred);
    } else {
        return detail::search_naive(first, last, s_first, s_last, pred);
    }
}

template<typename ForwardIterator1, typename ForwardIterator2>
ForwardIterator1 search(ForwardIterator1 first, ForwardIterator1 last,
                         ForwardIterator2 s_first, ForwardIterator2 s_last) {
    return search(first, last, s_first, s_last, equal_to<void>());
}

// ============================================================
// search_n — find first occurrence of n consecutive copies of value
// O(n)
// ============================================================
template<typename ForwardIterator, typename Size, typename T, typename BinaryPredicate>
ForwardIterator search_n(ForwardIterator first, ForwardIterator last,
                          Size count, const T& value, BinaryPredicate pred) {
    if (count <= 0) return first;

    ForwardIterator limit = first;
    {
        using diff = iterator_difference_t<ForwardIterator>;
        diff d = zstl::distance(first, last);
        if (d < static_cast<diff>(count)) return last;
        zstl::advance(limit, d - static_cast<diff>(count) + 1);
    }

    for (; first != limit; ++first) {
        if (!pred(*first, value)) continue;
        ForwardIterator it = first;
        Size n = 0;
        while (n < count && it != last && pred(*it, value)) {
            ++n; ++it;
        }
        if (n == count) return first;
        if (it == last) break;
        first = it;
        --first; // ++first in loop advances
    }
    return last;
}

template<typename ForwardIterator, typename Size, typename T>
ForwardIterator search_n(ForwardIterator first, ForwardIterator last,
                          Size count, const T& value) {
    return search_n(first, last, count, value, equal_to<void>());
}

// ============================================================
// count — count elements equal to value
// O(n)
// ============================================================
template<typename InputIterator, typename T>
constexpr iterator_difference_t<InputIterator>
count(InputIterator first, InputIterator last, const T& value) {
    iterator_difference_t<InputIterator> n = 0;
    for (; first != last; ++first) {
        if (*first == value) ++n;
    }
    return n;
}

// ============================================================
// count_if — count elements satisfying predicate
// O(n)
// ============================================================
template<typename InputIterator, typename Predicate>
constexpr iterator_difference_t<InputIterator>
count_if(InputIterator first, InputIterator last, Predicate pred) {
    iterator_difference_t<InputIterator> n = 0;
    for (; first != last; ++first) {
        if (pred(*first)) ++n;
    }
    return n;
}

// ============================================================
// mismatch — find first position where two ranges differ
// Returns pair of iterators pointing to first mismatched elements.
// O(min(n1, n2))
// ============================================================
template<typename InputIterator1, typename InputIterator2, typename BinaryPredicate>
constexpr pair<InputIterator1, InputIterator2>
mismatch(InputIterator1 first1, InputIterator1 last1,
         InputIterator2 first2, BinaryPredicate pred) {
    while (first1 != last1 && pred(*first1, *first2)) {
        ++first1; ++first2;
    }
    return {first1, first2};
}

template<typename InputIterator1, typename InputIterator2>
constexpr pair<InputIterator1, InputIterator2>
mismatch(InputIterator1 first1, InputIterator1 last1, InputIterator2 first2) {
    return mismatch(first1, last1, first2, equal_to<void>());
}

// ============================================================
// mismatch (four-iterator version) — with separate last1/last2
// ============================================================
template<typename InputIterator1, typename InputIterator2, typename BinaryPredicate>
constexpr pair<InputIterator1, InputIterator2>
mismatch(InputIterator1 first1, InputIterator1 last1,
         InputIterator2 first2, InputIterator2 last2, BinaryPredicate pred) {
    while (first1 != last1 && first2 != last2 && pred(*first1, *first2)) {
        ++first1; ++first2;
    }
    return {first1, first2};
}

template<typename InputIterator1, typename InputIterator2>
constexpr pair<InputIterator1, InputIterator2>
mismatch(InputIterator1 first1, InputIterator1 last1,
         InputIterator2 first2, InputIterator2 last2) {
    return mismatch(first1, last1, first2, last2, equal_to<void>());
}

// ============================================================
// equal — test if two ranges are equal
// O(min(n1, n2))
// ============================================================
template<typename InputIterator1, typename InputIterator2, typename BinaryPredicate>
constexpr bool equal(InputIterator1 first1, InputIterator1 last1,
                      InputIterator2 first2, BinaryPredicate pred) {
    // Use POD fast path for trivially comparable types
    using T1 = iterator_value_t<InputIterator1>;
    using T2 = iterator_value_t<InputIterator2>;
    if constexpr (std::is_same_v<T1, T2> &&
                  is_pod_v<T1> &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<InputIterator1>> &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<InputIterator2>>) {
        auto n = static_cast<size_t>(zstl::distance(first1, last1));
        return std::memcmp(&*first1, &*first2, n * sizeof(T1)) == 0;
    }
    for (; first1 != last1; ++first1, ++first2) {
        if (!pred(*first1, *first2)) return false;
    }
    return true;
}

template<typename InputIterator1, typename InputIterator2>
constexpr bool equal(InputIterator1 first1, InputIterator1 last1, InputIterator2 first2) {
    return equal(first1, last1, first2, equal_to<void>());
}

// ============================================================
// equal (four-iterator version) — with separate lengths
// ============================================================
template<typename InputIterator1, typename InputIterator2, typename BinaryPredicate>
constexpr bool equal(InputIterator1 first1, InputIterator1 last1,
                      InputIterator2 first2, InputIterator2 last2, BinaryPredicate pred) {
    using T1 = iterator_value_t<InputIterator1>;
    using T2 = iterator_value_t<InputIterator2>;
    if constexpr (std::is_same_v<T1, T2> &&
                  is_pod_v<T1> &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<InputIterator1>> &&
                  std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<InputIterator2>>) {
        auto n1 = static_cast<size_t>(zstl::distance(first1, last1));
        auto n2 = static_cast<size_t>(zstl::distance(first2, last2));
        if (n1 != n2) return false;
        return std::memcmp(&*first1, &*first2, n1 * sizeof(T1)) == 0;
    }
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (!pred(*first1, *first2)) return false;
    }
    return first1 == last1 && first2 == last2;
}

template<typename InputIterator1, typename InputIterator2>
constexpr bool equal(InputIterator1 first1, InputIterator1 last1,
                      InputIterator2 first2, InputIterator2 last2) {
    return equal(first1, last1, first2, last2, equal_to<void>());
}

// ============================================================
// all_of — test if all elements satisfy predicate
// O(n), short-circuits on first false
// ============================================================
template<typename InputIterator, typename Predicate>
constexpr bool all_of(InputIterator first, InputIterator last, Predicate pred) {
    return find_if_not(first, last, pred) == last;
}

// ============================================================
// any_of — test if any element satisfies predicate
// O(n), short-circuits on first true
// ============================================================
template<typename InputIterator, typename Predicate>
constexpr bool any_of(InputIterator first, InputIterator last, Predicate pred) {
    return find_if(first, last, pred) != last;
}

// ============================================================
// none_of — test if no elements satisfy predicate
// O(n), short-circuits on first true
// ============================================================
template<typename InputIterator, typename Predicate>
constexpr bool none_of(InputIterator first, InputIterator last, Predicate pred) {
    return find_if(first, last, pred) == last;
}

} // namespace zstl
