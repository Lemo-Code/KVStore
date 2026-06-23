// zstl utility — move, forward, swap, pair, integer_sequence, min/max/clamp
// Complete utility header with all essential components for containers and
// algorithms. Implemented from scratch; no std dependency for core utilities.
#pragma once

#include <cstddef>        // for size_t
#include <initializer_list>
#include <tuple>
#include "zstl/memory/type_traits.h"

namespace zstl {

// ============================================================
// Part 1: move, forward, move_if_noexcept
// ============================================================

// move — cast to rvalue reference (enables move semantics)
template<typename T>
constexpr remove_reference_t<T>&& move(T&& t) noexcept {
    return static_cast<remove_reference_t<T>&&>(t);
}

// forward — perfect forwarding with reference collapsing
// Overload for lvalues
template<typename T>
constexpr T&& forward(remove_reference_t<T>& t) noexcept {
    return static_cast<T&&>(t);
}

// Overload for rvalues (prevents binding rvalues to lvalue references)
template<typename T>
constexpr T&& forward(remove_reference_t<T>&& t) noexcept {
    static_assert(!is_lvalue_reference_v<T>,
        "zstl::forward: cannot forward an rvalue as an lvalue");
    return static_cast<T&&>(t);
}

// move_if_noexcept — conditional move: moves if noexcept, copies otherwise
template<typename T>
constexpr conditional_t<
    !is_nothrow_move_constructible_v<T> && is_copy_constructible_v<T>,
    const T&,
    T&&
> move_if_noexcept(T& x) noexcept {
    return zstl::move(x);
}

// ============================================================
// Part 2: swap
// ============================================================

// swap — generic two-value swap (uses move construction/assignment)
template<typename T>
constexpr void swap(T& a, T& b)
    noexcept(is_nothrow_move_constructible_v<T> && is_nothrow_move_assignable_v<T>)
{
    T tmp(zstl::move(a));
    a = zstl::move(b);
    b = zstl::move(tmp);
}

// Array swap (C++11 overload)
template<typename T, size_t N>
void swap(T (&a)[N], T (&b)[N]) noexcept(noexcept(swap(*a, *b))) {
    for (size_t i = 0; i < N; ++i) {
        swap(a[i], b[i]);
    }
}

// ============================================================
// Part 3: exchange
// ============================================================

// exchange — replace value and return old value
template<typename T, typename U = T>
constexpr T exchange(T& obj, U&& new_val)
    noexcept(is_nothrow_move_constructible_v<T> &&
             is_nothrow_assignable_v<T&, U>)
{
    T old_val = zstl::move(obj);
    obj = zstl::forward<U>(new_val);
    return old_val;
}

// ============================================================
// Part 4: as_const
// ============================================================

template<typename T>
constexpr add_const_t<T>& as_const(T& t) noexcept {
    return t;
}

// Prevent calling as_const on rvalues
template<typename T>
void as_const(const T&&) = delete;

// ============================================================
// addressof — obtain true address of an object (bypasses
// overloaded operator& if present)
// ============================================================

template<typename T>
T* addressof(T& arg) noexcept {
    // Reinterpret through char to bypass possible overloaded operator&
    return reinterpret_cast<T*>(
        &const_cast<char&>(
            reinterpret_cast<const volatile char&>(arg)));
}

// ============================================================
// Part 5: pair
// ============================================================

// piecewise_construct_t — tag for piecewise construction
struct piecewise_construct_t {
    explicit piecewise_construct_t() = default;
};
inline constexpr piecewise_construct_t piecewise_construct{};

// in_place_t — tag for in-place construction
struct in_place_t {
    explicit in_place_t() = default;
};
inline constexpr in_place_t in_place{};

// in_place_type_t — tag for in-place construction with explicit type
template<typename T>
struct in_place_type_t {
    explicit in_place_type_t() = default;
};
template<typename T>
inline constexpr in_place_type_t<T> in_place_type{};

// in_place_index_t — tag for in-place construction with index
template<size_t I>
struct in_place_index_t {
    explicit in_place_index_t() = default;
};
template<size_t I>
inline constexpr in_place_index_t<I> in_place_index{};

// pair — two-element heterogeneous container
template<typename T1, typename T2>
struct pair {
    using first_type  = T1;
    using second_type = T2;

    T1 first;
    T2 second;

    // Default constructor
    constexpr pair()
        noexcept(is_nothrow_default_constructible_v<T1> &&
                 is_nothrow_default_constructible_v<T2>)
        : first(), second() {}

