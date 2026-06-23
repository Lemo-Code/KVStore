// zstl reverse_iterator adapter — traverses a range in reverse order
// ============================================================
// reverse_iterator<Iter> wraps a bidirectional or random-access
// iterator and reverses its direction of traversal.
//
// Key semantics:
//   - Constructed from an underlying iterator it.
//   - *rit returns *(it-1) — the element *before* the stored position.
//     This is so that rbegin() (wrapping end()) dereferences to last-1.
//   - ++rit decrements the underlying iterator.
//   - --rit increments the underlying iterator.
//   - Comparison operators are reversed for relational ops.
//   - subtraction: rit1 - rit2 == rit2.base() - rit1.base()
//
// Example:
//   zstl::reverse_iterator rit = zstl::make_reverse_iterator(vec.end());
//   for (; rit != zstl::make_reverse_iterator(vec.begin()); ++rit) { ... }
// ============================================================
#pragma once

#include "zstl/iterators/iterator_traits.h"

namespace zstl {

// ============================================================
// reverse_iterator<Iter> class definition
// ============================================================
template<typename Iterator>
class reverse_iterator {
public:
    using iterator_type     = Iterator;
    using iterator_category = iterator_category_t<Iterator>;
    using value_type        = iterator_value_t<Iterator>;
    using difference_type   = iterator_difference_t<Iterator>;
    using pointer           = iterator_pointer_t<Iterator>;
    using reference         = iterator_reference_t<Iterator>;

    // ---- constructors ----
    constexpr reverse_iterator() noexcept(noexcept(Iterator())) : current_() {}

    explicit constexpr reverse_iterator(Iterator it) noexcept(noexcept(Iterator(it))) : current_(it) {}

    constexpr reverse_iterator(const reverse_iterator&) = default;
    constexpr reverse_iterator& operator=(const reverse_iterator&) = default;

    // Converting constructor from compatible reverse_iterator
    template<typename U>
    constexpr reverse_iterator(const reverse_iterator<U>& other) noexcept(noexcept(Iterator(other.base())))
        : current_(other.base()) {}

    // ---- base accessor ---- returns the underlying iterator
    constexpr Iterator base() const noexcept(noexcept(Iterator(current_))) {
        return current_;
    }

    // ---- dereference ----
    // Returns the element *before* the stored position.
    // For a reverse_iterator wrapping it: *rit == *(it - 1)
    constexpr reference operator*() const noexcept(noexcept(*--Iterator(current_))) {
        Iterator tmp = current_;
        --tmp;
        return *tmp;
    }

    constexpr pointer operator->() const noexcept(noexcept(&(*(*this)))) {
        return &(operator*());
    }

    // ---- element access (random-access only) ----
    constexpr reference operator[](difference_type n) const noexcept(noexcept(*(*this + n))) {
        return *(*this + n);
    }

    // ---- increment / decrement ----
    // ++rit  ->  --current_
    constexpr reverse_iterator& operator++() noexcept(noexcept(--current_)) {
        --current_;
        return *this;
    }

    constexpr reverse_iterator operator++(int) noexcept(noexcept(reverse_iterator(*this), --current_)) {
        reverse_iterator tmp = *this;
        --current_;
        return tmp;
    }

    // --rit  ->  ++current_
    constexpr reverse_iterator& operator--() noexcept(noexcept(++current_)) {
        ++current_;
        return *this;
    }

    constexpr reverse_iterator operator--(int) noexcept(noexcept(reverse_iterator(*this), ++current_)) {
        reverse_iterator tmp = *this;
        ++current_;
        return tmp;
    }

    // ---- arithmetic operators (random-access only) ----
    constexpr reverse_iterator operator+(difference_type n) const noexcept(noexcept(reverse_iterator(current_ - n))) {
        return reverse_iterator(current_ - n);
    }

    constexpr reverse_iterator operator-(difference_type n) const noexcept(noexcept(reverse_iterator(current_ + n))) {
        return reverse_iterator(current_ + n);
    }

    constexpr reverse_iterator& operator+=(difference_type n) noexcept(noexcept(current_ -= n)) {
        current_ -= n;
        return *this;
    }

