/**
 * @file    temporary_buffer.h
 * @brief   Temporary buffer utilities for short-lived allocations.
 * @author  lstl team
 * @date    2025
 *
 * Provides get_temporary_buffer() and return_temporary_buffer() for
 * obtaining short-lived uninitialized memory, plus an RAII guard class
 * (temporary_buffer_guard) for exception-safe usage.
 *
 * These are primarily used internally by algorithms that need scratch
 * space (e.g., stable_sort, inplace_merge). For production use, prefer
 * the pool allocators (default_alloc) or std::vector.
 *
 * @ingroup memory
 */

#pragma once

#include <cstddef>
#include <new>
#include <limits>

#include "utility.h"
#include "construct.h"
#include "alloc.h"

namespace lstl {

/**
 * @brief  Allocates uninitialized memory for up to @p n objects of type T.
 *
 * Attempts to allocate space for @p n objects. The actual number
 * allocated may be less than @p n (or even zero) if memory is tight.
 *
 * @tparam T  The type of objects to allocate space for.
 * @param  n  Desired number of objects.
 * @return    A pair containing:
 *            - first:  pointer to the allocated memory (or nullptr).
 *            - second: actual number of objects' worth of memory allocated
 *                      (may be less than n, or 0).
 *
 * @note  The returned memory is UNINITIALIZED. The caller must construct
 *        objects into it before use.
 * @note  Use return_temporary_buffer() to free the memory.
 *
 * @see return_temporary_buffer
 * @see temporary_buffer_guard
 */
template <typename T>
pair<T*, ptrdiff_t> get_temporary_buffer(ptrdiff_t n) {
    if (n <= 0) {
        return pair<T*, ptrdiff_t>(nullptr, 0);
    }

    size_t bytes = static_cast<size_t>(n) * sizeof(T);
    T* p = static_cast<T*>(malloc_alloc::allocate(bytes));

    return pair<T*, ptrdiff_t>(p, n);
}

/**
 * @brief  Returns memory allocated by get_temporary_buffer().
 *
 * @tparam T  The element type.
 * @param  p  Pointer previously returned by get_temporary_buffer().
 *            May be nullptr (in which case this is a no-op).
 *
 * @pre    @p p was allocated by a matching call to get_temporary_buffer<T>().
 */
template <typename T>
void return_temporary_buffer(T* p) {
    malloc_alloc::deallocate(p, 0);
}

/**
 * @brief  RAII guard for temporary buffers.
 *
 * Automatically calls return_temporary_buffer() on destruction,
 * ensuring exception-safe cleanup. Non-copyable but movable.
 *
 * @tparam T  The element type.
 *
 * Usage:
 * @code
 * {
 *     temporary_buffer_guard<int> guard(100);
 *     int* buf = guard.data();
 *     // use buf...
 * } // automatically freed here
 * @endcode
 */
template <typename T>
class temporary_buffer_guard {
public:
    /**
     * @brief  Allocates a temporary buffer for up to @p n objects.
     *
     * @param  n  Desired number of objects.
     */
    explicit temporary_buffer_guard(ptrdiff_t n)
        : buffer_(get_temporary_buffer<T>(n)) {}

    /**
     * @brief  Destructor — frees the temporary buffer.
     */
    ~temporary_buffer_guard() {
        if (buffer_.first) {
            return_temporary_buffer(buffer_.first);
        }
    }

    /**
     * @brief  Returns a pointer to the allocated memory.
     * @return  Pointer to the buffer, or nullptr if allocation failed.
     */
    T* data() const { return buffer_.first; }

    /**
     * @brief  Returns the number of objects' worth of memory allocated.
     * @return  The allocated count (may be 0).
     */
    ptrdiff_t size() const { return buffer_.second; }

    // Non-copyable
    temporary_buffer_guard(const temporary_buffer_guard&) = delete;
    temporary_buffer_guard& operator=(const temporary_buffer_guard&) = delete;

    /**
     * @brief  Move constructor — transfers ownership.
     *
     * @param  other  The source guard. After the move, other.data()
     *                returns nullptr.
     */
    temporary_buffer_guard(temporary_buffer_guard&& other) noexcept
        : buffer_(other.buffer_) {
        other.buffer_.first = nullptr;
        other.buffer_.second = 0;
    }

private:
    pair<T*, ptrdiff_t> buffer_;  ///< The managed buffer.
};

} // namespace lstl