    // Constructor from lvalue references
    constexpr pair(const T1& x, const T2& y)
        noexcept(is_nothrow_copy_constructible_v<T1> &&
                 is_nothrow_copy_constructible_v<T2>)
        : first(x), second(y) {}

    // Constructor with perfect forwarding
    template<typename U1 = T1, typename U2 = T2,
             enable_if_t<is_constructible_v<T1, U1&&> &&
                         is_constructible_v<T2, U2&&>, int> = 0>
    constexpr pair(U1&& x, U2&& y)
        noexcept(is_nothrow_constructible_v<T1, U1&&> &&
                 is_nothrow_constructible_v<T2, U2&&>)
        : first(zstl::forward<U1>(x)), second(zstl::forward<U2>(y)) {}

    // Converting copy constructor from another pair type
    template<typename U1, typename U2,
             enable_if_t<is_constructible_v<T1, const U1&> &&
                         is_constructible_v<T2, const U2&>, int> = 0>
    constexpr pair(const pair<U1, U2>& p)
        noexcept(is_nothrow_constructible_v<T1, const U1&> &&
                 is_nothrow_constructible_v<T2, const U2&>)
        : first(p.first), second(p.second) {}

    // Converting move constructor from another pair type
    template<typename U1, typename U2,
             enable_if_t<is_constructible_v<T1, U1&&> &&
                         is_constructible_v<T2, U2&&>, int> = 0>
    constexpr pair(pair<U1, U2>&& p)
        noexcept(is_nothrow_constructible_v<T1, U1&&> &&
                 is_nothrow_constructible_v<T2, U2&&>)
        : first(zstl::move(p.first)), second(zstl::move(p.second)) {}

    // Copy and move constructors (default)
    pair(const pair&) = default;
    pair(pair&&) = default;

    // Copy assignment
    constexpr pair& operator=(const pair& p)
        noexcept(is_nothrow_copy_assignable_v<T1> &&
                 is_nothrow_copy_assignable_v<T2>)
    {
        first  = p.first;
        second = p.second;
        return *this;
    }

    // Move assignment
    constexpr pair& operator=(pair&& p)
        noexcept(is_nothrow_move_assignable_v<T1> &&
                 is_nothrow_move_assignable_v<T2>)
    {
        first  = zstl::move(p.first);
        second = zstl::move(p.second);
        return *this;
    }

    // Converting copy assignment
    template<typename U1, typename U2>
    constexpr pair& operator=(const pair<U1, U2>& p)
        noexcept(is_nothrow_assignable_v<T1&, const U1&> &&
                 is_nothrow_assignable_v<T2&, const U2&>)
    {
        first  = p.first;
        second = p.second;
        return *this;
    }

    // Converting move assignment
    template<typename U1, typename U2>
    constexpr pair& operator=(pair<U1, U2>&& p)
        noexcept(is_nothrow_assignable_v<T1&, U1&&> &&
                 is_nothrow_assignable_v<T2&, U2&&>)
    {
        first  = zstl::move(p.first);
        second = zstl::move(p.second);
        return *this;
    }

