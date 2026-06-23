// zstl allocator — std-compatible allocator using MultiSizeClassPool,
// plus allocator_traits and std_alloc for standard new/delete.
#pragma once

#include <cstddef>
#include <new>
#include "zstl/memory/type_traits.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/construct.h"
#include "zstl/memory/pool.h"

namespace zstl {

// ============================================================
// default_alloc<T> — pool-based allocator (the default for
// all zstl containers)
// ============================================================

template<typename T>
class default_alloc {
public:
    using value_type      = T;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;

    // Allocator propagation traits
    using propagate_on_container_copy_assignment = false_type;
    using propagate_on_container_move_assignment = true_type;
    using propagate_on_container_swap            = false_type;
    using is_always_equal                       = true_type;

    // Rebind to a different value type
    template<typename U>
    struct rebind {
        using other = default_alloc<U>;
    };

    constexpr default_alloc() noexcept = default;
    constexpr default_alloc(const default_alloc&) noexcept = default;

    template<typename U>
    constexpr default_alloc(const default_alloc<U>&) noexcept {}

    // Allocate memory for n objects of type T
    [[nodiscard]] T* allocate(size_t n) {
        if (n == 0) return nullptr;
        size_t total = n * sizeof(T);

        // Oversized allocations bypass the pool
        if (__builtin_expect(total > kMaxPoolSize, 0)) {
            return static_cast<T*>(::operator new(total));
        }
        return static_cast<T*>(pool_malloc(total));
    }

    // Deallocate memory
    void deallocate(T* p, size_t n) noexcept {
        if (p == nullptr) return;
        size_t total = n * sizeof(T);

        if (__builtin_expect(total > kMaxPoolSize, 0)) {
            ::operator delete(p);
            return;
        }
        pool_free(static_cast<void*>(p), total);
    }

    // select_on_container_copy_construction
    // Since this allocator is stateless, just return a default instance
    default_alloc select_on_container_copy_construction() const noexcept {
        return default_alloc{};
    }
};

// Allocator comparison (always equal since stateless)
template<typename T, typename U>
constexpr bool operator==(const default_alloc<T>&,
                           const default_alloc<U>&) noexcept {
    return true;
}

template<typename T, typename U>
constexpr bool operator!=(const default_alloc<T>&,
                           const default_alloc<U>&) noexcept {
    return false;
}

// ============================================================
// std_alloc<T> — standard new/delete-based allocator
// (for use with types that need standard allocation, or for
// debugging/comparison purposes)
// ============================================================

template<typename T>
class std_alloc {
public:
    using value_type      = T;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;

    using propagate_on_container_copy_assignment = false_type;
    using propagate_on_container_move_assignment = false_type;
    using propagate_on_container_swap            = false_type;
    using is_always_equal                       = true_type;

    template<typename U>
    struct rebind {
        using other = std_alloc<U>;
    };

    constexpr std_alloc() noexcept = default;
    constexpr std_alloc(const std_alloc&) noexcept = default;
    template<typename U>
    constexpr std_alloc(const std_alloc<U>&) noexcept {}

    [[nodiscard]] T* allocate(size_t n) {
        if (n == 0) return nullptr;
        if (n > static_cast<size_t>(-1) / sizeof(T)) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, size_t n) noexcept {
        if (p == nullptr) return;
        ::operator delete(p, n * sizeof(T));
    }

    std_alloc select_on_container_copy_construction() const noexcept {
        return std_alloc{};
    }
};

template<typename T, typename U>
constexpr bool operator==(const std_alloc<T>&,
                           const std_alloc<U>&) noexcept {
    return true;
}

template<typename T, typename U>
constexpr bool operator!=(const std_alloc<T>&,
                           const std_alloc<U>&) noexcept {
    return false;
}

// ============================================================
// allocator_traits — uniform interface for all allocators
// ============================================================

template<typename Alloc>
struct allocator_traits {
    using allocator_type = Alloc;
    using value_type     = typename Alloc::value_type;
    using pointer        = value_type*;
    using const_pointer  = const value_type*;
    using void_pointer   = void*;
    using const_void_pointer = const void*;
    using size_type      = typename Alloc::size_type;
    using difference_type = typename Alloc::difference_type;

private:
    // Detection helpers for propagation traits
    // (must be declared before the using-aliases that reference them)
    template<typename A, typename = void>
    struct detect_propagate_copy { using type = false_type; };
    template<typename A>
    struct detect_propagate_copy<A, void_t<typename A::propagate_on_container_copy_assignment>> {
        using type = typename A::propagate_on_container_copy_assignment;
    };

