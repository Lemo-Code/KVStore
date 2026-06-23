// zero utility macros — assertions, branch hints, code annotations
#pragma once

#include <cstdio>
#include <cstdlib>

// ============================================================
// Assertions
// ============================================================

#define ZERO_ASSERT(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ::zero::detail::assert_fail(#cond, __FILE__, __LINE__, __func__);  \
        }                                                                      \
    } while (0)

#define ZERO_ASSERT_MSG(cond, msg)                                             \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ::zero::detail::assert_fail(msg, __FILE__, __LINE__, __func__);    \
        }                                                                      \
    } while (0)

#define ZERO_UNREACHABLE()                                                     \
    do {                                                                       \
        ::zero::detail::assert_fail("unreachable reached",                     \
                                    __FILE__, __LINE__, __func__);             \
        __builtin_unreachable();                                               \
    } while (0)

// ============================================================
// Branch prediction hints
// ============================================================

#if defined(__GNUC__) || defined(__clang__)
    #define ZERO_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define ZERO_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define ZERO_LIKELY(x)   (x)
    #define ZERO_UNLIKELY(x) (x)
#endif

// ============================================================
// Function attributes
// ============================================================

#if defined(__GNUC__) || defined(__clang__)
    #define ZERO_NOINLINE        __attribute__((noinline))
    #define ZERO_ALWAYS_INLINE   __attribute__((always_inline)) inline
    #define ZERO_USED            __attribute__((used))
    #define ZERO_WARN_UNUSED     __attribute__((warn_unused_result))
#else
    #define ZERO_NOINLINE
    #define ZERO_ALWAYS_INLINE   inline
    #define ZERO_USED
    #define ZERO_WARN_UNUSED
#endif

// ============================================================
// Pointer aliasing
// ============================================================

#if defined(__cplusplus) && defined(__GNUC__)
    #define ZERO_RESTRICT __restrict__
#elif defined(_MSC_VER)
    #define ZERO_RESTRICT __restrict
#else
    #define ZERO_RESTRICT
#endif

// ============================================================
// Stringification
// ============================================================

#define ZERO_STRINGIFY_IMPL(x) #x
#define ZERO_STRINGIFY(x)      ZERO_STRINGIFY_IMPL(x)

// ============================================================
// Class-level copy / move disable macros
// ============================================================

#define ZERO_DISABLE_COPY(ClassName)                       \
    ClassName(const ClassName&) = delete;                  \
    ClassName& operator=(const ClassName&) = delete

#define ZERO_DISABLE_MOVE(ClassName)                       \
    ClassName(ClassName&&) noexcept = delete;              \
    ClassName& operator=(ClassName&&) noexcept = delete

#define ZERO_DISABLE_COPY_MOVE(ClassName)                  \
    ZERO_DISABLE_COPY(ClassName);                          \
    ZERO_DISABLE_MOVE(ClassName)

#define ZERO_DEFAULT_MOVE(ClassName)                       \
    ClassName(ClassName&&) noexcept = default;             \
    ClassName& operator=(ClassName&&) noexcept = default

// ============================================================
// Unused variable suppression
// ============================================================

#define ZERO_UNUSED(x) (void)(x)

// ============================================================
// Cache-line alignment (64 bytes on x86-64 and ARM64)
// ============================================================

#define ZERO_CACHE_LINE_SIZE 64
#define ZERO_ALIGNED_CACHE alignas(ZERO_CACHE_LINE_SIZE)

// ============================================================
// Compiler attribute for likely-switch-default
// ============================================================

#if defined(__GNUC__) || defined(__clang__)
    // Use [[fallthrough]] from C++17
    #define ZERO_FALLTHROUGH [[fallthrough]]
#else
    #define ZERO_FALLTHROUGH
#endif

// ============================================================
// Misc
// ============================================================

// Number of elements in a static C array
#define ZERO_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

namespace zero {
namespace detail {

[[noreturn]] inline void assert_fail(const char* msg, const char* file,
                                      int line, const char* func) {
    fprintf(stderr, "ZERO ASSERTION FAILED: %s\n"
                    "  File:     %s\n"
                    "  Line:     %d\n"
                    "  Function: %s\n",
            msg, file, line, func);
    fflush(stderr);
    std::abort();
}

} // namespace detail

// Global library panic (abort with message)
[[noreturn]] inline void panic(const char* msg) {
    fprintf(stderr, "ZERO PANIC: %s\n", msg);
    fflush(stderr);
    std::abort();
}

} // namespace zero
