/**
 * @file    allocator.h
 * @brief   Standard-compliant allocator interface and allocator_traits.
 * @author  lstl team
 * @date    2025
 *
 * Provides:
 * - lstl::allocator<T>:  A standard-conforming allocator that forwards
 *                        to ::operator new / ::operator delete.
 * - lstl::allocator_traits<Alloc>:  Traits class for querying allocator
 *                        properties and performing allocation operations.
 * - lstl::simple_alloc<T, Alloc>:  A simplified allocator adapter that
 *                        binds a raw allocator to a specific type.
 *
 * The allocator interface follows the C++11 allocator model with
 * rebind support, making it compatible with all lstl containers.
 *
 * @ingroup memory
 */

#pragma once

#include <cstddef>
#include <new>
#include <limits>
#include <utility>

#include "type_traits.h"
#include "utility.h"
#include "construct.h"

namespace lstl {

// =========================================================================
// allocator
// =========================================================================

/**
 * @brief  Standard-compliant allocator using global operator new/delete.
 *
 * This is the default allocator for all lstl containers. It provides
 * the minimal interface required by the standard:
 * - allocate(n):  Allocates raw memory for n objects.
 * - deallocate(p, n):  Deallocates memory previously returned by allocate().
 * - construct/destroy:  Constructs/destroys objects in-place.
 * - rebind<U>:  Mechanism to obtain allocator<U> from allocator<T>.
 *
 * @tparam T  The type of objects this allocator manages.
 *
 * @note  This allocator is stateless — all instances are interchangeable.
 *        Comparison operators always return true.
 */
template <typename T>
class allocator {
public:
    // ---- Standard allocator typedefs ----
    typedef T               value_type;      ///< Element type.
    typedef T*              pointer;          ///< Pointer to element.
    typedef const T*        const_pointer;    ///< Const pointer to element.
    typedef T&              reference;        ///< Reference to element.
    typedef const T&        const_reference;  ///< Const reference to element.
    typedef size_t          size_type;        ///< Size type.
    typedef ptrdiff_t       difference_type;  ///< Pointer difference type.

    /**
     * @brief  Rebind mechanism — allows obtaining allocator<U> from allocator<T>.
     *
     * This is essential for node-based containers (list, map, etc.) that
     * internally allocate node types rather than the user-facing value_type.
     *
     * @tparam U  The type to rebind to.
     */
    template <typename U>
    struct rebind {
        typedef allocator<U> other;  ///< The rebound allocator type.
    };

    /// @name Construction
    /// @{
    allocator() noexcept {}
    allocator(const allocator&) noexcept {}
    template <typename U> allocator(const allocator<U>&) noexcept {}
    ~allocator() {}
    /// @}

    /**
     * @brief  Returns the address of a reference (even if operator& is overloaded).
     * @param  x  The object to get the address of.
     * @return    &x
     */
    pointer address(reference x) const noexcept { return &x; }

    /// @overload
    const_pointer address(const_reference x) const noexcept { return &x; }

    /**
     * @brief  Allocates raw memory for @p n objects of type T.
     *
     * The returned memory is uninitialized; the caller must construct
     * objects into it using construct() or placement new.
     *
     * @param  n     Number of objects worth of memory to allocate.
     * @param  hint  Optional locality hint (ignored by this allocator).
     * @return       Pointer to the allocated memory.
     *
     * @throws std::bad_alloc  If allocation fails or n > max_size().
     *
     * @post   The returned memory is valid for at least n * sizeof(T) bytes.
     * @note   Does NOT construct any objects.
     */
    pointer allocate(size_type n, const void* /*hint*/ = nullptr) {
        if (n > max_size()) {
            throw std::bad_alloc();
        }
        return static_cast<pointer>(::operator new(n * sizeof(T)));
    }

