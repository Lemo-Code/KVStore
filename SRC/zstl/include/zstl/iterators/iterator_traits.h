// zstl iterator traits, tags, base iterator, and free functions
#pragma once

#include <iterator>
#include <cstddef>
#include "zstl/memory/type_traits.h"
#include "zstl/memory/utility.h"

namespace zstl {

// ============================================================
// Iterator category tags
// ============================================================
// C++17-compatible hierarchy: input -> forward -> bidirectional -> random_access -> contiguous
// output is standalone (only usable as a tag, not part of the inheritance hierarchy)
// ============================================================

using input_iterator_tag         = std::input_iterator_tag;
using output_iterator_tag        = std::output_iterator_tag;
using forward_iterator_tag       = std::forward_iterator_tag;
using bidirectional_iterator_tag = std::bidirectional_iterator_tag;
using random_access_iterator_tag = std::random_access_iterator_tag;
// std::contiguous_iterator_tag is C++20; for C++17 compatibility
// we use random_access_iterator_tag for pointers / contiguous iterators.
using contiguous_iterator_tag    = random_access_iterator_tag;

// ============================================================
// iterator_traits — primary template
// Extracts: iterator_category, value_type, difference_type, pointer, reference
// ============================================================
template<typename Iterator>
struct iterator_traits {
    using iterator_category = typename Iterator::iterator_category;
    using value_type        = typename Iterator::value_type;
    using difference_type   = typename Iterator::difference_type;
    using pointer           = typename Iterator::pointer;
    using reference         = typename Iterator::reference;
};

// Specialization for pointers (T*)
template<typename T>
struct iterator_traits<T*> {
    using iterator_category = contiguous_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;
};

// Specialization for const pointers (const T*)
template<typename T>
struct iterator_traits<const T*> {
    using iterator_category = contiguous_iterator_tag;
    using value_type        = T;
    using difference_type   = ::ptrdiff_t;
    using pointer           = const T*;
    using reference         = const T&;
};

// ============================================================
// Convenience type aliases
// ============================================================
template<typename Iterator>
using iterator_category_t = typename iterator_traits<Iterator>::iterator_category;

template<typename Iterator>
using iterator_value_t = typename iterator_traits<Iterator>::value_type;

template<typename Iterator>
using iterator_difference_t = typename iterator_traits<Iterator>::difference_type;

template<typename Iterator>
using iterator_pointer_t = typename iterator_traits<Iterator>::pointer;

template<typename Iterator>
using iterator_reference_t = typename iterator_traits<Iterator>::reference;

// ============================================================
// iterator — base class for defining custom iterators
// Provides the standard five typedefs so custom iterators
// only need to inherit and define operator* / operator++
// ============================================================
template<
    typename Category,
    typename T,
    typename Distance  = ptrdiff_t,
    typename Pointer   = T*,
    typename Reference = T&
>
struct iterator {
    using iterator_category = Category;
    using value_type        = T;
    using difference_type   = Distance;
    using pointer           = Pointer;
    using reference         = Reference;
};

// ============================================================
// Tag dispatch helpers — select algorithm based on iterator category
// ============================================================

// Check if an iterator category is at least a given category
namespace detail {

template<typename IteratorTag, typename RequiredTag>
struct has_iterator_category : std::is_base_of<RequiredTag, IteratorTag> {};

template<>
struct has_iterator_category<output_iterator_tag, output_iterator_tag> : std::true_type {};

template<>
struct has_iterator_category<output_iterator_tag, input_iterator_tag> : std::false_type {};

// Convenience variable templates for category checking
template<typename Iterator>
inline constexpr bool is_input_iterator_v =
    std::is_base_of_v<input_iterator_tag, iterator_category_t<Iterator>>;

template<typename Iterator>
inline constexpr bool is_forward_iterator_v =
    std::is_base_of_v<forward_iterator_tag, iterator_category_t<Iterator>>;

template<typename Iterator>
inline constexpr bool is_bidirectional_iterator_v =
    std::is_base_of_v<bidirectional_iterator_tag, iterator_category_t<Iterator>>;

template<typename Iterator>
inline constexpr bool is_random_access_iterator_v =
    std::is_base_of_v<random_access_iterator_tag, iterator_category_t<Iterator>>;

template<typename Iterator>
inline constexpr bool is_contiguous_iterator_v =
    std::is_base_of_v<contiguous_iterator_tag, iterator_category_t<Iterator>>;

} // namespace detail

// ============================================================
// advance — move iterator forward/backward by n steps
// O(1) for random-access, O(n) otherwise
// ============================================================
template<typename InputIterator, typename Distance>
constexpr void advance(InputIterator& it, Distance n) {
    if constexpr (std::is_base_of_v<random_access_iterator_tag,
                                     iterator_category_t<InputIterator>>) {
        it += static_cast<iterator_difference_t<InputIterator>>(n);
    } else {
        if constexpr (std::is_base_of_v<bidirectional_iterator_tag,
                                         iterator_category_t<InputIterator>>) {
            while (n > 0) { ++it; --n; }
            while (n < 0) { --it; ++n; }
        } else {
            // Forward-only: only support non-negative n
            while (n > 0) { ++it; --n; }
        }
    }
}

// ============================================================
// distance — compute distance between two iterators
// O(1) for random-access, O(n) otherwise
// ============================================================
template<typename InputIterator>
constexpr iterator_difference_t<InputIterator> distance(InputIterator first, InputIterator last) {
    if constexpr (std::is_base_of_v<random_access_iterator_tag,
                                     iterator_category_t<InputIterator>>) {
        return last - first;
    } else {
        iterator_difference_t<InputIterator> n = 0;
        while (first != last) {
            ++first;
            ++n;
        }
        return n;
    }
}

// ============================================================
// next / prev — return advanced iterator without modifying original
// ============================================================
template<typename InputIterator>
constexpr InputIterator next(InputIterator it,
                              iterator_difference_t<InputIterator> n = 1) {
    zstl::advance(it, n);
    return it;
}

template<typename BidirectionalIterator>
constexpr BidirectionalIterator prev(BidirectionalIterator it,
                                      iterator_difference_t<BidirectionalIterator> n = 1) {
    zstl::advance(it, -n);
    return it;
}

// ============================================================
// begin / end — free function wrappers for container/array access
// ============================================================

// C-style array overloads
template<typename T, size_t N>
constexpr T* begin(T (&arr)[N]) noexcept {
    return arr;
}

template<typename T, size_t N>
constexpr T* end(T (&arr)[N]) noexcept {
    return arr + N;
}

// Container overloads (SFINAE-friendly via .begin() / .end())
template<typename Container>
constexpr auto begin(Container& c) -> decltype(c.begin()) {
    return c.begin();
}

template<typename Container>
constexpr auto begin(const Container& c) -> decltype(c.begin()) {
    return c.begin();
}

template<typename Container>
constexpr auto end(Container& c) -> decltype(c.end()) {
    return c.end();
}

template<typename Container>
constexpr auto end(const Container& c) -> decltype(c.end()) {
    return c.end();
}

// ============================================================
// cbegin / cend — const-qualified begin/end
// ============================================================
template<typename Container>
constexpr auto cbegin(const Container& c) noexcept(noexcept(zstl::begin(c)))
    -> decltype(zstl::begin(c)) {
    return zstl::begin(c);
}

template<typename Container>
constexpr auto cend(const Container& c) noexcept(noexcept(zstl::end(c)))
    -> decltype(zstl::end(c)) {
    return zstl::end(c);
}

// ============================================================
// rbegin / rend — reverse begin/end
// ============================================================
template<typename T, size_t N>
constexpr std::reverse_iterator<T*> rbegin(T (&arr)[N]) noexcept {
    return std::reverse_iterator<T*>(arr + N);
}

template<typename T, size_t N>
constexpr std::reverse_iterator<T*> rend(T (&arr)[N]) noexcept {
    return std::reverse_iterator<T*>(arr);
}

template<typename Container>
constexpr auto rbegin(Container& c) -> decltype(c.rbegin()) {
    return c.rbegin();
}

template<typename Container>
constexpr auto rbegin(const Container& c) -> decltype(c.rbegin()) {
    return c.rbegin();
}

template<typename Container>
constexpr auto rend(Container& c) -> decltype(c.rend()) {
    return c.rend();
}

template<typename Container>
constexpr auto rend(const Container& c) -> decltype(c.rend()) {
    return c.rend();
}

// ============================================================
// crbegin / crend — const reverse begin/end
// ============================================================
template<typename Container>
constexpr auto crbegin(const Container& c) -> decltype(zstl::rbegin(c)) {
    return zstl::rbegin(c);
}

template<typename Container>
constexpr auto crend(const Container& c) -> decltype(zstl::rend(c)) {
    return zstl::rend(c);
}

// ============================================================
// size — free function for containers and arrays
// ============================================================
template<typename T, size_t N>
constexpr size_t size(const T (&/*arr*/)[N]) noexcept {
    return N;
}

template<typename Container>
constexpr auto size(const Container& c) -> decltype(c.size()) {
    return c.size();
}

// ============================================================
// empty — free function for containers and arrays
// ============================================================
template<typename T, size_t N>
constexpr bool empty(const T (&/*arr*/)[N]) noexcept {
    return false; // C-style arrays are never empty (N > 0)
}

template<typename Container>
constexpr auto empty(const Container& c) -> decltype(c.empty()) {
    return c.empty();
}

// ============================================================
// data — free function for containers and arrays
// ============================================================
template<typename T, size_t N>
constexpr T* data(T (&arr)[N]) noexcept {
    return arr;
}

template<typename Container>
constexpr auto data(Container& c) -> decltype(c.data()) {
    return c.data();
}

template<typename Container>
constexpr auto data(const Container& c) -> decltype(c.data()) {
    return c.data();
}

// ============================================================
// iter_swap — swap values pointed to by two iterators
// ============================================================
template<typename Iterator1, typename Iterator2>
constexpr void iter_swap(Iterator1 a, Iterator2 b) {
    zstl::swap(*a, *b);
}

} // namespace zstl
