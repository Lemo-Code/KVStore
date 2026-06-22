/**
 * @file    type_traits.h
 * @brief   Type trait utilities for compile-time type introspection.
 * @author  lstl team
 * @date    2025
 *
 * Provides type detection metafunctions used throughout lstl for
 * optimization of memory operations (e.g., dispatching between
 * trivial and non-trivial copy/destroy paths).
 *
 * All traits are C++14 compatible and follow the standard integral_constant
 * pattern for compatibility with <type_traits> SFINAE idioms.
 *
 * @ingroup memory
 */

#pragma once

#include <type_traits>

namespace lstl {

/**
 * @brief  Determines whether a type is POD (Plain Old Data).
 *
 * In C++14, POD is approximated as: trivial + standard layout.
 * POD types can be safely manipulated via memcpy/memmove instead
 * of element-wise copy construction.
 *
 * @tparam T  The type to examine.
 *
 * @code
 * static_assert(is_pod<int>::value, "int is POD");
 * static_assert(!is_pod<std::string>::value, "string is not POD");
 * @endcode
 */
template <typename T>
struct is_pod : std::integral_constant<bool,
    std::is_trivial<T>::value && std::is_standard_layout<T>::value> {};

/**
 * @brief  Checks if T has a trivial default constructor.
 * @tparam T  The type to examine.
 *
 * Trivial default constructors perform no action; objects can be
 * allocated without calling construct().
 */
template <typename T>
struct has_trivial_default_constructor
    : std::is_trivially_default_constructible<T> {};

/**
 * @brief  Checks if T has a trivial copy constructor.
 * @tparam T  The type to examine.
 *
 * Types with trivial copy constructors can be safely memcpy'd
 * during uninitialized_copy operations.
 */
template <typename T>
struct has_trivial_copy_constructor
    : std::is_trivially_copy_constructible<T> {};

/**
 * @brief  Checks if T has a trivial copy assignment operator.
 * @tparam T  The type to examine.
 */
template <typename T>
struct has_trivial_copy_assignment
    : std::is_trivially_copy_assignable<T> {};

/**
 * @brief  Checks if T has a trivial destructor.
 * @tparam T  The type to examine.
 *
 * Types with trivial destructors do not need destroy() calls;
 * their memory can simply be deallocated.
 */
template <typename T>
struct has_trivial_destructor
    : std::is_trivially_destructible<T> {};

/**
 * @brief  Compile-time conditional type selection.
 *
 * Selects type T if B is true, otherwise selects type F.
 * Equivalent to std::conditional.
 *
 * @tparam B  Boolean condition.
 * @tparam T  Type selected when B == true.
 * @tparam F  Type selected when B == false.
 */
template <bool B, typename T, typename F>
struct conditional {
    typedef T type;  ///< The selected type.
};

template <typename T, typename F>
struct conditional<false, T, F> {
    typedef F type;  ///< The selected type.
};

/// @brief  Alias template for conditional<B, T, F>::type.
template <bool B, typename T, typename F>
using conditional_t = typename conditional<B, T, F>::type;

/**
 * @brief  SFINAE enabler — yields type only when B is true.
 *
 * Used to constrain template overloads. When B is false,
 * enable_if has no ::type member, causing substitution failure.
 *
 * @tparam B  Enable condition.
 * @tparam T  The type to expose (defaults to void).
 */
template <bool B, typename T = void>
struct enable_if {};

template <typename T>
struct enable_if<true, T> {
    typedef T type;  ///< Exposed type when condition is true.
};

/// @brief  Alias template for enable_if<B, T>::type.
template <bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;

/**
 * @brief  Compile-time type equality test.
 *
 * Inherits from true_type when T and U are the same type,
 * otherwise inherits from false_type.
 *
 * @tparam T  First type.
 * @tparam U  Second type.
 */
template <typename T, typename U>
struct is_same : std::false_type {};

template <typename T>
struct is_same<T, T> : std::true_type {};

// ---------------------------------------------------------------------------
// CV-qualifier and reference removal
// ---------------------------------------------------------------------------

/// @brief Removes top-level const qualifier.
template <typename T> struct remove_const          { typedef T type; };
template <typename T> struct remove_const<const T> { typedef T type; };

/// @brief Removes top-level volatile qualifier.
template <typename T> struct remove_volatile             { typedef T type; };
template <typename T> struct remove_volatile<volatile T> { typedef T type; };

/// @brief Removes both const and volatile qualifiers.
template <typename T>
struct remove_cv {
    typedef typename remove_volatile<typename remove_const<T>::type>::type type;
};

/// @brief Removes reference (both lvalue and rvalue).
template <typename T> struct remove_reference       { typedef T type; };
template <typename T> struct remove_reference<T&>   { typedef T type; };
template <typename T> struct remove_reference<T&&>  { typedef T type; };

/// @brief Adds an lvalue reference to T.
template <typename T>
struct add_lvalue_reference {
    typedef T& type;
};

/**
 * @brief  Decays a type by removing reference and cv-qualifiers.
 *
 * Produces the "by-value" type suitable for storage.
 *
 * @tparam T  The type to decay.
 */
template <typename T>
struct decay {
    typedef typename remove_cv<typename remove_reference<T>::type>::type type;
};

/**
 * @brief  Checks if T is a scalar type.
 *
 * Scalar types include: arithmetic, enum, pointer, member pointer,
 * and nullptr_t. Scalar types are always POD.
 *
 * @tparam T  The type to examine.
 */
template <typename T>
struct is_scalar : std::integral_constant<bool,
    std::is_arithmetic<T>::value ||
    std::is_enum<T>::value ||
    std::is_pointer<T>::value ||
    std::is_member_pointer<T>::value ||
    std::is_null_pointer<T>::value> {};

/// @brief  Checks if T is an integral type (delegates to std::is_integral).
template <typename T>
struct is_integral : std::is_integral<T> {};

/**
 * @brief  Checks if T is trivially copyable (safe for memcpy to uninitialized memory).
 *
 * Uses std::is_trivially_copyable (C++11). This is stricter than
 * has_trivial_copy_constructor — it also requires a trivial destructor
 * and no virtual functions, ensuring memcpy is truly safe.
 */
template <typename T>
struct is_trivially_copyable : std::is_trivially_copyable<T> {};

// Re-export commonly-used standard traits for convenience.
using std::true_type;
using std::false_type;
using std::integral_constant;

} // namespace lstl