    // swap
    constexpr void swap(pair& p)
        noexcept(is_nothrow_swappable_v<T1> && is_nothrow_swappable_v<T2>)
    {
        zstl::swap(first, p.first);
        zstl::swap(second, p.second);
    }
};

// ============================================================
// pair — comparison operators
// ============================================================

template<typename T1, typename T2>
constexpr bool operator==(const pair<T1, T2>& a, const pair<T1, T2>& b) {
    return a.first == b.first && a.second == b.second;
}

template<typename T1, typename T2>
constexpr bool operator!=(const pair<T1, T2>& a, const pair<T1, T2>& b) {
    return !(a == b);
}

template<typename T1, typename T2>
constexpr bool operator<(const pair<T1, T2>& a, const pair<T1, T2>& b) {
    if (a.first < b.first) return true;
    if (b.first < a.first) return false;
    return a.second < b.second;
}

template<typename T1, typename T2>
constexpr bool operator<=(const pair<T1, T2>& a, const pair<T1, T2>& b) {
    return !(b < a);
}

template<typename T1, typename T2>
constexpr bool operator>(const pair<T1, T2>& a, const pair<T1, T2>& b) {
    return b < a;
}

template<typename T1, typename T2>
constexpr bool operator>=(const pair<T1, T2>& a, const pair<T1, T2>& b) {
    return !(a < b);
}

// ============================================================
// pair — swap specialization
// ============================================================

template<typename T1, typename T2>
constexpr void swap(pair<T1, T2>& a, pair<T1, T2>& b)
    noexcept(noexcept(a.swap(b)))
{
    a.swap(b);
}

// ============================================================
// make_pair
// ============================================================

template<typename T1, typename T2>
constexpr pair<decay_t<T1>, decay_t<T2>> make_pair(T1&& x, T2&& y)
    noexcept(is_nothrow_constructible_v<decay_t<T1>, T1&&> &&
             is_nothrow_constructible_v<decay_t<T2>, T2&&>)
{
    return pair<decay_t<T1>, decay_t<T2>>(zstl::forward<T1>(x), zstl::forward<T2>(y));
}

// ============================================================
// Part 6: integer_sequence and index_sequence
// ============================================================

template<typename T, T... Ints>
struct integer_sequence {
    using value_type = T;
    static constexpr size_t size() noexcept { return sizeof...(Ints); }
};

template<size_t... Ints>
using index_sequence = integer_sequence<size_t, Ints...>;

// ============================================================
// make_integer_sequence implementation via logarithmic recursion
// ============================================================

namespace detail {

// Helper: given [0..M-1], produce [0..M-1, M..2M-1]
// So concat_n<M>::template apply<I0..IM-1> = integer_sequence<I0..IM-1, (I0+M)..(IM-1+M)>
template<size_t M>
struct concat_n {
    template<size_t... Is>
    static auto apply(index_sequence<Is...>)
        -> integer_sequence<size_t, Is..., (Is + M)...>;
};

// make_integer_sequence_impl<N>::type = integer_sequence<size_t, 0, 1, ..., N-1>
// Logarithmic: divides and conquers
template<size_t N>
struct make_integer_sequence_impl {
private:
    using half = typename make_integer_sequence_impl<N / 2>::type;
    using both = decltype(concat_n<N / 2>::apply(half{}));

    // For odd N, append N-1
    template<size_t... Is>
    static integer_sequence<size_t, Is..., N - 1>
    append_last(integer_sequence<size_t, Is...>);

