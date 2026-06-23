// zstl construct — placement construction, destruction, and uninitialized
// memory operations. Optimized with memmove for trivially relocatable types.
#pragma once

#include <new>
#include <cstring>        // for memmove, memset
#include <iterator>       // for iterator_traits
#include "zstl/memory/type_traits.h"
#include "zstl/memory/utility.h"

namespace zstl {

// ============================================================
// Part 1: construct — placement-new a single object
// ============================================================

// Default construct
template<typename T>
void construct(T* p)
    noexcept(is_nothrow_default_constructible_v<T>)
{
    ::new (static_cast<void*>(p)) T;
}

// Construct from one value
template<typename T, typename U>
void construct(T* p, U&& value)
    noexcept(is_nothrow_constructible_v<T, U&&>)
{
    ::new (static_cast<void*>(p)) T(zstl::forward<U>(value));
}

// Construct with arbitrary arguments
template<typename T, typename... Args>
void construct(T* p, Args&&... args)
    noexcept(is_nothrow_constructible_v<T, Args&&...>)
{
    ::new (static_cast<void*>(p)) T(zstl::forward<Args>(args)...);
}

// ============================================================
// Part 2: destroy / destroy_at — destruct a single object
// ============================================================

// destroy_at — C++17 interface, calls destructor on a single object
template<typename T>
void destroy_at(T* p) noexcept {
    if constexpr (!is_trivially_destructible_v<T>) {
        if (p) p->~T();
    }
}

// destroy — convenience alias
template<typename T>
void destroy(T* p) noexcept {
    destroy_at(p);
}

// ============================================================
// Part 3: destroy_range — destroy [first, last)
// ============================================================

template<typename ForwardIterator>
void destroy_range(ForwardIterator first, ForwardIterator last) noexcept {
    using value_type = typename std::iterator_traits<ForwardIterator>::value_type;
    if constexpr (is_trivially_destructible_v<value_type>) {
        // No-op: trivially destructible types need no teardown
    } else {
        for (; first != last; ++first) {
            destroy_at(zstl::addressof(*first));
        }
    }
}

// destroy_n — destroy n elements starting at first
template<typename ForwardIterator, typename Size>
ForwardIterator destroy_n(ForwardIterator first, Size n) noexcept {
    using value_type = typename std::iterator_traits<ForwardIterator>::value_type;
    if constexpr (is_trivially_destructible_v<value_type>) {
        // No-op
    } else {
        for (Size i = 0; i < n; ++i, ++first) {
            destroy_at(zstl::addressof(*first));
        }
    }
    return first;
}

// ============================================================
// Part 4: uninitialized_copy — copy [first, last) to
//          uninitialized memory at result
// ============================================================

template<typename InputIterator, typename ForwardIterator>
ForwardIterator uninitialized_copy(InputIterator first, InputIterator last,
                                    ForwardIterator result) {
    using T = typename std::iterator_traits<ForwardIterator>::value_type;

    // Fast path: memmove for trivially relocatable (POD-like) types
    if constexpr (is_trivially_relocatable_v<T>) {
        if (first != last) {
            // Compute byte count from iterator distance (works for
            // contiguous iterators like pointers).
            auto count = static_cast<size_t>(last - first);
            if (count > 0) {
                __builtin_memmove(static_cast<void*>(zstl::addressof(*result)),
                                  static_cast<const void*>(zstl::addressof(*first)),
                                  count * sizeof(T));
            }
            return result + count;
        }
        return result;
    }

    // Slow path: element-by-element with exception safety
    ForwardIterator cur = result;
    try {
        for (; first != last; ++first, (void)++cur) {
            construct(zstl::addressof(*cur), *first);
        }
    } catch (...) {
        // Roll back: destroy already-constructed elements
        destroy_range(result, cur);
        throw;
    }
    return cur;
}

// ============================================================
// Part 5: uninitialized_copy_n — copy n elements
// ============================================================

template<typename InputIterator, typename Size, typename ForwardIterator>
ForwardIterator uninitialized_copy_n(InputIterator first, Size n,
                                      ForwardIterator result) {
    using T = typename std::iterator_traits<ForwardIterator>::value_type;

    if constexpr (is_trivially_relocatable_v<T>) {
        if (n > 0) {
            __builtin_memmove(static_cast<void*>(zstl::addressof(*result)),
                              static_cast<const void*>(zstl::addressof(*first)),
                              static_cast<size_t>(n) * sizeof(T));
        }
        return result + n;
    }

    ForwardIterator cur = result;
    try {
        for (Size i = 0; i < n; ++i, (void)++cur, (void)++first) {
            construct(zstl::addressof(*cur), *first);
        }
    } catch (...) {
        destroy_range(result, cur);
        throw;
    }
    return cur;
}

// ============================================================
// Part 6: uninitialized_fill — fill [first, last) with value
// ============================================================

template<typename ForwardIterator, typename T>
void uninitialized_fill(ForwardIterator first, ForwardIterator last,
                         const T& value) {
    using VT = typename std::iterator_traits<ForwardIterator>::value_type;

    // Fast path: memset for byte-like POD types (single-byte value types)
    if constexpr (is_trivially_copyable_v<VT> && sizeof(VT) == 1) {
        if (first != last) {
            __builtin_memset(static_cast<void*>(zstl::addressof(*first)),
                             *reinterpret_cast<const unsigned char*>(&value),
                             static_cast<size_t>(last - first));
        }
        return;
    }

    // General path
    ForwardIterator cur = first;
    try {
        for (; cur != last; ++cur) {
            construct(zstl::addressof(*cur), value);
        }
    } catch (...) {
        destroy_range(first, cur);
        throw;
    }
}

// ============================================================
// Part 7: uninitialized_fill_n — fill n elements
// ============================================================

template<typename ForwardIterator, typename Size, typename T>
ForwardIterator uninitialized_fill_n(ForwardIterator first, Size n,
                                      const T& value) {
    using VT = typename std::iterator_traits<ForwardIterator>::value_type;

    if constexpr (is_trivially_copyable_v<VT> && sizeof(VT) == 1) {
        if (n > 0) {
            __builtin_memset(static_cast<void*>(zstl::addressof(*first)),
                             *reinterpret_cast<const unsigned char*>(&value),
                             static_cast<size_t>(n));
        }
        return first + n;
    }

    ForwardIterator cur = first;
    try {
        for (Size i = 0; i < n; ++i, (void)++cur) {
            construct(zstl::addressof(*cur), value);
        }
    } catch (...) {
        destroy_range(first, cur);
        throw;
    }
    return cur;
}

// ============================================================
// Part 8: uninitialized_move — move [first, last) to
//          uninitialized memory at result
// ============================================================

template<typename InputIterator, typename ForwardIterator>
ForwardIterator uninitialized_move(InputIterator first, InputIterator last,
                                    ForwardIterator result) {
    using T = typename std::iterator_traits<ForwardIterator>::value_type;

    // Fast path: memmove for trivially relocatable types
    // (identifies types where move == memcpy and destructor is a no-op)
    if constexpr (is_trivially_relocatable_v<T>) {
        if (first != last) {
            auto count = static_cast<size_t>(last - first);
            if (count > 0) {
                __builtin_memmove(static_cast<void*>(zstl::addressof(*result)),
                                  static_cast<const void*>(zstl::addressof(*first)),
                                  count * sizeof(T));
            }
            return result + count;
        }
        return result;
    }

    // Slow path: move-construct each element with rollback
    ForwardIterator cur = result;
    try {
        for (; first != last; ++first, (void)++cur) {
            construct(zstl::addressof(*cur), zstl::move(*first));
        }
    } catch (...) {
        destroy_range(result, cur);
        throw;
    }
    return cur;
}

// ============================================================
// Part 9: uninitialized_move_n — move n elements
// ============================================================

template<typename InputIterator, typename Size, typename ForwardIterator>
ForwardIterator uninitialized_move_n(InputIterator first, Size n,
                                      ForwardIterator result) {
    using T = typename std::iterator_traits<ForwardIterator>::value_type;

    if constexpr (is_trivially_relocatable_v<T>) {
        if (n > 0) {
            __builtin_memmove(static_cast<void*>(zstl::addressof(*result)),
                              static_cast<const void*>(zstl::addressof(*first)),
                              static_cast<size_t>(n) * sizeof(T));
        }
        return result + n;
    }

    ForwardIterator cur = result;
    try {
        for (Size i = 0; i < n; ++i, (void)++cur, (void)++first) {
            construct(zstl::addressof(*cur), zstl::move(*first));
        }
    } catch (...) {
        destroy_range(result, cur);
        throw;
    }
    return cur;
}

// ============================================================
// Part 10: uninitialized_default_construct — default construct
//          [first, last)
// ============================================================

template<typename ForwardIterator>
void uninitialized_default_construct(ForwardIterator first,
                                      ForwardIterator last) {
    using T = typename std::iterator_traits<ForwardIterator>::value_type;

    // Trivial types: no initialization needed (indeterminate value OK)
    if constexpr (is_trivially_default_constructible_v<T>) {
        return;  // No-op: memory already "exists"
    }

    ForwardIterator cur = first;
    try {
        for (; cur != last; ++cur) {
            construct(zstl::addressof(*cur));
        }
    } catch (...) {
        destroy_range(first, cur);
        throw;
    }
}

// ============================================================
// Part 11: uninitialized_default_construct_n — default construct
//          n elements
// ============================================================

template<typename ForwardIterator, typename Size>
ForwardIterator uninitialized_default_construct_n(ForwardIterator first,
                                                    Size n) {
    using T = typename std::iterator_traits<ForwardIterator>::value_type;

    if constexpr (is_trivially_default_constructible_v<T>) {
        return first + n;
    }

    ForwardIterator cur = first;
    try {
        for (Size i = 0; i < n; ++i, (void)++cur) {
            construct(zstl::addressof(*cur));
        }
    } catch (...) {
        destroy_range(first, cur);
        throw;
    }
    return cur;
}

// ============================================================
// Part 12: uninitialized_value_construct — value construct
//          (zero-initialize) [first, last)
// ============================================================

template<typename ForwardIterator>
void uninitialized_value_construct(ForwardIterator first,
                                    ForwardIterator last) {
    using T = typename std::iterator_traits<ForwardIterator>::value_type;

    // For scalar types, value-initialization == zero-initialization
    if constexpr (is_scalar_v<T> && is_trivially_default_constructible_v<T>) {
        if (first != last) {
            __builtin_memset(static_cast<void*>(zstl::addressof(*first)), 0,
                             static_cast<size_t>(last - first) * sizeof(T));
        }
        return;
    }

    ForwardIterator cur = first;
    try {
        for (; cur != last; ++cur) {
            // Value-initialize: T() for fundamental types == zero
            construct(zstl::addressof(*cur));
        }
    } catch (...) {
        destroy_range(first, cur);
        throw;
    }
}

// ============================================================
// Part 13: uninitialized_value_construct_n — value construct
//          n elements
// ============================================================

template<typename ForwardIterator, typename Size>
ForwardIterator uninitialized_value_construct_n(ForwardIterator first,
                                                  Size n) {
    using T = typename std::iterator_traits<ForwardIterator>::value_type;

    if constexpr (is_scalar_v<T> && is_trivially_default_constructible_v<T>) {
        if (n > 0) {
            __builtin_memset(static_cast<void*>(zstl::addressof(*first)), 0,
                             static_cast<size_t>(n) * sizeof(T));
        }
        return first + n;
    }

    ForwardIterator cur = first;
    try {
        for (Size i = 0; i < n; ++i, (void)++cur) {
            construct(zstl::addressof(*cur));
        }
    } catch (...) {
        destroy_range(first, cur);
        throw;
    }
    return cur;
}

// ============================================================
// Part 14: relocatable optimization — bulk move via memmove
// ============================================================
// relocate_n moves n elements from src to uninitialized dst and
// destroys the source objects (or not, for trivially destructible types).
// This is the key optimization behind vector reallocation.

template<typename T>
void relocate_n(T* src, size_t n, T* dst) noexcept {
    if constexpr (is_trivially_relocatable_v<T>) {
        // Fast path: single memmove, no destructor calls needed
        if (n > 0 && src != dst) {
            __builtin_memmove(static_cast<void*>(dst),
                              static_cast<const void*>(src),
                              n * sizeof(T));
        }
    } else if constexpr (is_nothrow_move_constructible_v<T> &&
                         is_trivially_destructible_v<T>) {
        // Medium path: move-construct each, but destructor is trivial
        for (size_t i = 0; i < n; ++i) {
            construct(dst + i, zstl::move(src[i]));
        }
    } else {
        // Full path: move-construct with rollback
        size_t i = 0;
        try {
            for (; i < n; ++i) {
                construct(dst + i, zstl::move(src[i]));
            }
        } catch (...) {
            for (size_t j = 0; j < i; ++j) {
                destroy_at(dst + j);
            }
            throw;
        }
        // Destroy source after successful move
        for (size_t j = 0; j < n; ++j) {
            destroy_at(src + j);
        }
    }
}

// relocate_backward_n — like relocate_n but copies backward
// (src + n - 1 down to src) to (dst + n - 1 down to dst).
// Used when dst overlaps src and dst > src.
template<typename T>
void relocate_backward_n(T* src, size_t n, T* dst) noexcept {
    if (n == 0) return;

    if constexpr (is_trivially_relocatable_v<T>) {
        if (src != dst) {
            __builtin_memmove(static_cast<void*>(dst),
                              static_cast<const void*>(src),
                              n * sizeof(T));
        }
    } else {
        // Process in reverse order for safety with overlapping ranges
        size_t i = n;
        try {
            while (i > 0) {
                --i;
                construct(dst + i, zstl::move(src[i]));
            }
            // Destroy sources
            for (i = 0; i < n; ++i) {
                destroy_at(src + i);
            }
        } catch (...) {
            // Destroy successfully-moved elements (those from i+1 to n-1)
            for (size_t j = i + 1; j < n; ++j) {
                destroy_at(dst + j);
            }
            throw;
        }
    }
}

} // namespace zstl
