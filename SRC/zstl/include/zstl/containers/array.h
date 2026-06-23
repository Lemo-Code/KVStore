// zstl array — fixed-size array with zero overhead over C-style array
//
// array<T, N> is an aggregate type (no user-defined constructors) wrapping
// a C-style array. Provides the standard container interface without any
// dynamic allocation or overhead.
//
// Complexity: all operations are O(1) except fill() which is O(N).
// Memory: exactly sizeof(T) * N bytes (no extra bookkeeping).
#pragma once

#include <cstddef>
#include <stdexcept>
#include <tuple>
#include "zstl/memory/utility.h"
#include "zstl/iterators/iterator_traits.h"
#include "zstl/iterators/reverse_iterator.h"

namespace zstl {

template<typename T, size_t N>
struct array {
    // ---- types ----
    using value_type      = T;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;
    using iterator        = T*;
    using const_iterator  = const T*;
    using reverse_iterator       = zstl::reverse_iterator<iterator>;
    using const_reverse_iterator = zstl::reverse_iterator<const_iterator>;

    // ---- data ----
    T elems_[N];  // Public aggregate (C++17 structured binding compatible)

    // ---- element access ----
    constexpr reference operator[](size_type i) noexcept { return elems_[i]; }
    constexpr const_reference operator[](size_type i) const noexcept { return elems_[i]; }

    constexpr reference at(size_type i) {
        if (i >= N) throw std::out_of_range("array::at: index out of range");
        return elems_[i];
    }
    constexpr const_reference at(size_type i) const {
        if (i >= N) throw std::out_of_range("array::at: index out of range");
        return elems_[i];
    }

    constexpr reference front() noexcept { return elems_[0]; }
    constexpr const_reference front() const noexcept { return elems_[0]; }
    constexpr reference back() noexcept { return elems_[N - 1]; }
    constexpr const_reference back() const noexcept { return elems_[N - 1]; }
    constexpr pointer data() noexcept { return elems_; }
    constexpr const_pointer data() const noexcept { return elems_; }

    // ---- iterators ----
    constexpr iterator begin() noexcept { return elems_; }
    constexpr const_iterator begin() const noexcept { return elems_; }
    constexpr const_iterator cbegin() const noexcept { return elems_; }
    constexpr iterator end() noexcept { return elems_ + N; }
    constexpr const_iterator end() const noexcept { return elems_ + N; }
    constexpr const_iterator cend() const noexcept { return elems_ + N; }

    constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
    constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    // ---- capacity ----
    constexpr bool empty() const noexcept { return N == 0; }
    constexpr size_type size() const noexcept { return N; }
    constexpr size_type max_size() const noexcept { return N; }

    // ---- modifiers ----
    constexpr void fill(const T& value) {
        for (size_type i = 0; i < N; ++i) {
            elems_[i] = value;
        }
    }

    constexpr void swap(array& other) noexcept {
        for (size_type i = 0; i < N; ++i) {
            zstl::swap(elems_[i], other.elems_[i]);
        }
    }

    // ============================================================
    // Comparison operators (member, for ADL)
    // ============================================================
    friend constexpr bool operator==(const array& a, const array& b) {
        for (size_type i = 0; i < N; ++i) {
            if (!(a[i] == b[i])) return false;
        }
        return true;
    }

    friend constexpr bool operator!=(const array& a, const array& b) {
        return !(a == b);
    }

    friend constexpr bool operator<(const array& a, const array& b) {
        for (size_type i = 0; i < N; ++i) {
            if (a[i] < b[i]) return true;
            if (b[i] < a[i]) return false;
        }
        return false;
    }

    friend constexpr bool operator>(const array& a, const array& b) {
        return b < a;
    }

    friend constexpr bool operator<=(const array& a, const array& b) {
        return !(b < a);
    }

    friend constexpr bool operator>=(const array& a, const array& b) {
        return !(a < b);
    }
};

// ============================================================
// Zero-size specialization
// ============================================================
template<typename T>
struct array<T, 0> {
    using value_type      = T;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;
    using iterator        = T*;
    using const_iterator  = const T*;
    using reverse_iterator       = zstl::reverse_iterator<iterator>;
    using const_reverse_iterator = zstl::reverse_iterator<const_iterator>;

    // Empty data member (sizeof(array<T,0>) > 0 per the standard)
    char dummy_;

    constexpr reference operator[](size_type) noexcept { return *static_cast<T*>(nullptr); /* UB if used */ }
    constexpr const_reference operator[](size_type) const noexcept { return *static_cast<const T*>(nullptr); }

