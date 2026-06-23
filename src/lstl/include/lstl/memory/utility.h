/**
 * @file    utility.h
 * @brief   Core utility definitions: pair, swap, move, forward, integer_sequence.
 * @author  lstl team
 * @date    2025
 *
 * Provides foundational utilities used throughout lstl:
 * - lstl::pair:   A C++14-compatible pair type with full comparison operators.
 * - lstl::move:   Casts to rvalue reference enabling move semantics.
 * - lstl::forward: Perfect forwarding for template arguments.
 * - lstl::swap:   ADL-safe element exchange.
 * - integer_sequence: C++14-compatible index sequence for variadic expansion.
 *
 * All components are designed to interoperate with their std counterparts
 * while avoiding dependency on specific STL implementation details.
 *
 * @ingroup memory
 */

#pragma once

#include <utility>
#include <cstddef>

namespace lstl {

// =========================================================================
// Move semantics
// =========================================================================

/**
 * @brief  Converts an lvalue or rvalue to an rvalue reference.
 *
 * Enables move semantics by casting the argument to an rvalue reference
 * type. Use lstl::move (not std::move) within lstl code to avoid ADL
 * ambiguities when both namespaces are visible.
 *
 * @tparam T  The deduced type of the argument.
 * @param  t  The object to cast.
 * @return    An rvalue reference to @p t.
 *
 * @note  After moving from an object, it is in a valid-but-unspecified state.
 */
template <typename T>
constexpr typename std::remove_reference<T>::type&& move(T&& t) noexcept {
    return static_cast<typename std::remove_reference<T>::type&&>(t);
}

/**
 * @brief  Perfect forwarding — preserves lvalue/rvalue-ness of the argument.
 *
 * Used in template functions that need to forward their arguments to
 * another function without changing their value category.
 *
 * @tparam T  The deduced type (must be explicitly specified).
 * @param  t  The argument to forward.
 * @return    @p t with its original value category preserved.
 *
 * @warning  T must be explicitly specified; the compiler cannot deduce it.
 */
template <typename T>
constexpr T&& forward(typename std::remove_reference<T>::type& t) noexcept {
    return static_cast<T&&>(t);
}

template <typename T>
constexpr T&& forward(typename std::remove_reference<T>::type&& t) noexcept {
    static_assert(!std::is_lvalue_reference<T>::value,
                  "Cannot forward an rvalue as an lvalue.");
    return static_cast<T&&>(t);
}

// =========================================================================
// swap
// =========================================================================

/**
 * @brief  Exchanges the values of two objects.
 *
 * Uses move semantics for efficiency. Provides the strong exception
 * guarantee when T's move operations are noexcept.
 *
 * @tparam T  The type of objects to swap (must be MoveConstructible and MoveAssignable).
 * @param  a  First object.
 * @param  b  Second object.
 *
 * @post  a contains the old value of b, and b contains the old value of a.
 */
template <typename T>
void swap(T& a, T& b) noexcept(std::is_nothrow_move_constructible<T>::value &&
                                std::is_nothrow_move_assignable<T>::value) {
    T tmp = lstl::move(a);
    a = lstl::move(b);
    b = lstl::move(tmp);
}

// =========================================================================
// pair
// =========================================================================

/**
 * @brief  A heterogeneous pair of values.
 *
 * lstl::pair is API-compatible with std::pair but provides explicit
 * copy/move constructors to avoid issues with const-qualified first
 * members (as used in map::value_type).
 *
 * @tparam T1  Type of the first element.
 * @tparam T2  Type of the second element.
 *
 * @note  The copy constructor is explicitly declared to prevent implicit
 *        deletion when T1 or T2 is const-qualified and a move constructor
 *        is also declared.
 */
template <typename T1, typename T2>
struct pair {
    typedef T1 first_type;   ///< Type of the first element.
    typedef T2 second_type;  ///< Type of the second element.

    T1 first;   ///< The first element.
    T2 second;  ///< The second element.

    /**
     * @brief  Default constructor — value-initializes both elements.
     *
     * Each element is initialized via T(), which for fundamental types
     * means zero-initialization.
     */
    pair() : first(), second() {}

    /**
     * @brief  Constructs a pair from two values.
     * @param  a  Value for the first element.
     * @param  b  Value for the second element.
     */
    pair(const T1& a, const T2& b) : first(a), second(b) {}

    /**
     * @brief  Copy constructor — copies both elements from another pair.
     * @param  p  The source pair.
     *
     * Explicitly defined to prevent implicit deletion when T1 is const.
     */
    pair(const pair& p) : first(p.first), second(p.second) {}

    /**
     * @brief  Converting constructor — constructs from a pair with different types.
     *
     * Enabled only when U1 is implicitly convertible to T1 and U2 to T2.
     *
     * @tparam U1  Source first element type.
     * @tparam U2  Source second element type.
     * @param  p   The source pair to convert from.
     */
    template <typename U1, typename U2>
    pair(const pair<U1, U2>& p) : first(p.first), second(p.second) {}

    /**
     * @brief  Move constructor — moves both elements from another pair.
     * @param  p  The source pair (will be left in a valid-but-unspecified state).
     */
    pair(pair&& p) noexcept
        : first(lstl::move(p.first)), second(lstl::move(p.second)) {}

    /**
     * @brief  Move assignment operator.
     * @param  p  The source pair.
     * @return    Reference to this pair.
     */
    pair& operator=(pair&& p) noexcept {
        first = lstl::move(p.first);
        second = lstl::move(p.second);
        return *this;
    }

