// zstl atomic — std::atomic wrapper with full API surface
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace zstl {

// ============================================================
// memory_order enum (mirrors std::memory_order)
// ============================================================
using memory_order = std::memory_order;

inline constexpr memory_order memory_order_relaxed = std::memory_order_relaxed;
inline constexpr memory_order memory_order_consume = std::memory_order_consume;
inline constexpr memory_order memory_order_acquire = std::memory_order_acquire;
inline constexpr memory_order memory_order_release = std::memory_order_release;
inline constexpr memory_order memory_order_acq_rel  = std::memory_order_acq_rel;
inline constexpr memory_order memory_order_seq_cst  = std::memory_order_seq_cst;

// ============================================================
// atomic<T> — thin wrapper around std::atomic
// ============================================================
template<typename T>
class atomic {
    std::atomic<T> impl_;

public:
    using value_type = T;

    // Default constructor (not atomic-initialized!)
    atomic() noexcept = default;

    // Initialize with value
    constexpr atomic(T desired) noexcept : impl_(desired) {}

    // Non-copyable, non-movable
    atomic(const atomic&) = delete;
    atomic& operator=(const atomic&) = delete;
    atomic(atomic&&) = delete;
    atomic& operator=(atomic&&) = delete;

    // ---- store ----
    void store(T desired, memory_order order = memory_order_seq_cst) noexcept {
        impl_.store(desired, order);
    }

    // ---- load ----
    T load(memory_order order = memory_order_seq_cst) const noexcept {
        return impl_.load(order);
    }

    // ---- exchange ----
    T exchange(T desired, memory_order order = memory_order_seq_cst) noexcept {
        return impl_.exchange(desired, order);
    }

    // ---- compare_exchange_weak ----
    bool compare_exchange_weak(T& expected, T desired,
                                memory_order success,
                                memory_order failure) noexcept {
        return impl_.compare_exchange_weak(expected, desired, success, failure);
    }

    bool compare_exchange_weak(T& expected, T desired,
                                memory_order order = memory_order_seq_cst) noexcept {
        return impl_.compare_exchange_weak(expected, desired, order);
    }

    // ---- compare_exchange_strong ----
    bool compare_exchange_strong(T& expected, T desired,
                                  memory_order success,
                                  memory_order failure) noexcept {
        return impl_.compare_exchange_strong(expected, desired, success, failure);
    }

    bool compare_exchange_strong(T& expected, T desired,
                                  memory_order order = memory_order_seq_cst) noexcept {
        return impl_.compare_exchange_strong(expected, desired, order);
    }

    // ---- operator T (implicit conversion) ----
    operator T() const noexcept {
        return load();
    }

    // ---- operator= ----
    T operator=(T desired) noexcept {
        store(desired);
        return desired;
    }

    // ---- is_lock_free ----
    bool is_lock_free() const noexcept {
        return impl_.is_lock_free();
    }

    // ---- is_always_lock_free ----
    static constexpr bool is_always_lock_free = std::atomic<T>::is_always_lock_free;

    // Access to underlying std::atomic for interop
    std::atomic<T>& _std_atomic() noexcept { return impl_; }
    const std::atomic<T>& _std_atomic() const noexcept { return impl_; }
};

// ============================================================
// Partial specialization: atomic<Integral> adds fetch_* ops
// ============================================================
#define ZSTL_ATOMIC_INTEGRAL_SPEC(I)                                         \
template<>                                                                   \
class atomic<I> {                                                            \
    std::atomic<I> impl_;                                                    \
public:                                                                      \
    using value_type = I;                                                    \
    using difference_type = I;                                               \
                                                                             \
    atomic() noexcept = default;                                             \
    constexpr atomic(I desired) noexcept : impl_(desired) {}                 \
                                                                             \
    atomic(const atomic&) = delete;                                          \
    atomic& operator=(const atomic&) = delete;                               \
    atomic(atomic&&) = delete;                                               \
    atomic& operator=(atomic&&) = delete;                                    \
                                                                             \
    void store(I desired, memory_order order = memory_order_seq_cst) noexcept { \
        impl_.store(desired, order);                                         \
    }                                                                        \
                                                                             \
    I load(memory_order order = memory_order_seq_cst) const noexcept {       \
        return impl_.load(order);                                            \
    }                                                                        \
                                                                             \
    I exchange(I desired, memory_order order = memory_order_seq_cst) noexcept { \
        return impl_.exchange(desired, order);                               \
    }                                                                        \
                                                                             \
    bool compare_exchange_weak(I& expected, I desired,                       \
                                memory_order success,                        \
                                memory_order failure) noexcept {             \
        return impl_.compare_exchange_weak(expected, desired, success, failure); \
    }                                                                        \
                                                                             \
    bool compare_exchange_weak(I& expected, I desired,                       \
                                memory_order order = memory_order_seq_cst) noexcept { \
        return impl_.compare_exchange_weak(expected, desired, order);        \
    }                                                                        \
                                                                             \
    bool compare_exchange_strong(I& expected, I desired,                     \
                                  memory_order success,                      \
                                  memory_order failure) noexcept {           \
        return impl_.compare_exchange_strong(expected, desired, success, failure); \
    }                                                                        \
                                                                             \
    bool compare_exchange_strong(I& expected, I desired,                     \
                                  memory_order order = memory_order_seq_cst) noexcept { \
        return impl_.compare_exchange_strong(expected, desired, order);      \
    }                                                                        \
                                                                             \
    I fetch_add(I arg, memory_order order = memory_order_seq_cst) noexcept { \
        return impl_.fetch_add(arg, order);                                  \
    }                                                                        \
                                                                             \
    I fetch_sub(I arg, memory_order order = memory_order_seq_cst) noexcept { \
        return impl_.fetch_sub(arg, order);                                  \
    }                                                                        \
                                                                             \
    I fetch_and(I arg, memory_order order = memory_order_seq_cst) noexcept { \
        return impl_.fetch_and(arg, order);                                  \
    }                                                                        \
                                                                             \
    I fetch_or(I arg, memory_order order = memory_order_seq_cst) noexcept {  \
        return impl_.fetch_or(arg, order);                                   \
    }                                                                        \
                                                                             \
    I fetch_xor(I arg, memory_order order = memory_order_seq_cst) noexcept { \
        return impl_.fetch_xor(arg, order);                                  \
    }                                                                        \
                                                                             \
    I operator++() noexcept { return fetch_add(1) + 1; }                     \
    I operator++(int) noexcept { return fetch_add(1); }                      \
    I operator--() noexcept { return fetch_sub(1) - 1; }                     \
    I operator--(int) noexcept { return fetch_sub(1); }                      \
                                                                             \
    I operator+=(I arg) noexcept { return fetch_add(arg) + arg; }            \
    I operator-=(I arg) noexcept { return fetch_sub(arg) - arg; }            \
    I operator&=(I arg) noexcept { return fetch_and(arg) & arg; }            \
    I operator|=(I arg) noexcept { return fetch_or(arg) | arg; }             \
    I operator^=(I arg) noexcept { return fetch_xor(arg) ^ arg; }            \
                                                                             \
    operator I() const noexcept { return load(); }                           \
    I operator=(I desired) noexcept { store(desired); return desired; }      \
                                                                             \
    bool is_lock_free() const noexcept { return impl_.is_lock_free(); }      \
    static constexpr bool is_always_lock_free = std::atomic<I>::is_always_lock_free; \
                                                                             \
    std::atomic<I>& _std_atomic() noexcept { return impl_; }                 \
    const std::atomic<I>& _std_atomic() const noexcept { return impl_; }     \
};

ZSTL_ATOMIC_INTEGRAL_SPEC(char)
ZSTL_ATOMIC_INTEGRAL_SPEC(signed char)
ZSTL_ATOMIC_INTEGRAL_SPEC(unsigned char)
// Note: int8_t/uint8_t typically alias signed/unsigned char on most
// platforms. The signed char / unsigned char specializations above
// already cover them; these are omitted to avoid duplicate definitions.
ZSTL_ATOMIC_INTEGRAL_SPEC(short)
ZSTL_ATOMIC_INTEGRAL_SPEC(unsigned short)
ZSTL_ATOMIC_INTEGRAL_SPEC(int)
ZSTL_ATOMIC_INTEGRAL_SPEC(unsigned int)
ZSTL_ATOMIC_INTEGRAL_SPEC(long)
ZSTL_ATOMIC_INTEGRAL_SPEC(unsigned long)
ZSTL_ATOMIC_INTEGRAL_SPEC(long long)
ZSTL_ATOMIC_INTEGRAL_SPEC(unsigned long long)
ZSTL_ATOMIC_INTEGRAL_SPEC(wchar_t)
ZSTL_ATOMIC_INTEGRAL_SPEC(char16_t)
ZSTL_ATOMIC_INTEGRAL_SPEC(char32_t)
// int8_t/uint8_t omitted -- covered by signed/unsigned char specializations above
//
// int16_t/uint16_t/int32_t/uint32_t/int64_t/uint64_t are also omitted
// because they are typedefs for the short/int/long/long long types
// already specialized above.  The convenience typedefs below (e.g.
// atomic_int32_t) will resolve to the correct specialization via
// the underlying type.

#undef ZSTL_ATOMIC_INTEGRAL_SPEC

// ============================================================
// Partial specialization: atomic<T*> (pointer arithmetic)
// ============================================================
template<typename T>
class atomic<T*> {
    std::atomic<T*> impl_;

public:
    using value_type = T*;
    using difference_type = ptrdiff_t;

    atomic() noexcept = default;
    constexpr atomic(T* desired) noexcept : impl_(desired) {}

    atomic(const atomic&) = delete;
    atomic& operator=(const atomic&) = delete;
    atomic(atomic&&) = delete;
    atomic& operator=(atomic&&) = delete;

    void store(T* desired, memory_order order = memory_order_seq_cst) noexcept {
        impl_.store(desired, order);
    }

    T* load(memory_order order = memory_order_seq_cst) const noexcept {
        return impl_.load(order);
    }

    T* exchange(T* desired, memory_order order = memory_order_seq_cst) noexcept {
        return impl_.exchange(desired, order);
    }

    bool compare_exchange_weak(T*& expected, T* desired,
                                memory_order success,
                                memory_order failure) noexcept {
        return impl_.compare_exchange_weak(expected, desired, success, failure);
    }

    bool compare_exchange_weak(T*& expected, T* desired,
                                memory_order order = memory_order_seq_cst) noexcept {
        return impl_.compare_exchange_weak(expected, desired, order);
    }

    bool compare_exchange_strong(T*& expected, T* desired,
                                  memory_order success,
                                  memory_order failure) noexcept {
        return impl_.compare_exchange_strong(expected, desired, success, failure);
    }

    bool compare_exchange_strong(T*& expected, T* desired,
                                  memory_order order = memory_order_seq_cst) noexcept {
        return impl_.compare_exchange_strong(expected, desired, order);
    }

    T* fetch_add(difference_type arg, memory_order order = memory_order_seq_cst) noexcept {
        return impl_.fetch_add(arg, order);
    }

    T* fetch_sub(difference_type arg, memory_order order = memory_order_seq_cst) noexcept {
        return impl_.fetch_sub(arg, order);
    }

    T* operator++() noexcept { return fetch_add(1) + 1; }
    T* operator++(int) noexcept { return fetch_add(1); }
    T* operator--() noexcept { return fetch_sub(1) - 1; }
    T* operator--(int) noexcept { return fetch_sub(1); }

    T* operator+=(difference_type arg) noexcept { return fetch_add(arg) + arg; }
    T* operator-=(difference_type arg) noexcept { return fetch_sub(arg) - arg; }

    operator T*() const noexcept { return load(); }
    T* operator=(T* desired) noexcept { store(desired); return desired; }

    bool is_lock_free() const noexcept { return impl_.is_lock_free(); }
    static constexpr bool is_always_lock_free = std::atomic<T*>::is_always_lock_free;

    std::atomic<T*>& _std_atomic() noexcept { return impl_; }
    const std::atomic<T*>& _std_atomic() const noexcept { return impl_; }
};

// ============================================================
// atomic_flag — lock-free boolean flag
// ============================================================
class atomic_flag {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    atomic_flag() noexcept = default;
    atomic_flag(const atomic_flag&) = delete;
    atomic_flag& operator=(const atomic_flag&) = delete;

    bool test_and_set(memory_order order = memory_order_seq_cst) noexcept {
        return flag_.test_and_set(order);
    }

    void clear(memory_order order = memory_order_seq_cst) noexcept {
        flag_.clear(order);
    }

    // test() is C++20; omit for C++17 compatibility
    // bool test(memory_order order = memory_order_seq_cst) const noexcept {
    //     return flag_.test(order);
    // }

    std::atomic_flag& _std_atomic_flag() noexcept { return flag_; }
};

// ============================================================
// Convenience typedefs
// ============================================================

using atomic_bool     = atomic<bool>;
using atomic_char     = atomic<char>;
using atomic_schar    = atomic<signed char>;
using atomic_uchar    = atomic<unsigned char>;
using atomic_short    = atomic<short>;
using atomic_ushort   = atomic<unsigned short>;
using atomic_int      = atomic<int>;
using atomic_uint     = atomic<unsigned int>;
using atomic_long     = atomic<long>;
using atomic_ulong    = atomic<unsigned long>;
using atomic_llong    = atomic<long long>;
using atomic_ullong   = atomic<unsigned long long>;
using atomic_wchar_t  = atomic<wchar_t>;
using atomic_char16_t = atomic<char16_t>;
using atomic_char32_t = atomic<char32_t>;
using atomic_int8_t   = atomic<int8_t>;
using atomic_uint8_t  = atomic<uint8_t>;
using atomic_int16_t  = atomic<int16_t>;
using atomic_uint16_t = atomic<uint16_t>;
using atomic_int32_t  = atomic<int32_t>;
using atomic_uint32_t = atomic<uint32_t>;
using atomic_int64_t  = atomic<int64_t>;
using atomic_uint64_t = atomic<uint64_t>;
using atomic_intptr_t = atomic<intptr_t>;
using atomic_uintptr_t= atomic<uintptr_t>;
using atomic_size_t   = atomic<size_t>;
using atomic_ptrdiff_t= atomic<ptrdiff_t>;

// ============================================================
// Free functions
// ============================================================

template<typename T>
T atomic_load(const atomic<T>* a) noexcept {
    return a->load();
}

template<typename T>
T atomic_load(const atomic<T>* a, memory_order order) noexcept {
    return a->load(order);
}

template<typename T>
void atomic_store(atomic<T>* a, T desired) noexcept {
    a->store(desired);
}

template<typename T>
void atomic_store(atomic<T>* a, T desired, memory_order order) noexcept {
    a->store(desired, order);
}

template<typename T>
T atomic_exchange(atomic<T>* a, T desired) noexcept {
    return a->exchange(desired);
}

template<typename T>
T atomic_exchange(atomic<T>* a, T desired, memory_order order) noexcept {
    return a->exchange(desired, order);
}

template<typename T>
bool atomic_compare_exchange_weak(atomic<T>* a, T& expected, T desired) noexcept {
    return a->compare_exchange_weak(expected, desired);
}

template<typename T>
bool atomic_compare_exchange_weak(atomic<T>* a, T& expected, T desired,
                                   memory_order success, memory_order failure) noexcept {
    return a->compare_exchange_weak(expected, desired, success, failure);
}

template<typename T>
bool atomic_compare_exchange_strong(atomic<T>* a, T& expected, T desired) noexcept {
    return a->compare_exchange_strong(expected, desired);
}

template<typename T>
bool atomic_compare_exchange_strong(atomic<T>* a, T& expected, T desired,
                                     memory_order success, memory_order failure) noexcept {
    return a->compare_exchange_strong(expected, desired, success, failure);
}

template<typename Integral>
Integral atomic_fetch_add(atomic<Integral>* a, Integral arg) noexcept {
    return a->fetch_add(arg);
}

template<typename Integral>
Integral atomic_fetch_add(atomic<Integral>* a, Integral arg, memory_order order) noexcept {
    return a->fetch_add(arg, order);
}

template<typename Integral>
Integral atomic_fetch_sub(atomic<Integral>* a, Integral arg) noexcept {
    return a->fetch_sub(arg);
}

template<typename Integral>
Integral atomic_fetch_sub(atomic<Integral>* a, Integral arg, memory_order order) noexcept {
    return a->fetch_sub(arg, order);
}

// Overloads for pointer types
template<typename T>
T* atomic_fetch_add(atomic<T*>* a, ptrdiff_t arg) noexcept {
    return a->fetch_add(arg);
}

template<typename T>
T* atomic_fetch_add(atomic<T*>* a, ptrdiff_t arg, memory_order order) noexcept {
    return a->fetch_add(arg, order);
}

template<typename T>
T* atomic_fetch_sub(atomic<T*>* a, ptrdiff_t arg) noexcept {
    return a->fetch_sub(arg);
}

template<typename T>
T* atomic_fetch_sub(atomic<T*>* a, ptrdiff_t arg, memory_order order) noexcept {
    return a->fetch_sub(arg, order);
}

} // namespace zstl
