// zstl move_iterator adapter — dereferences to rvalue references
#pragma once

#include "zstl/iterators/iterator_traits.h"
#include "zstl/memory/utility.h"

namespace zstl {

// ============================================================
// move_iterator<Iter>
// Adapter that converts lvalue dereferences into rvalue references,
// enabling move semantics when passing iterators to algorithms.
//   *mit returns zstl::move(*underlying)
// ============================================================
template<typename Iterator>
class move_iterator {
public:
    using iterator_type     = Iterator;
    using iterator_category = iterator_category_t<Iterator>;
    using value_type        = iterator_value_t<Iterator>;
    using difference_type   = iterator_difference_t<Iterator>;
    // For move_iterator, pointer is the underlying iterator itself
    // (so operator-> returns the underlying iterator for chained access)
    using pointer           = Iterator;
    // reference is always an rvalue reference to value_type
    using reference         = value_type&&;

    // ---- constructors ----
    constexpr move_iterator() noexcept(noexcept(Iterator())) : current_() {}

    explicit constexpr move_iterator(Iterator it) noexcept(noexcept(Iterator(it))) : current_(it) {}

    constexpr move_iterator(const move_iterator&) = default;
    constexpr move_iterator& operator=(const move_iterator&) = default;

    // Converting constructor from compatible move_iterator
    template<typename U>
    constexpr move_iterator(const move_iterator<U>& other) noexcept(noexcept(Iterator(other.base())))
        : current_(other.base()) {}

    // ---- base accessor ----
    constexpr Iterator base() const noexcept(noexcept(Iterator(current_))) {
        return current_;
    }

    // ---- dereference ----
    // Returns rvalue reference, enabling move semantics
    constexpr reference operator*() const noexcept(noexcept(zstl::move(*current_))) {
        return zstl::move(*current_);
    }

    // Returns the underlying iterator for -> chaining
    constexpr pointer operator->() const noexcept(noexcept(Iterator(current_))) {
        return current_;
    }

    // ---- element access (random-access only) ----
    constexpr reference operator[](difference_type n) const noexcept(noexcept(zstl::move(current_[n]))) {
        return zstl::move(current_[n]);
    }

    // ---- increment / decrement ----
    constexpr move_iterator& operator++() noexcept(noexcept(++current_)) {
        ++current_;
        return *this;
    }

    constexpr move_iterator operator++(int) noexcept(noexcept(move_iterator(*this), ++current_)) {
        move_iterator tmp = *this;
        ++current_;
        return tmp;
    }

    constexpr move_iterator& operator--() noexcept(noexcept(--current_)) {
        --current_;
        return *this;
    }

    constexpr move_iterator operator--(int) noexcept(noexcept(move_iterator(*this), --current_)) {
        move_iterator tmp = *this;
        --current_;
        return tmp;
    }

    // ---- arithmetic operators (random-access only) ----
    constexpr move_iterator operator+(difference_type n) const noexcept(noexcept(move_iterator(current_ + n))) {
        return move_iterator(current_ + n);
    }

    constexpr move_iterator operator-(difference_type n) const noexcept(noexcept(move_iterator(current_ - n))) {
        return move_iterator(current_ - n);
    }

    constexpr move_iterator& operator+=(difference_type n) noexcept(noexcept(current_ += n)) {
        current_ += n;
        return *this;
    }

    constexpr move_iterator& operator-=(difference_type n) noexcept(noexcept(current_ -= n)) {
        current_ -= n;
        return *this;
    }

private:
    Iterator current_;
};

// ============================================================
// Non-member operator+ (difference_type + move_iterator)
// ============================================================
template<typename Iterator>
constexpr move_iterator<Iterator> operator+(
    typename move_iterator<Iterator>::difference_type n,
    const move_iterator<Iterator>& it) noexcept(noexcept(it + n)) {
    return it + n;
}

// ============================================================
// Equality / inequality
// ============================================================
template<typename Iterator1, typename Iterator2>
constexpr bool operator==(const move_iterator<Iterator1>& a,
                           const move_iterator<Iterator2>& b) noexcept(noexcept(a.base() == b.base())) {
    return a.base() == b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator!=(const move_iterator<Iterator1>& a,
                           const move_iterator<Iterator2>& b) noexcept(noexcept(a.base() != b.base())) {
    return a.base() != b.base();
}

// ============================================================
// Relational operators
// ============================================================
template<typename Iterator1, typename Iterator2>
constexpr bool operator<(const move_iterator<Iterator1>& a,
                          const move_iterator<Iterator2>& b) noexcept(noexcept(a.base() < b.base())) {
    return a.base() < b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator>(const move_iterator<Iterator1>& a,
                          const move_iterator<Iterator2>& b) noexcept(noexcept(a.base() > b.base())) {
    return a.base() > b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator<=(const move_iterator<Iterator1>& a,
                           const move_iterator<Iterator2>& b) noexcept(noexcept(a.base() <= b.base())) {
    return a.base() <= b.base();
}

template<typename Iterator1, typename Iterator2>
constexpr bool operator>=(const move_iterator<Iterator1>& a,
                           const move_iterator<Iterator2>& b) noexcept(noexcept(a.base() >= b.base())) {
    return a.base() >= b.base();
}

// ============================================================
// Difference operator
// ============================================================
template<typename Iterator1, typename Iterator2>
constexpr auto operator-(const move_iterator<Iterator1>& a,
                          const move_iterator<Iterator2>& b)
    noexcept(noexcept(a.base() - b.base()))
    -> decltype(a.base() - b.base()) {
    return a.base() - b.base();
}

// ============================================================
// make_move_iterator — convenience factory
// ============================================================
template<typename Iterator>
constexpr move_iterator<Iterator> make_move_iterator(Iterator it) noexcept(noexcept(move_iterator<Iterator>(it))) {
    return move_iterator<Iterator>(it);
}

} // namespace zstl