    /**
     * @brief  Copy assignment operator.
     * @param  p  The source pair.
     * @return    Reference to this pair.
     */
    pair& operator=(const pair& p) {
        first = p.first;
        second = p.second;
        return *this;
    }

    /**
     * @brief  Swaps the contents of this pair with another.
     * @param  p  The pair to swap with.
     *
     * @post  This pair contains p's old values, and p contains this pair's old values.
     */
    void swap(pair& p) noexcept {
        lstl::swap(first, p.first);
        lstl::swap(second, p.second);
    }
};

// ---------------------------------------------------------------------------
// pair comparison operators
// ---------------------------------------------------------------------------

/// @name Pair Equality
/// @{

/**
 * @brief  Equality comparison for pairs.
 * @return true if both first and second elements compare equal.
 */
template <typename T1, typename T2>
inline bool operator==(const pair<T1, T2>& x, const pair<T1, T2>& y) {
    return x.first == y.first && x.second == y.second;
}

/// @brief  Inequality comparison for pairs.
template <typename T1, typename T2>
inline bool operator!=(const pair<T1, T2>& x, const pair<T1, T2>& y) {
    return !(x == y);
}

/// @}

/// @name Pair Ordering
/// @{

/**
 * @brief  Less-than comparison (lexicographic).
 *
 * Compares first elements; if equal, compares second elements.
 */
template <typename T1, typename T2>
inline bool operator<(const pair<T1, T2>& x, const pair<T1, T2>& y) {
    return x.first < y.first || (!(y.first < x.first) && x.second < y.second);
}

/// @brief  Greater-than comparison for pairs.
template <typename T1, typename T2>
inline bool operator>(const pair<T1, T2>& x, const pair<T1, T2>& y) {
    return y < x;
}

/// @brief  Less-than-or-equal comparison for pairs.
template <typename T1, typename T2>
inline bool operator<=(const pair<T1, T2>& x, const pair<T1, T2>& y) {
    return !(y < x);
}

/// @brief  Greater-than-or-equal comparison for pairs.
template <typename T1, typename T2>
inline bool operator>=(const pair<T1, T2>& x, const pair<T1, T2>& y) {
    return !(x < y);
}

/// @}

/**
 * @brief  Creates a pair, deducing the types from the arguments.
 *
 * @tparam T1  Type of the first argument (deduced).
 * @tparam T2  Type of the second argument (deduced).
 * @param  x   First value.
 * @param  y   Second value.
 * @return     A pair<T1, T2> containing copies of x and y.
 *
 * @code
 * auto p = lstl::make_pair(42, std::string("hello"));
 * // p is pair<int, std::string>
 * @endcode
 */
template <typename T1, typename T2>
inline pair<T1, T2> make_pair(T1 x, T2 y) {
    return pair<T1, T2>(x, y);
}

// =========================================================================
// min / max
// =========================================================================

/**
 * @brief  Returns the greater of two values.
 * @param  a  First value.
 * @param  b  Second value.
 * @return    a if b < a, otherwise b.
 */
template <typename T>
constexpr const T& max(const T& a, const T& b) {
    return (a < b) ? b : a;
}

/**
 * @brief  Returns the lesser of two values.
 * @param  a  First value.
 * @param  b  Second value.
 * @return    a if a < b, otherwise b.
 */
template <typename T>
constexpr const T& min(const T& a, const T& b) {
    return (b < a) ? b : a;
}

// =========================================================================
// integer_sequence (C++14 backport)
// =========================================================================

/**
 * @brief  Represents a compile-time sequence of integer values.
 *
 * @tparam T     The integer type of the sequence elements.
 * @tparam Ints  The sequence values.
 */
template <typename T, T... Ints>
struct integer_sequence {
    typedef T value_type;  ///< The integer type of elements.

    /**
     * @brief  Returns the number of elements in the sequence.
     * @return sizeof...(Ints)
     */
    static constexpr size_t size() noexcept { return sizeof...(Ints); }
};

/// @brief  An integer_sequence of size_t values.
template <size_t... Ints>
using index_sequence = integer_sequence<size_t, Ints...>;

namespace detail {

/** @brief  Recursive implementation of make_integer_sequence. */
template <typename T, size_t N, T... Ints>
struct make_integer_sequence_impl
    : make_integer_sequence_impl<T, N - 1, N - 1, Ints...> {};

/** @brief  Base case: N == 0. */
template <typename T, T... Ints>
struct make_integer_sequence_impl<T, 0, Ints...> {
    using type = integer_sequence<T, Ints...>;
};

} // namespace detail

/**
 * @brief  Creates an integer_sequence<T, 0, 1, ..., N-1>.
 *
 * @tparam T  The integer type.
 * @tparam N  The number of elements.
 *
 * @code
 * using seq = make_integer_sequence<int, 3>;
 * // seq = integer_sequence<int, 0, 1, 2>
 * @endcode
 */
template <typename T, size_t N>
using make_integer_sequence = typename detail::make_integer_sequence_impl<T, N>::type;

/// @brief  Creates an index_sequence<0, 1, ..., N-1>.
template <size_t N>
using make_index_sequence = make_integer_sequence<size_t, N>;

/// @brief  Creates an index_sequence with size equal to the number of types in the pack.
template <typename... T>
using index_sequence_for = make_index_sequence<sizeof...(T)>;

} // namespace lstl