    static constexpr bool is_odd = (N % 2) != 0;
public:
    using type = conditional_t<is_odd,
        decltype(append_last(both{})),
        both>;
};

template<>
struct make_integer_sequence_impl<0> {
    using type = integer_sequence<size_t>;
};

template<>
struct make_integer_sequence_impl<1> {
    using type = integer_sequence<size_t, 0>;
};

} // namespace detail

template<size_t N>
using make_index_sequence = typename detail::make_integer_sequence_impl<N>::type;

template<typename T, T N>
using make_integer_sequence = typename detail::make_integer_sequence_impl<static_cast<size_t>(N)>::type;  // NOTE: simplified — only works for T convertible to size_t

// For general integer types, provide a basic implementation
namespace detail {
template<typename T, T N>
struct make_int_seq_gen {
    // For non-size_t T, we generate index_sequence and cast
    template<size_t... Is>
    static integer_sequence<T, static_cast<T>(Is)...>
    cast_seq(index_sequence<Is...>);
    using type = decltype(cast_seq(make_index_sequence<static_cast<size_t>(N)>{}));
};
template<typename T, T N>
using make_int_seq_t = typename make_int_seq_gen<T, N>::type;
}

// make_integer_sequence for size_t uses the general (log-depth) implementation above;
// no alias-template partial specialization is needed (or valid).

// index_sequence_for — for parameter packs
template<typename... Ts>
using index_sequence_for = make_index_sequence<sizeof...(Ts)>;

// ============================================================
// Part 7: min / max / minmax / clamp
// ============================================================

// min — initializer_list overload
template<typename T>
constexpr T min(std::initializer_list<T> ilist) {
    const T* p = ilist.begin();
    const T* end = ilist.end();
    T result = *p;
    for (++p; p != end; ++p) {
        if (*p < result) result = *p;
    }
    return result;
}

// min — two-argument versions
template<typename T>
constexpr const T& min(const T& a, const T& b)
    noexcept(noexcept(a < b))
{
    return (b < a) ? b : a;
}

template<typename T, typename Compare>
constexpr const T& min(const T& a, const T& b, Compare comp)
    noexcept(noexcept(comp(a, b)))
{
    return comp(b, a) ? b : a;
}

// max — initializer_list overload
template<typename T>
constexpr T max(std::initializer_list<T> ilist) {
    const T* p = ilist.begin();
    const T* end = ilist.end();
    T result = *p;
    for (++p; p != end; ++p) {
        if (result < *p) result = *p;
    }
    return result;
}

// max — two-argument versions
template<typename T>
constexpr const T& max(const T& a, const T& b)
    noexcept(noexcept(a < b))
{
    return (a < b) ? b : a;
}

template<typename T, typename Compare>
constexpr const T& max(const T& a, const T& b, Compare comp)
    noexcept(noexcept(comp(a, b)))
{
    return comp(a, b) ? b : a;
}

// minmax — returns a pair of min and max
template<typename T>
constexpr pair<const T&, const T&> minmax(const T& a, const T& b)
    noexcept(noexcept(a < b))
{
    return (b < a) ? pair<const T&, const T&>(b, a)
                   : pair<const T&, const T&>(a, b);
}

template<typename T, typename Compare>
constexpr pair<const T&, const T&> minmax(const T& a, const T& b, Compare comp)
    noexcept(noexcept(comp(b, a)))
{
    return comp(b, a) ? pair<const T&, const T&>(b, a)
                      : pair<const T&, const T&>(a, b);
}

template<typename T>
constexpr pair<T, T> minmax(std::initializer_list<T> ilist) {
    auto a = ilist.begin();
    auto b = ilist.end();
    T min_val = *a;
    T max_val = *a;
    for (++a; a != b; ++a) {
        if (*a < min_val) min_val = *a;
        if (max_val < *a) max_val = *a;
    }
    return pair<T, T>(zstl::move(min_val), zstl::move(max_val));
}

template<typename T, typename Compare>
constexpr pair<T, T> minmax(std::initializer_list<T> ilist, Compare comp) {
    auto a = ilist.begin();
    auto b = ilist.end();
    T min_val = *a;
    T max_val = *a;
    for (++a; a != b; ++a) {
        if (comp(*a, min_val)) min_val = *a;
        if (comp(max_val, *a)) max_val = *a;
    }
    return pair<T, T>(zstl::move(min_val), zstl::move(max_val));
}

// clamp
template<typename T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi)
    noexcept(noexcept(v < lo) && noexcept(hi < v))
{
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

template<typename T, typename Compare>
constexpr const T& clamp(const T& v, const T& lo, const T& hi, Compare comp)
    noexcept(noexcept(comp(v, lo)) && noexcept(comp(hi, v)))
{
    return comp(v, lo) ? lo : comp(hi, v) ? hi : v;
}

// ============================================================
// Part 8: Less / Greater / EqualTo functors
// ============================================================

template<typename T = void>
struct less {
    constexpr bool operator()(const T& a, const T& b) const {
        return a < b;
    }
};

template<>
struct less<void> {
    using is_transparent = int;
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(zstl::forward<T>(a) < zstl::forward<U>(b)))
        -> decltype(zstl::forward<T>(a) < zstl::forward<U>(b))
    {
        return zstl::forward<T>(a) < zstl::forward<U>(b);
    }
};

template<typename T = void>
struct greater {
    constexpr bool operator()(const T& a, const T& b) const {
        return a > b;
    }
};

template<>
struct greater<void> {
    using is_transparent = int;
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(zstl::forward<T>(a) > zstl::forward<U>(b)))
        -> decltype(zstl::forward<T>(a) > zstl::forward<U>(b))
    {
        return zstl::forward<T>(a) > zstl::forward<U>(b);
    }
};

template<typename T = void>
struct equal_to {
    constexpr bool operator()(const T& a, const T& b) const {
        return a == b;
    }
};

template<>
struct equal_to<void> {
    using is_transparent = int;
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(zstl::forward<T>(a) == zstl::forward<U>(b)))
        -> decltype(zstl::forward<T>(a) == zstl::forward<U>(b))
    {
        return zstl::forward<T>(a) == zstl::forward<U>(b);
    }
};

// ============================================================
// Part 9: Identity / Select1st / Select2nd (key extractors)
// ============================================================

template<typename T>
struct identity {
    constexpr T& operator()(T& x) const noexcept { return x; }
    constexpr const T& operator()(const T& x) const noexcept { return x; }
};

template<typename Pair>
struct select1st {
    constexpr typename Pair::first_type&
    operator()(Pair& p) const noexcept { return p.first; }
    constexpr const typename Pair::first_type&
    operator()(const Pair& p) const noexcept { return p.first; }
};

template<typename Pair>
struct select2nd {
    constexpr typename Pair::second_type&
    operator()(Pair& p) const noexcept { return p.second; }
    constexpr const typename Pair::second_type&
    operator()(const Pair& p) const noexcept { return p.second; }
};

} // namespace zstl