    /**
     * @brief  Deallocates memory previously obtained from allocate().
     *
     * @param  p  Pointer returned by a previous call to allocate().
     * @param  n  Number of objects that were allocated (ignored, but
     *            provided for interface compatibility).
     *
     * @pre    @p p was returned by allocate() on an equivalent allocator.
     * @pre    Any objects in the memory have been destroyed.
     */
    void deallocate(pointer p, size_type /*n*/) {
        ::operator delete(p);
    }

    /**
     * @brief  Returns the maximum number of elements that can be allocated.
     *
     * This is the theoretical maximum imposed by the size_type limits,
     * not the actual available system memory.
     *
     * @return  numeric_limits<size_type>::max() / sizeof(T)
     */
    size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    /**
     * @brief  Constructs an object of type U at address p.
     *
     * Uses placement new with perfect forwarding.
     *
     * @tparam U     The actual type to construct (may differ from T).
     * @tparam Args  Constructor argument types.
     * @param  p     Memory location.
     * @param  args  Arguments forwarded to U's constructor.
     */
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        lstl::construct(p, std::forward<Args>(args)...);
    }

    /**
     * @brief  Destroys an object of type U at address p.
     *
     * Calls the destructor but does NOT free memory.
     *
     * @tparam U  The type of the object to destroy.
     * @param  p  Pointer to the object.
     */
    template <typename U>
    void destroy(U* p) {
        lstl::destroy(p);
    }
};

/**
 * @brief  Void specialization of allocator (required by the standard).
 *
 * The void allocator cannot allocate or deallocate; it exists solely
 * to support rebinding from void to other types.
 */
template <>
class allocator<void> {
public:
    typedef void        value_type;
    typedef void*       pointer;
    typedef const void* const_pointer;

    template <typename U>
    struct rebind {
        typedef allocator<U> other;
    };
};

// ---- Allocator comparison ----

/// @brief  All instances of lstl::allocator compare equal (stateless).
template <typename T1, typename T2>
inline bool operator==(const allocator<T1>&, const allocator<T2>&) noexcept {
    return true;
}

/// @brief  All instances of lstl::allocator compare equal.
template <typename T1, typename T2>
inline bool operator!=(const allocator<T1>&, const allocator<T2>&) noexcept {
    return false;
}

// =========================================================================
// allocator_traits
// =========================================================================

/**
 * @brief  Traits class providing uniform access to allocator properties.
 *
 * allocator_traits is the standard mechanism for querying allocator
 * capabilities. It provides default implementations when the allocator
 * does not define certain types or methods.
 *
 * @tparam Alloc  The allocator type.
 *
 * @note  All lstl containers use allocator_traits to interact with
 *        allocators, ensuring compatibility with custom allocators
 *        that may not provide the full standard interface.
 */
template <typename Alloc>
struct allocator_traits {
    typedef Alloc                                    allocator_type;   ///< The allocator type.
    typedef typename Alloc::value_type               value_type;       ///< Element type.
    typedef typename Alloc::pointer                  pointer;          ///< Pointer type.
    typedef typename Alloc::const_pointer            const_pointer;    ///< Const pointer type.
    typedef typename Alloc::size_type                size_type;        ///< Size type.
    typedef typename Alloc::difference_type          difference_type;  ///< Difference type.

    /**
     * @brief  Rebinds the allocator to a different type.
     * @tparam U  The type to rebind to.
     */
    template <typename U>
    using rebind_alloc = typename Alloc::template rebind<U>::other;

    /**
     * @brief  Allocates memory via the allocator.
     * @param  a  The allocator instance.
     * @param  n  Number of objects.
     * @return    Pointer to allocated (uninitialized) memory.
     */
    static pointer allocate(Alloc& a, size_type n) {
        return a.allocate(n);
    }

    /// @overload  Allocate with hint.
    static pointer allocate(Alloc& a, size_type n, const_pointer hint) {
        return a.allocate(n, hint);
    }

    /**
     * @brief  Deallocates memory via the allocator.
     * @param  a  The allocator instance.
     * @param  p  Pointer to deallocate.
     * @param  n  Number of objects originally allocated.
     */
    static void deallocate(Alloc& a, pointer p, size_type n) {
        a.deallocate(p, n);
    }