    constexpr reference at(size_type) {
        throw std::out_of_range("array<T,0>::at: index out of range");
    }
    constexpr const_reference at(size_type) const {
        throw std::out_of_range("array<T,0>::at: index out of range");
    }

    constexpr reference front() noexcept { return operator[](0); }
    constexpr const_reference front() const noexcept { return operator[](0); }
    constexpr reference back() noexcept { return operator[](0); }
    constexpr const_reference back() const noexcept { return operator[](0); }

    constexpr pointer data() noexcept { return nullptr; }
    constexpr const_pointer data() const noexcept { return nullptr; }

    constexpr iterator begin() noexcept { return nullptr; }
    constexpr const_iterator begin() const noexcept { return nullptr; }
    constexpr const_iterator cbegin() const noexcept { return nullptr; }
    constexpr iterator end() noexcept { return nullptr; }
    constexpr const_iterator end() const noexcept { return nullptr; }
    constexpr const_iterator cend() const noexcept { return nullptr; }

    constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(nullptr); }
    constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(nullptr); }
    constexpr reverse_iterator rend() noexcept { return reverse_iterator(nullptr); }
    constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(nullptr); }

    constexpr bool empty() const noexcept { return true; }
    constexpr size_type size() const noexcept { return 0; }
    constexpr size_type max_size() const noexcept { return 0; }

    constexpr void fill(const T&) noexcept {}
    constexpr void swap(array&) noexcept {}

    friend constexpr bool operator==(const array&, const array&) { return true; }
    friend constexpr bool operator!=(const array&, const array&) { return false; }
    friend constexpr bool operator<(const array&, const array&) { return false; }
    friend constexpr bool operator>(const array&, const array&) { return false; }
    friend constexpr bool operator<=(const array&, const array&) { return true; }
    friend constexpr bool operator>=(const array&, const array&) { return true; }
};

// ============================================================
// Non-member swap
// ============================================================
template<typename T, size_t N>
constexpr void swap(array<T, N>& a, array<T, N>& b) noexcept {
    a.swap(b);
}

// ============================================================
// Comparison operators
// ============================================================
template<typename T, size_t N>
constexpr bool operator==(const array<T, N>& a, const array<T, N>& b) {
    for (size_t i = 0; i < N; ++i) {
        if (!(a[i] == b[i])) return false;
    }
    return true;
}

template<typename T, size_t N>
constexpr bool operator!=(const array<T, N>& a, const array<T, N>& b) {
    return !(a == b);
}

template<typename T, size_t N>
constexpr bool operator<(const array<T, N>& a, const array<T, N>& b) {
    for (size_t i = 0; i < N; ++i) {
        if (a[i] < b[i]) return true;
        if (b[i] < a[i]) return false;
    }
    return false;
}

template<typename T, size_t N>
constexpr bool operator>(const array<T, N>& a, const array<T, N>& b) {
    return b < a;
}

template<typename T, size_t N>
constexpr bool operator<=(const array<T, N>& a, const array<T, N>& b) {
    return !(b < a);
}

template<typename T, size_t N>
constexpr bool operator>=(const array<T, N>& a, const array<T, N>& b) {
    return !(a < b);
}

// ============================================================
// Tuple-like access
// ============================================================
template<size_t I, typename T, size_t N>
constexpr T& get(array<T, N>& a) noexcept {
    static_assert(I < N, "array::get: index out of bounds");
    return a.elems_[I];
}

template<size_t I, typename T, size_t N>
constexpr const T& get(const array<T, N>& a) noexcept {
    static_assert(I < N, "array::get: index out of bounds");
    return a.elems_[I];
}

template<size_t I, typename T, size_t N>
constexpr T&& get(array<T, N>&& a) noexcept {
    static_assert(I < N, "array::get: index out of bounds");
    return zstl::move(a.elems_[I]);
}

template<size_t I, typename T, size_t N>
constexpr const T&& get(const array<T, N>&& a) noexcept {
    static_assert(I < N, "array::get: index out of bounds");
    return zstl::move(a.elems_[I]);
}

} // namespace zstl

// ============================================================
// tuple_size / tuple_element specializations (must be in std namespace)
// ============================================================
namespace std {

template<typename T, size_t N>
struct tuple_size<zstl::array<T, N>> : integral_constant<size_t, N> {};

template<size_t I, typename T, size_t N>
struct tuple_element<I, zstl::array<T, N>> {
    using type = T;
};

} // namespace std
