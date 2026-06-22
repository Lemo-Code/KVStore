/**
 * @file    construct.h
 * @brief   Object construction and destruction utilities.
 * @author  lstl team
 * @date    2025
 *
 * Provides placement-new based construction and explicit destructor
 * invocation for memory management. Includes optimized no-op paths
 * for trivially-destructible types to avoid unnecessary work.
 *
 * @ingroup memory
 */

#pragma once

#include <cstring>
#include <new>
#include <iterator>

#include "type_traits.h"

namespace lstl {

// =========================================================================
// construct — placement new construction
// =========================================================================

/**
 * @name construct
 * @brief  Constructs an object at a given memory location using placement new.
 *
 * These functions do NOT allocate memory; they assume @p p points to
 * already-allocated but uninitialized storage.
 *
 * @param  p  Pointer to uninitialized memory.
 * @param  args...  Arguments forwarded to the constructor of T.
 *
 * @pre    @p p points to valid, uninitialized memory of at least sizeof(T) bytes.
 * @post   A fully-constructed T object exists at @p p.
 */

/// @{
template <typename T>
void construct(T* p) {
    ::new (static_cast<void*>(p)) T();
}

// Trivially copyable types: use placement new from value.
// Note: *p = value on uninitialized memory is UB, so we use
// placement new which correctly starts the object's lifetime.
template <typename T, typename U>
typename enable_if<is_trivially_copyable<T>::value>::type
construct(T* p, const U& value) {
    ::new (static_cast<void*>(p)) T(value);
}

template <typename T, typename U>
typename enable_if<!is_trivially_copyable<T>::value>::type
construct(T* p, const U& value) {
    ::new (static_cast<void*>(p)) T(value);
}

template <typename T, typename U1, typename U2>
void construct(T* p, const U1& a1, const U2& a2) {
    ::new (static_cast<void*>(p)) T(a1, a2);
}

template <typename T, typename U1, typename U2, typename U3>
void construct(T* p, const U1& a1, const U2& a2, const U3& a3) {
    ::new (static_cast<void*>(p)) T(a1, a2, a3);
}

#ifdef __cpp_variadic_templates
/** @brief  Variadic construction for compilers with C++11 variadic template support. */
template <typename T, typename... Args>
void construct(T* p, Args&&... args) {
    ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
}
#endif
/// @}

// =========================================================================
// destroy — explicit destructor invocation
// =========================================================================

/**
 * @brief  Destroys a single object by calling its destructor.
 *
 * Does NOT free memory; the caller is responsible for deallocation.
 *
 * @tparam T  The type of object to destroy.
 * @param  p  Pointer to the object.
 *
 * @post   The object at @p p has been destroyed.
 */
template <typename T>
void destroy(T* p) {
    p->~T();
}

/**
 * @brief  Destroys all objects in the range [first, last).
 *
 * Provides two overloads selected via enable_if:
 * - For trivially-destructible types: no-op (the loop is optimized away).
 * - For non-trivial types: iterates and calls destructor on each element.
 *
 * @tparam ForwardIterator  Iterator type (at least forward iterator).
 * @param  first  Beginning of the range to destroy.
 * @param  last   End of the range (one past the last element).
 *
 * @post   All objects in [first, last) have been destroyed.
 * @note   Memory is NOT freed; use the allocator to deallocate.
 */
template <typename ForwardIterator>
typename enable_if<
    has_trivial_destructor<
        typename std::iterator_traits<ForwardIterator>::value_type
    >::value
>::type
destroy(ForwardIterator /*first*/, ForwardIterator /*last*/) {
    // Trivially destructible types: no work needed.
}

template <typename ForwardIterator>
typename enable_if<
    !has_trivial_destructor<
        typename std::iterator_traits<ForwardIterator>::value_type
    >::value
>::type
destroy(ForwardIterator first, ForwardIterator last) {
    for (; first != last; ++first) {
        destroy(&*first);
    }
}

/**
 * @brief  Destroys objects in the pointer range [first, last).
 *
 * Unlike the iterator version, this takes raw pointers.
 *
 * @tparam T      Element type.
 * @param  first  Pointer to the first element.
 * @param  last   Pointer past the last element.
 */
template <typename T>
void destroy_range(T* first, T* last) {
    for (; first != last; ++first) {
        destroy(first);
    }
}

/**
 * @brief  Destroys exactly @p n objects starting at @p first.
 *
 * @tparam ForwardIterator  Iterator type.
 * @tparam Size             Integer type for the count.
 * @param  first  Iterator to the first element to destroy.
 * @param  n      Number of elements to destroy.
 * @return        Iterator past the last destroyed element (first + n).
 */
template <typename ForwardIterator, typename Size>
ForwardIterator destroy_n(ForwardIterator first, Size n) {
    for (; n > 0; --n, ++first) {
        destroy(&*first);
    }
    return first;
}

} // namespace lstl