    /**
     * @brief  Queries the maximum allocation size.
     * @param  a  The allocator instance.
     * @return    Maximum number of objects that can be allocated.
     */
    static size_type max_size(const Alloc& a) noexcept {
        return a.max_size();
    }

    /**
     * @brief  Constructs an object at p using the allocator.
     *
     * Uses lstl::construct for the actual construction; the allocator
     * parameter is provided for interface compatibility but is not
     * actually used by stateless allocators.
     *
     * @tparam T     Object type to construct.
     * @tparam Args  Constructor argument types.
     * @param  a     The allocator (unused by stateless allocators).
     * @param  p     Memory location.
     * @param  args  Constructor arguments.
     */
    template <typename T, typename... Args>
    static void construct(Alloc& /*a*/, T* p, Args&&... args) {
        lstl::construct(p, std::forward<Args>(args)...);
    }

    /**
     * @brief  Destroys an object at p using the allocator.
     * @tparam T  Object type.
     * @param  a  The allocator (unused by stateless allocators).
     * @param  p  Pointer to the object.
     */
    template <typename T>
    static void destroy(Alloc& /*a*/, T* p) {
        lstl::destroy(p);
    }
};

// =========================================================================
// simple_alloc
// =========================================================================

/**
 * @brief  Simplified allocator adapter for a specific type.
 *
 * Wraps a raw (non-typed) allocator (such as default_alloc or malloc_alloc)
 * and provides typed allocate/deallocate methods that work in terms of
 * object counts rather than byte counts.
 *
 * @tparam T      The type to allocate.
 * @tparam Alloc  The underlying raw allocator (must provide static
 *                allocate(size_t) and deallocate(void*, size_t)).
 *
 * Usage:
 * @code
 * typedef simple_alloc<MyNode, default_alloc> node_alloc;
 * MyNode* p = node_alloc::allocate(10);  // allocate 10 nodes
 * node_alloc::deallocate(p, 10);         // deallocate 10 nodes
 * @endcode
 */
template <typename T, typename Alloc>
class simple_alloc {
public:
    typedef T           value_type;       ///< Element type.
    typedef T*          pointer;          ///< Pointer type.
    typedef const T*    const_pointer;    ///< Const pointer type.
    typedef T&          reference;        ///< Reference type.
    typedef const T&    const_reference;  ///< Const reference type.
    typedef size_t      size_type;        ///< Size type.

    /**
     * @brief  Allocates memory for n objects.
     *
     * @param  n  Number of objects to allocate space for.
     * @return    Pointer to at least n * sizeof(T) bytes of uninitialized memory,
     *            or nullptr if n == 0.
     */
    static pointer allocate(size_type n) {
        return n != 0 ? static_cast<pointer>(Alloc::allocate(n * sizeof(T)))
                       : nullptr;
    }

    /**
     * @brief  Allocates memory for a single object.
     * @return  Pointer to at least sizeof(T) bytes of uninitialized memory.
     */
    static pointer allocate() {
        return static_cast<pointer>(Alloc::allocate(sizeof(T)));
    }

    /**
     * @brief  Deallocates memory for n objects.
     * @param  p  Pointer previously returned by allocate(n).
     * @param  n  The same count passed to allocate().
     */
    static void deallocate(pointer p, size_type n) {
        if (n != 0) {
            Alloc::deallocate(p, n * sizeof(T));
        }
    }

    /**
     * @brief  Deallocates memory for a single object.
     * @param  p  Pointer previously returned by allocate().
     */
    static void deallocate(pointer p) {
        Alloc::deallocate(p, sizeof(T));
    }

    /**
     * @brief  Reallocates memory for n objects (POD fast path).
     *         Forwards to Alloc::reallocate with byte counts.
     */
    static pointer reallocate(pointer p, size_type old_n, size_type new_n) {
        return static_cast<pointer>(
            Alloc::reallocate(p, old_n * sizeof(T), new_n * sizeof(T)));
    }
};

} // namespace lstl