    constexpr reverse_iterator& operator-=(difference_type n) noexcept(noexcept(current_ += n)) {
        current_ += n;
        return *this;
    }

    // Friend declarations for heterogeneous comparison and arithmetic
    template<typename I1, typename I2>
    friend constexpr bool operator==(const reverse_iterator<I1>&, const reverse_iterator<I2>&) noexcept;
    template<typename I1, typename I2>
    friend constexpr bool operator<(const reverse_iterator<I1>&, const reverse_iterator<I2>&) noexcept;
    template<typename I1, typename I2>
    friend constexpr auto operator-(const reverse_iterator<I1>&, const reverse_iterator<I2>&) noexcept;

protected:
    Iterator current_;
};

// ============================================================
// Non-member operator+(n, rit) — supports n + reverse_iterator
// ============================================================
template<typename Iterator>
constexpr reverse_iterator<Iterator> operator+(
    typename reverse_iterator<Iterator>::difference_type n,
    const reverse_iterator<Iterator>& it) noexcept(noexcept(it + n)) {
    return it + n;
}

// ============================================================
// Equality / inequality
// ============================================================
template<typename Iterator1, typename Iterator2>
constexpr bool operator==(const reverse_iterator<Iterator1>& a,
                           const reverse_iterator<Iterator2>& b) noexcept(noexcept(a.base() == b.base())) {
    return a.base() == b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator!=(const reverse_iterator<Iterator1>& a,
                           const reverse_iterator<Iterator2>& b) noexcept(noexcept(a.base() != b.base())) {
    return a.base() != b.base();
}

// ============================================================
// Relational operators (<, >, <=, >=)
// Note: reversed semantics because reverse_iterator traverses backwards
//   a < b  means a reaches b later in forward-direction terms
// ============================================================
template<typename Iterator1, typename Iterator2>
constexpr bool operator<(const reverse_iterator<Iterator1>& a,
                          const reverse_iterator<Iterator2>& b) noexcept(noexcept(a.base() > b.base())) {
    return a.base() > b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator>(const reverse_iterator<Iterator1>& a,
                          const reverse_iterator<Iterator2>& b) noexcept(noexcept(a.base() < b.base())) {
    return a.base() < b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator<=(const reverse_iterator<Iterator1>& a,
                           const reverse_iterator<Iterator2>& b) noexcept(noexcept(a.base() >= b.base())) {
    return a.base() >= b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator>=(const reverse_iterator<Iterator1>& a,
                           const reverse_iterator<Iterator2>& b) noexcept(noexcept(a.base() <= b.base())) {
    return a.base() <= b.base();
}

// ============================================================
// Difference operator
// Returns the distance between two reverse_iterators.
//   a - b == b.base() - a.base()  (reversed)
// ============================================================
template<typename Iterator1, typename Iterator2>
constexpr auto operator-(const reverse_iterator<Iterator1>& a,
                          const reverse_iterator<Iterator2>& b)
    noexcept(noexcept(b.base() - a.base()))
    -> decltype(b.base() - a.base()) {
    return b.base() - a.base();
}

// ============================================================
// make_reverse_iterator — convenience factory
// ============================================================
template<typename Iterator>
constexpr reverse_iterator<Iterator> make_reverse_iterator(Iterator it) noexcept(noexcept(reverse_iterator<Iterator>(it))) {
    return reverse_iterator<Iterator>(it);
}

// ============================================================
// Type trait: is_reverse_iterator
// ============================================================
template<typename T>
struct is_reverse_iterator : std::false_type {};

template<typename Iterator>
struct is_reverse_iterator<reverse_iterator<Iterator>> : std::true_type {};

template<typename T>
inline constexpr bool is_reverse_iterator_v = is_reverse_iterator<T>::value;

// ============================================================
// Underlying iterator type extractor for reverse_iterator
// ============================================================
template<typename T>
struct reverse_iterator_underlying;

template<typename Iterator>
struct reverse_iterator_underlying<reverse_iterator<Iterator>> {
    using type = Iterator;
};

template<typename T>
using reverse_iterator_underlying_t = typename reverse_iterator_underlying<T>::type;

} // namespace zstl
