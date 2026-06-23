/**
 * @file    uninitialized.h
 * @brief   Uninitialized memory algorithms.
 * @author  lstl team
 * @date    2025
 *
 * Provides safe construction of objects into uninitialized memory:
 * - uninitialized_copy: copy-constructs a range into raw memory.
 * - uninitialized_fill: fill-constructs a range with copies of a value.
 * - uninitialized_fill_n: fill-constructs n elements.
 * - uninitialized_move: move-constructs a range into raw memory.
 *
 * Each algorithm has two internal code paths:
 * - POD path:    uses memmove/memcpy for trivially-copyable types.
 * - Non-POD path: element-by-element construction with exception safety
 *   (rolls back on failure).
 *
 * @ingroup memory
 */

#pragma once

#include <cstring>
#include <iterator>

#include "type_traits.h"
#include "construct.h"
#include "utility.h"

namespace lstl {

namespace detail {

// =========================================================================
// POD-optimized helpers (selected via tag dispatch)
// =========================================================================

/**
 * @brief  POD path for uninitialized_copy — uses memmove.
 *
 * Trivially-copyable types can be bulk-copied without per-element
 * construction, providing significant performance improvements for
 * large arrays of scalars or POD structs.
 *
 * @tparam InputIterator     Source iterator type.
 * @tparam ForwardIterator   Destination iterator type.
 * @param  first, last       Source range.
 * @param  result            Destination (uninitialized memory).
 * @param  true_type         Tag indicating the POD path.
 * @return                   Iterator past the last copied element in the destination.
 */
template <typename InputIterator, typename ForwardIterator>
ForwardIterator
uninitialized_copy_aux(InputIterator first, InputIterator last,
                       ForwardIterator result, true_type) {
    typedef typename std::iterator_traits<InputIterator>::value_type value_type;
    size_t n = static_cast<size_t>(std::distance(first, last));
    if (n > 0) {
        std::memmove(&*result, &*first, n * sizeof(value_type));
    }
    return result + n;
}

/**
 * @brief  Non-POD path for uninitialized_copy — element-wise construction.
 *
 * Constructs each element individually. If any construction throws,
 * all previously-constructed elements are destroyed before rethrowing,
 * providing the strong exception safety guarantee.
 */
template <typename InputIterator, typename ForwardIterator>
ForwardIterator
uninitialized_copy_aux(InputIterator first, InputIterator last,
                       ForwardIterator result, false_type) {
    ForwardIterator cur = result;
    try {
        for (; first != last; ++first, ++cur) {
            construct(&*cur, *first);
        }
        return cur;
    } catch (...) {
        lstl::destroy(result, cur);
        throw;
    }
}

/**
 * @brief  POD path for uninitialized_fill — bulk memcpy.
 *
 * Constructs the first element normally, then memcpy's the pattern
 * to the remaining elements. Only valid for trivially-copyable types.
 */
template <typename ForwardIterator, typename T>
void uninitialized_fill_aux(ForwardIterator first, ForwardIterator last,
                            const T& x, true_type) {
    typedef typename std::iterator_traits<ForwardIterator>::value_type value_type;
    if (first == last) return;
    construct(&*first, x);
    ForwardIterator cur = first;
    ++cur;
    for (; cur != last; ++cur) {
        std::memcpy(&*cur, &*first, sizeof(value_type));
    }
}

/**
 * @brief  Non-POD path for uninitialized_fill — element-wise construction.
 *
 * Provides strong exception safety: if any construction fails,
 * previously-constructed elements are destroyed.
 */
template <typename ForwardIterator, typename T>
void uninitialized_fill_aux(ForwardIterator first, ForwardIterator last,
                            const T& x, false_type) {
    ForwardIterator cur = first;
    try {
        for (; cur != last; ++cur) {
            construct(&*cur, x);
        }
    } catch (...) {
        lstl::destroy(first, cur);
        throw;
    }
}

/**
 * @brief  POD path for uninitialized_fill_n — bulk memcpy (returns end iterator).
 */
template <typename ForwardIterator, typename Size, typename T>
ForwardIterator
uninitialized_fill_n_aux(ForwardIterator first, Size n, const T& x, true_type) {
    if (n <= 0) return first;
    ForwardIterator cur = first;
    construct(&*cur, x);
    ++cur;
    typedef typename std::iterator_traits<ForwardIterator>::value_type value_type;
    for (--n; n > 0; --n, ++cur) {
        std::memcpy(&*cur, &*first, sizeof(value_type));
    }
    return cur;
}

/**
 * @brief  Non-POD path for uninitialized_fill_n — element-wise (returns end iterator).
 */
template <typename ForwardIterator, typename Size, typename T>
ForwardIterator
uninitialized_fill_n_aux(ForwardIterator first, Size n, const T& x, false_type) {
    ForwardIterator cur = first;
    try {
        for (; n > 0; --n, ++cur) {
            construct(&*cur, x);
        }
        return cur;
    } catch (...) {
        lstl::destroy(first, cur);
        throw;
    }
}

} // namespace detail

// =========================================================================
// Public API
// =========================================================================

/**
 * @brief  Copies elements from [first, last) to uninitialized memory at result.
 *
 * Selects between POD bulk-copy and element-wise construction based on
 * whether the value type has a trivial copy constructor.
 *
 * @tparam InputIterator     Source iterator type.
 * @tparam ForwardIterator   Destination iterator type.
 * @param  first, last       Source range.
 * @param  result            Destination (must point to uninitialized memory).
 * @return                   Iterator past the last copied element in the destination.
 *
 * @throws  Re-throws any exception from element copy constructors,
 *          after destroying already-constructed elements.
 *
 * @pre  [result, result + (last - first)) is valid uninitialized memory.
 * @post  Elements in the destination are copy-constructed from the source.
 */
template <typename InputIterator, typename ForwardIterator>
ForwardIterator
uninitialized_copy(InputIterator first, InputIterator last,
                   ForwardIterator result) {
    typedef typename std::iterator_traits<ForwardIterator>::value_type value_type;
    return detail::uninitialized_copy_aux(
        first, last, result,
        typename is_trivially_copyable<value_type>::type());
}

/**
 * @brief  Fills [first, last) with copies of @p x.
 *
 * @tparam ForwardIterator  Destination iterator type.
 * @tparam T                Value type.
 * @param  first, last      Destination range (uninitialized memory).
 * @param  x                The value to fill with.
 *
 * @pre   [first, last) is valid uninitialized memory.
 * @post  All elements in [first, last) are copy-constructed from x.
 */
template <typename ForwardIterator, typename T>
void uninitialized_fill(ForwardIterator first, ForwardIterator last,
                        const T& x) {
    typedef typename std::iterator_traits<ForwardIterator>::value_type value_type;
    detail::uninitialized_fill_aux(
        first, last, x,
        typename is_trivially_copyable<value_type>::type());
}

/**
 * @brief  Fills @p n elements starting at @p first with copies of @p x.
 *
 * @tparam ForwardIterator  Destination iterator type.
 * @tparam Size             Integer type for the count.
 * @tparam T                Value type.
 * @param  first            Destination (uninitialized memory).
 * @param  n                Number of elements to fill.
 * @param  x                The value to fill with.
 * @return                  Iterator past the last filled element.
 *
 * @pre   [first, first + n) is valid uninitialized memory.
 */
template <typename ForwardIterator, typename Size, typename T>
ForwardIterator
uninitialized_fill_n(ForwardIterator first, Size n, const T& x) {
    typedef typename std::iterator_traits<ForwardIterator>::value_type value_type;
    return detail::uninitialized_fill_n_aux(
        first, n, x,
        typename is_trivially_copyable<value_type>::type());
}

/**
 * @brief  Move-constructs elements from [first, last) to uninitialized memory.
 *
 * Uses lstl::move on each source element, leaving them in a
 * valid-but-unspecified state. Falls back to copy for non-movable types.
 *
 * @tparam InputIterator     Source iterator type.
 * @tparam ForwardIterator   Destination iterator type.
 * @param  first, last       Source range.
 * @param  result            Destination (uninitialized memory).
 * @return                   Iterator past the last moved element in the destination.
 *
 * @throws  Re-throws any exception from element move constructors,
 *          after destroying already-constructed elements.
 */
template <typename InputIterator, typename ForwardIterator>
ForwardIterator
uninitialized_move(InputIterator first, InputIterator last,
                   ForwardIterator result) {
    ForwardIterator cur = result;
    try {
        for (; first != last; ++first, ++cur) {
            construct(&*cur, lstl::move(*first));
        }
        return cur;
    } catch (...) {
        lstl::destroy(result, cur);
        throw;
    }
}

} // namespace lstl