    template<typename A, typename = void>
    struct detect_propagate_move { using type = false_type; };
    template<typename A>
    struct detect_propagate_move<A, void_t<typename A::propagate_on_container_move_assignment>> {
        using type = typename A::propagate_on_container_move_assignment;
    };

    template<typename A, typename = void>
    struct detect_propagate_swap { using type = false_type; };
    template<typename A>
    struct detect_propagate_swap<A, void_t<typename A::propagate_on_container_swap>> {
        using type = typename A::propagate_on_container_swap;
    };

    template<typename A, typename = void>
    struct detect_is_always_equal { using type = false_type; };
    template<typename A>
    struct detect_is_always_equal<A, void_t<typename A::is_always_equal>> {
        using type = typename A::is_always_equal;
    };

    // Rebind detection
    template<typename A, typename U>
    struct rebind_helper {
        using type = typename A::template rebind<U>::other;
    };
    // Fallback: if no rebind, construct Alloc<U> from Alloc<T>
    // (won't work for non-template allocators, but those are rare)

public:
    // Propagation traits: use Alloc's definition if present, else false_type
    using propagate_on_container_copy_assignment =
        typename detect_propagate_copy<Alloc>::type;
    using propagate_on_container_move_assignment =
        typename detect_propagate_move<Alloc>::type;
    using propagate_on_container_swap =
        typename detect_propagate_swap<Alloc>::type;
    using is_always_equal =
        typename detect_is_always_equal<Alloc>::type;

    // Rebind: Alloc<T> -> Alloc<U>
    template<typename U>
    using rebind_alloc = typename rebind_helper<Alloc, U>::type;

    // Allocate
    [[nodiscard]] static pointer allocate(Alloc& a, size_type n) {
        return a.allocate(n);
    }

    // Deallocate
    static void deallocate(Alloc& a, pointer p, size_type n) noexcept {
        a.deallocate(p, n);
    }

    // Construct (in-place)
    template<typename T, typename... Args>
    static void construct(Alloc& /*a*/, T* p, Args&&... args) {
        zstl::construct(p, zstl::forward<Args>(args)...);
    }

    // Destroy
    template<typename T>
    static void destroy(Alloc& /*a*/, T* p) noexcept {
        zstl::destroy_at(p);
    }

    // Max size — use SFINAE to detect if allocator has max_size()
private:
    template<typename A>
    static auto max_size_impl(int, const A& a) noexcept
        -> decltype(a.max_size()) { return a.max_size(); }
    template<typename A>
    static size_type max_size_impl(long, const A&) noexcept {
        return static_cast<size_type>(-1) / sizeof(value_type);
    }
public:
    static size_type max_size(const Alloc& a) noexcept {
        return max_size_impl<Alloc>(0, a);
    }

    // select_on_container_copy_construction
    static Alloc select_on_container_copy_construction(const Alloc& a) {
        return a.select_on_container_copy_construction();
    }
};

// Specialization for void value_type (needed for rebinding)
template<>
class default_alloc<void> {
public:
    using value_type = void;
    using size_type  = size_t;

    template<typename U>
    struct rebind {
        using other = default_alloc<U>;
    };
};

template<>
class std_alloc<void> {
public:
    using value_type = void;
    using size_type  = size_t;

    template<typename U>
    struct rebind {
        using other = std_alloc<U>;
    };
};

// ============================================================
// Convenience aliases
// ============================================================

template<typename T>
using allocator = default_alloc<T>;

} // namespace zstl
