// zstl type_traits — complete type trait suite implemented from scratch
// No dependence on std <type_traits> or other std headers for traits.
// Uses compiler intrinsics where necessary; SFINAE fallbacks otherwise.
#pragma once

#include <cstddef>   // for size_t, nullptr_t
#include <cstdint>   // for intmax_t, uintmax_t

namespace zstl {

// ============================================================
// Part 1: integral_constant, bool_constant, true_type, false_type
// ============================================================

template<typename T, T v>
struct integral_constant {
    static constexpr T value = v;
    using value_type = T;
    using type       = integral_constant<T, v>;
    constexpr operator T() const noexcept { return v; }
    constexpr T operator()() const noexcept { return v; }
};

template<bool B>
using bool_constant = integral_constant<bool, B>;

using true_type  = bool_constant<true>;
using false_type = bool_constant<false>;

// ============================================================
// Part 2: forward declarations (needed by later traits)
// ============================================================

// remove_cv — forward (defined fully in Part 14)
template<typename T> struct remove_cv { using type = T; };
template<typename T> struct remove_cv<T const> { using type = T; };
template<typename T> struct remove_cv<T volatile> { using type = T; };
template<typename T> struct remove_cv<T const volatile> { using type = T; };
template<typename T> using remove_cv_t = typename remove_cv<T>::type;

// conditional — used by conjunction/disjunction
template<bool B, typename T, typename F>
struct conditional { using type = T; };
template<typename T, typename F>
struct conditional<false, T, F> { using type = F; };
template<bool B, typename T, typename F>
using conditional_t = typename conditional<B, T, F>::type;

// ============================================================
// Part 3: conjunction, disjunction, negation (C++17 logical ops)
// ============================================================

template<typename...> struct conjunction : true_type {};
template<typename B1> struct conjunction<B1> : B1 {};
template<typename B1, typename... Bn>
struct conjunction<B1, Bn...>
    : conditional_t<static_cast<bool>(B1::value), conjunction<Bn...>, B1> {};

template<typename...> struct disjunction : false_type {};
template<typename B1> struct disjunction<B1> : B1 {};
template<typename B1, typename... Bn>
struct disjunction<B1, Bn...>
    : conditional_t<static_cast<bool>(B1::value), B1, disjunction<Bn...>> {};

template<typename B>
struct negation : bool_constant<!static_cast<bool>(B::value)> {};

template<typename... Bn>
inline constexpr bool conjunction_v = conjunction<Bn...>::value;
template<typename... Bn>
inline constexpr bool disjunction_v = disjunction<Bn...>::value;
template<typename B>
inline constexpr bool negation_v = negation<B>::value;

// ============================================================
// Part 4: enable_if
// ============================================================

template<bool B, typename T = void>
struct enable_if { using type = T; };
template<typename T>
struct enable_if<false, T> {};

template<bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;

// ============================================================
// Part 5: void_t — the detection idiom
// ============================================================

template<typename...>
using void_t = void;

// ============================================================
// Part 6: Type modifications — references (needed early)
// ============================================================

template<typename T> struct remove_reference { using type = T; };
template<typename T> struct remove_reference<T&> { using type = T; };
template<typename T> struct remove_reference<T&&> { using type = T; };
template<typename T>
using remove_reference_t = typename remove_reference<T>::type;

template<typename T>
struct add_lvalue_reference { using type = T&; };
template<typename T>
struct add_lvalue_reference<T&> { using type = T&; };
template<> struct add_lvalue_reference<void> { using type = void; };
template<> struct add_lvalue_reference<void const> { using type = void const; };
template<> struct add_lvalue_reference<void volatile> { using type = void volatile; };
template<> struct add_lvalue_reference<void const volatile> { using type = void const volatile; };
template<typename T>
using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;

template<typename T>
struct add_rvalue_reference { using type = T&&; };
template<typename T>
struct add_rvalue_reference<T&> { using type = T&; };
template<> struct add_rvalue_reference<void> { using type = void; };
template<> struct add_rvalue_reference<void const> { using type = void const; };
template<> struct add_rvalue_reference<void volatile> { using type = void volatile; };
template<> struct add_rvalue_reference<void const volatile> { using type = void const volatile; };
template<typename T>
using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

// ============================================================
// Part 7: declval — needed for SFINAE expressions
// ============================================================

template<typename T>
add_rvalue_reference_t<T> declval() noexcept;

// ============================================================
// Part 8: Type modifications — const/volatile (more)
// ============================================================

template<typename T> struct remove_const { using type = T; };
template<typename T> struct remove_const<T const> { using type = T; };
template<typename T>
using remove_const_t = typename remove_const<T>::type;

template<typename T> struct remove_volatile { using type = T; };
template<typename T> struct remove_volatile<T volatile> { using type = T; };
template<typename T>
using remove_volatile_t = typename remove_volatile<T>::type;

template<typename T> struct add_const { using type = T const; };
template<typename T>
using add_const_t = typename add_const<T>::type;

template<typename T> struct add_volatile { using type = T volatile; };
template<typename T>
using add_volatile_t = typename add_volatile<T>::type;

template<typename T> struct add_cv { using type = T const volatile; };
template<typename T>
using add_cv_t = typename add_cv<T>::type;

// ============================================================
// Part 9: Primary type categories
// ============================================================

// --- is_void ---
template<typename T> struct is_void_impl : false_type {};
template<> struct is_void_impl<void> : true_type {};
template<> struct is_void_impl<void const> : true_type {};
template<> struct is_void_impl<void volatile> : true_type {};
template<> struct is_void_impl<void const volatile> : true_type {};
template<typename T>
struct is_void : is_void_impl<remove_cv_t<T>> {};
template<typename T>
inline constexpr bool is_void_v = is_void<T>::value;

// --- is_null_pointer ---
template<typename T> struct is_null_pointer_impl : false_type {};
template<> struct is_null_pointer_impl<std::nullptr_t> : true_type {};
template<> struct is_null_pointer_impl<std::nullptr_t const> : true_type {};
template<> struct is_null_pointer_impl<std::nullptr_t volatile> : true_type {};
template<> struct is_null_pointer_impl<std::nullptr_t const volatile> : true_type {};
template<typename T>
struct is_null_pointer : is_null_pointer_impl<remove_cv_t<T>> {};
template<typename T>
inline constexpr bool is_null_pointer_v = is_null_pointer<T>::value;

// --- is_integral ---
template<typename T> struct is_integral_impl : false_type {};
template<> struct is_integral_impl<bool>               : true_type {};
template<> struct is_integral_impl<char>               : true_type {};
template<> struct is_integral_impl<signed char>        : true_type {};
template<> struct is_integral_impl<unsigned char>      : true_type {};
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
template<> struct is_integral_impl<char8_t>            : true_type {};
#endif
template<> struct is_integral_impl<char16_t>           : true_type {};
template<> struct is_integral_impl<char32_t>           : true_type {};
template<> struct is_integral_impl<wchar_t>            : true_type {};
template<> struct is_integral_impl<short>              : true_type {};
template<> struct is_integral_impl<unsigned short>     : true_type {};
template<> struct is_integral_impl<int>                : true_type {};
template<> struct is_integral_impl<unsigned int>       : true_type {};
template<> struct is_integral_impl<long>               : true_type {};
template<> struct is_integral_impl<unsigned long>      : true_type {};
template<> struct is_integral_impl<long long>          : true_type {};
template<> struct is_integral_impl<unsigned long long> : true_type {};
template<typename T>
struct is_integral : is_integral_impl<remove_cv_t<T>> {};
template<typename T>
inline constexpr bool is_integral_v = is_integral<T>::value;

// --- is_floating_point ---
template<typename T> struct is_floating_point_impl : false_type {};
template<> struct is_floating_point_impl<float>       : true_type {};
template<> struct is_floating_point_impl<double>      : true_type {};
template<> struct is_floating_point_impl<long double> : true_type {};
template<typename T>
struct is_floating_point : is_floating_point_impl<remove_cv_t<T>> {};
template<typename T>
inline constexpr bool is_floating_point_v = is_floating_point<T>::value;

// --- is_array ---
template<typename T> struct is_array : false_type {};
template<typename T> struct is_array<T[]> : true_type {};
template<typename T, std::size_t N> struct is_array<T[N]> : true_type {};
template<typename T>
inline constexpr bool is_array_v = is_array<T>::value;

// --- is_pointer ---
template<typename T> struct is_pointer_impl : false_type {};
template<typename T> struct is_pointer_impl<T*> : true_type {};
template<typename T>
struct is_pointer : is_pointer_impl<remove_cv_t<T>> {};
template<typename T>
inline constexpr bool is_pointer_v = is_pointer<T>::value;

// --- is_lvalue_reference ---
template<typename T> struct is_lvalue_reference : false_type {};
template<typename T> struct is_lvalue_reference<T&> : true_type {};
template<typename T>
inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

// --- is_rvalue_reference ---
template<typename T> struct is_rvalue_reference : false_type {};
template<typename T> struct is_rvalue_reference<T&&> : true_type {};
template<typename T>
inline constexpr bool is_rvalue_reference_v = is_rvalue_reference<T>::value;

// --- is_reference ---
template<typename T> struct is_reference : false_type {};
template<typename T> struct is_reference<T&>  : true_type {};
template<typename T> struct is_reference<T&&> : true_type {};
template<typename T>
inline constexpr bool is_reference_v = is_reference<T>::value;

// --- is_const (needed by is_function below) ---
template<typename T> struct is_const_impl : false_type {};
template<typename T> struct is_const_impl<T const> : true_type {};
template<typename T>
struct is_const : is_const_impl<T> {};
template<typename T>
inline constexpr bool is_const_v = is_const<T>::value;

// --- is_function (detects non-reference, non-const-volatile-qualified non-void types that are not objects) ---
// A function type has the property that const-qualifying it is a no-op.
// For any non-function type T, T const is distinct from T.
// For function types, adding const has no effect.
template<typename T>
struct is_function : bool_constant<!is_const_v<T const> && !is_reference_v<T>> {};
template<typename T>
inline constexpr bool is_function_v = is_function<T>::value;

// --- is_member_object_pointer ---
template<typename T> struct is_member_object_pointer_impl : false_type {};
template<typename T, typename C>
struct is_member_object_pointer_impl<T C::*> : bool_constant<!is_function_v<T>> {};
template<typename T>
struct is_member_object_pointer : is_member_object_pointer_impl<remove_cv_t<T>> {};
template<typename T>
inline constexpr bool is_member_object_pointer_v = is_member_object_pointer<T>::value;

// --- is_member_function_pointer ---
template<typename T> struct is_member_function_pointer_impl : false_type {};
template<typename T, typename C>
struct is_member_function_pointer_impl<T C::*> : bool_constant<is_function_v<T>> {};
template<typename T>
struct is_member_function_pointer : is_member_function_pointer_impl<remove_cv_t<T>> {};
template<typename T>
inline constexpr bool is_member_function_pointer_v = is_member_function_pointer<T>::value;

// --- is_member_pointer ---
template<typename T>
struct is_member_pointer : bool_constant<
    is_member_object_pointer_v<T> || is_member_function_pointer_v<T>> {};
template<typename T>
inline constexpr bool is_member_pointer_v = is_member_pointer<T>::value;

// --- is_enum ---
// Requires compiler builtin for correctness.
#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct is_enum : bool_constant<__is_enum(T)> {};
#else
// Fallback: negative test — exclude all other primary categories
template<typename T>
struct is_enum : bool_constant<
    !is_void_v<T> && !is_null_pointer_v<T> && !is_integral_v<T> &&
    !is_floating_point_v<T> && !is_array_v<T> && !is_pointer_v<T> &&
    !is_reference_v<T> && !is_member_pointer_v<T> && !is_union_v<T> &&
    !is_class_v<T> && !is_function_v<T>> {};
#endif
template<typename T>
inline constexpr bool is_enum_v = is_enum<T>::value;

// --- is_union ---
#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct is_union : bool_constant<__is_union(T)> {};
#else
template<typename T>
struct is_union : false_type {}; // Cannot detect without compiler builtin
#endif
template<typename T>
inline constexpr bool is_union_v = is_union<T>::value;

// --- is_class ---
// Detect by checking for pointer-to-member: only classes/structs support it.
// Need is_union_v first (above), so this comes after.
namespace detail {
template<typename T>
auto test_is_class(int T::*) -> true_type;
template<typename>
auto test_is_class(...) -> false_type;
}
template<typename T>
struct is_class : bool_constant<!is_union_v<T> &&
    decltype(detail::test_is_class<T>(nullptr))::value> {};
template<typename T>
inline constexpr bool is_class_v = is_class<T>::value;

// ============================================================
// Part 10: Composite type categories
// ============================================================

template<typename T>
struct is_arithmetic : bool_constant<is_integral_v<T> || is_floating_point_v<T>> {};
template<typename T>
inline constexpr bool is_arithmetic_v = is_arithmetic<T>::value;

template<typename T>
struct is_fundamental : bool_constant<is_void_v<T> || is_null_pointer_v<T> ||
    is_arithmetic_v<T>> {};
template<typename T>
inline constexpr bool is_fundamental_v = is_fundamental<T>::value;

template<typename T>
struct is_scalar : bool_constant<is_arithmetic_v<T> || is_enum_v<T> ||
    is_pointer_v<T> || is_member_pointer_v<T> || is_null_pointer_v<T>> {};
template<typename T>
inline constexpr bool is_scalar_v = is_scalar<T>::value;

template<typename T>
struct is_object : bool_constant<!is_function_v<T> && !is_reference_v<T> &&
    !is_void_v<T>> {};
template<typename T>
inline constexpr bool is_object_v = is_object<T>::value;

template<typename T>
struct is_compound : bool_constant<!is_fundamental_v<T>> {};
template<typename T>
inline constexpr bool is_compound_v = is_compound<T>::value;

// ============================================================
// Part 11: Type properties — const/volatile detection
// ============================================================

template<typename T> struct is_volatile_impl : false_type {};
template<typename T> struct is_volatile_impl<T volatile> : true_type {};
template<typename T>
struct is_volatile : is_volatile_impl<T> {};
template<typename T>
inline constexpr bool is_volatile_v = is_volatile<T>::value;

// --- is_trivial ---
#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct is_trivial : bool_constant<__is_trivial(T)> {};
#else
template<typename T>
struct is_trivial : bool_constant<__has_trivial_constructor(T) &&
    __has_trivial_copy(T) && __has_trivial_assign(T) &&
    __has_trivial_destructor(T)> {};
#endif
template<typename T>
inline constexpr bool is_trivial_v = is_trivial<T>::value;

// --- is_trivially_copyable ---
#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct is_trivially_copyable : bool_constant<__is_trivially_copyable(T)> {};
#else
template<typename T>
struct is_trivially_copyable : is_trivial<T> {};
#endif
template<typename T>
inline constexpr bool is_trivially_copyable_v = is_trivially_copyable<T>::value;

// --- is_standard_layout ---
#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct is_standard_layout : bool_constant<__is_standard_layout(T)> {};
#else
template<typename T>
struct is_standard_layout : bool_constant<is_scalar_v<T>> {};
#endif
template<typename T>
inline constexpr bool is_standard_layout_v = is_standard_layout<T>::value;

// --- is_pod (pre-C++20) ---
#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct is_pod : bool_constant<__is_pod(T)> {};
#else
template<typename T>
struct is_pod : bool_constant<is_standard_layout_v<T> && is_trivial_v<T>> {};
#endif
template<typename T>
inline constexpr bool is_pod_v = is_pod<T>::value;

// --- is_literal_type ---
#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct is_literal_type : bool_constant<__is_literal_type(T)> {};
#else
template<typename T>
struct is_literal_type : true_type {};
#endif
template<typename T>
inline constexpr bool is_literal_type_v = is_literal_type<T>::value;

// --- is_empty ---
#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct is_empty : bool_constant<__is_empty(T)> {};
#else
namespace detail {
template<typename T, bool = is_class_v<T>>
struct is_empty_impl : false_type {};
template<typename T>
struct is_empty_impl<T, true> : bool_constant<sizeof(T) == 1> {};
}
template<typename T>
struct is_empty : detail::is_empty_impl<T> {};
#endif
template<typename T>
inline constexpr bool is_empty_v = is_empty<T>::value;

// --- is_polymorphic ---
#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct is_polymorphic : bool_constant<__is_polymorphic(T)> {};
#else
template<typename T>
struct is_polymorphic : false_type {};
#endif
template<typename T>
inline constexpr bool is_polymorphic_v = is_polymorphic<T>::value;

// --- is_abstract ---
#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct is_abstract : bool_constant<__is_abstract(T)> {};
#else
namespace detail {
template<typename T>
auto test_abstract(T) -> false_type;
auto test_abstract(...) -> true_type;
}
template<typename T>
struct is_abstract : decltype(detail::test_abstract(declval<T>())) {};
#endif
template<typename T>
inline constexpr bool is_abstract_v = is_abstract<T>::value;

// --- is_final ---
#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct is_final : bool_constant<__is_final(T)> {};
#else
template<typename T>
struct is_final : false_type {};
#endif
template<typename T>
inline constexpr bool is_final_v = is_final<T>::value;

// --- is_signed ---
template<typename T> struct is_signed_impl : false_type {};
template<> struct is_signed_impl<char> {
    static constexpr bool value = static_cast<char>(-1) < 0;
};
// Note: char signedness is implementation-defined, handled via value check above
template<> struct is_signed_impl<signed char> : true_type {};
template<> struct is_signed_impl<short> : true_type {};
template<> struct is_signed_impl<int> : true_type {};
template<> struct is_signed_impl<long> : true_type {};
template<> struct is_signed_impl<long long> : true_type {};
template<> struct is_signed_impl<float> : true_type {};
template<> struct is_signed_impl<double> : true_type {};
template<> struct is_signed_impl<long double> : true_type {};
template<typename T>
struct is_signed : is_signed_impl<remove_cv_t<T>> {};
template<typename T>
inline constexpr bool is_signed_v = is_signed<T>::value;

// --- is_unsigned ---
template<typename T>
struct is_unsigned : bool_constant<is_arithmetic_v<T> && !is_signed_v<T>> {};
template<typename T>
inline constexpr bool is_unsigned_v = is_unsigned<T>::value;

// --- has_virtual_destructor ---
#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct has_virtual_destructor : bool_constant<__has_virtual_destructor(T)> {};
#else
template<typename T>
struct has_virtual_destructor : false_type {};
#endif
template<typename T>
inline constexpr bool has_virtual_destructor_v = has_virtual_destructor<T>::value;

// ============================================================
// Part 12: Type relationships
// ============================================================

// --- is_same ---
template<typename T, typename U>
struct is_same : false_type {};
template<typename T>
struct is_same<T, T> : true_type {};
template<typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;

// --- is_base_of ---
#if defined(__GNUC__) || defined(__clang__)
template<typename Base, typename Derived>
struct is_base_of : bool_constant<__is_base_of(Base, Derived)> {};
#else
namespace detail {
template<typename Base>
auto test_base_of(Base*) -> true_type;
auto test_base_of(...) -> false_type;
}
template<typename Base, typename Derived>
struct is_base_of : bool_constant<
    is_class_v<Base> && is_class_v<Derived> &&
    decltype(detail::test_base_of(static_cast<Derived*>(nullptr)))::value> {};
#endif
template<typename Base, typename Derived>
inline constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;

// --- is_convertible ---
namespace detail {
template<typename From, typename To>
auto test_convertible(int) -> decltype(
    static_cast<To(*)(From)>(nullptr), true_type{});
template<typename, typename>
auto test_convertible(...) -> false_type;
}
template<typename From, typename To>
struct is_convertible : bool_constant<
    (is_void_v<From> && is_void_v<To>) ||
    decltype(detail::test_convertible<From, To>(0))::value> {};
template<typename From, typename To>
inline constexpr bool is_convertible_v = is_convertible<From, To>::value;

// ============================================================
// Part 13: Supported operations — construction
// These fundamentally require compiler builtins for correctness.
// ============================================================

#if defined(__GNUC__) || defined(__clang__)

template<typename T, typename... Args>
struct is_constructible
    : bool_constant<__is_constructible(T, Args...)> {};
template<typename T, typename... Args>
inline constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

template<typename T, typename... Args>
struct is_trivially_constructible
    : bool_constant<__is_trivially_constructible(T, Args...)> {};
template<typename T, typename... Args>
inline constexpr bool is_trivially_constructible_v =
    is_trivially_constructible<T, Args...>::value;

template<typename T, typename... Args>
struct is_nothrow_constructible
    : bool_constant<__is_nothrow_constructible(T, Args...)> {};
template<typename T, typename... Args>
inline constexpr bool is_nothrow_constructible_v =
    is_nothrow_constructible<T, Args...>::value;

#else
// SFINAE-based fallbacks
namespace detail {
template<typename T, typename... Args>
auto test_constructible(int) -> decltype(T(declval<Args>()...), true_type{});
template<typename...>
auto test_constructible(...) -> false_type;
}
template<typename T, typename... Args>
struct is_constructible
    : decltype(detail::test_constructible<T, Args...>(0)) {};
template<typename T, typename... Args>
inline constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

template<typename T, typename... Args>
struct is_trivially_constructible
    : bool_constant<is_trivial_v<T> && is_constructible_v<T, Args...>> {};
template<typename T, typename... Args>
inline constexpr bool is_trivially_constructible_v =
    is_trivially_constructible<T, Args...>::value;

template<typename T, typename... Args>
struct is_nothrow_constructible
    : bool_constant<is_constructible_v<T, Args...> &&
          noexcept(T(declval<Args>()...))> {};
template<typename T, typename... Args>
inline constexpr bool is_nothrow_constructible_v =
    is_nothrow_constructible<T, Args...>::value;
#endif

// --- Convenience aliases ---
template<typename T>
using is_default_constructible = is_constructible<T>;
template<typename T>
inline constexpr bool is_default_constructible_v = is_default_constructible<T>::value;

template<typename T>
using is_trivially_default_constructible = is_trivially_constructible<T>;
template<typename T>
inline constexpr bool is_trivially_default_constructible_v =
    is_trivially_default_constructible<T>::value;

template<typename T>
using is_nothrow_default_constructible = is_nothrow_constructible<T>;
template<typename T>
inline constexpr bool is_nothrow_default_constructible_v =
    is_nothrow_default_constructible<T>::value;

template<typename T>
struct is_copy_constructible
    : is_constructible<T, add_lvalue_reference_t<add_const_t<T>>> {};
template<typename T>
inline constexpr bool is_copy_constructible_v = is_copy_constructible<T>::value;

template<typename T>
struct is_trivially_copy_constructible
    : is_trivially_constructible<T, add_lvalue_reference_t<add_const_t<T>>> {};
template<typename T>
inline constexpr bool is_trivially_copy_constructible_v =
    is_trivially_copy_constructible<T>::value;

template<typename T>
struct is_nothrow_copy_constructible
    : is_nothrow_constructible<T, add_lvalue_reference_t<add_const_t<T>>> {};
template<typename T>
inline constexpr bool is_nothrow_copy_constructible_v =
    is_nothrow_copy_constructible<T>::value;

template<typename T>
struct is_move_constructible
    : is_constructible<T, add_rvalue_reference_t<T>> {};
template<typename T>
inline constexpr bool is_move_constructible_v = is_move_constructible<T>::value;

template<typename T>
struct is_trivially_move_constructible
    : is_trivially_constructible<T, add_rvalue_reference_t<T>> {};
template<typename T>
inline constexpr bool is_trivially_move_constructible_v =
    is_trivially_move_constructible<T>::value;

template<typename T>
struct is_nothrow_move_constructible
    : is_nothrow_constructible<T, add_rvalue_reference_t<T>> {};
template<typename T>
inline constexpr bool is_nothrow_move_constructible_v =
    is_nothrow_move_constructible<T>::value;

// ============================================================
// Part 14: Supported operations — assignment
// ============================================================

#if defined(__GNUC__) || defined(__clang__)

template<typename T, typename U>
struct is_assignable : bool_constant<__is_assignable(T, U)> {};
template<typename T, typename U>
inline constexpr bool is_assignable_v = is_assignable<T, U>::value;

template<typename T, typename U>
struct is_trivially_assignable : bool_constant<__is_trivially_assignable(T, U)> {};
template<typename T, typename U>
inline constexpr bool is_trivially_assignable_v =
    is_trivially_assignable<T, U>::value;

template<typename T, typename U>
struct is_nothrow_assignable : bool_constant<__is_nothrow_assignable(T, U)> {};
template<typename T, typename U>
inline constexpr bool is_nothrow_assignable_v = is_nothrow_assignable<T, U>::value;

#else
namespace detail {
template<typename T, typename U>
auto test_assignable(int) -> decltype(declval<T&>() = declval<U>(), true_type{});
template<typename...>
auto test_assignable(...) -> false_type;
}
template<typename T, typename U>
struct is_assignable : decltype(detail::test_assignable<T, U>(0)) {};
template<typename T, typename U>
inline constexpr bool is_assignable_v = is_assignable<T, U>::value;

template<typename T, typename U>
struct is_trivially_assignable
    : bool_constant<is_trivial_v<T> && is_assignable_v<T, U>> {};
template<typename T, typename U>
inline constexpr bool is_trivially_assignable_v =
    is_trivially_assignable<T, U>::value;

template<typename T, typename U>
struct is_nothrow_assignable
    : bool_constant<is_assignable_v<T, U> &&
          noexcept(declval<T&>() = declval<U>())> {};
template<typename T, typename U>
inline constexpr bool is_nothrow_assignable_v = is_nothrow_assignable<T, U>::value;
#endif

template<typename T>
struct is_copy_assignable
    : is_assignable<add_lvalue_reference_t<T>, add_lvalue_reference_t<add_const_t<T>>> {};
template<typename T>
inline constexpr bool is_copy_assignable_v = is_copy_assignable<T>::value;

template<typename T>
struct is_move_assignable
    : is_assignable<add_lvalue_reference_t<T>, add_rvalue_reference_t<T>> {};
template<typename T>
inline constexpr bool is_move_assignable_v = is_move_assignable<T>::value;

template<typename T>
struct is_trivially_copy_assignable
    : is_trivially_assignable<add_lvalue_reference_t<T>,
                               add_lvalue_reference_t<add_const_t<T>>> {};
template<typename T>
inline constexpr bool is_trivially_copy_assignable_v =
    is_trivially_copy_assignable<T>::value;

template<typename T>
struct is_trivially_move_assignable
    : is_trivially_assignable<add_lvalue_reference_t<T>,
                               add_rvalue_reference_t<T>> {};
template<typename T>
inline constexpr bool is_trivially_move_assignable_v =
    is_trivially_move_assignable<T>::value;

template<typename T>
struct is_nothrow_copy_assignable
    : is_nothrow_assignable<add_lvalue_reference_t<T>,
                             add_lvalue_reference_t<add_const_t<T>>> {};
template<typename T>
inline constexpr bool is_nothrow_copy_assignable_v =
    is_nothrow_copy_assignable<T>::value;

template<typename T>
struct is_nothrow_move_assignable
    : is_nothrow_assignable<add_lvalue_reference_t<T>,
                             add_rvalue_reference_t<T>> {};
template<typename T>
inline constexpr bool is_nothrow_move_assignable_v =
    is_nothrow_move_assignable<T>::value;

// ============================================================
// Forward declaration (definition is in Part 23 below)
template<typename T>
struct rank;

// Part 15: Supported operations — destruction
// ============================================================

// is_destructible: Most complete types are destructible except
// reference types, void, and arrays of unknown bound.
template<typename T>
struct is_destructible : bool_constant<is_reference_v<T> ||
    (is_object_v<T> && (!is_array_v<T> || (rank<T>::value > 0)))> {};
template<typename T>
inline constexpr bool is_destructible_v = is_destructible<T>::value;

#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct is_trivially_destructible : bool_constant<__has_trivial_destructor(T)> {};
#else
template<typename T>
struct is_trivially_destructible
    : bool_constant<is_reference_v<T> || is_trivial_v<T>> {};
#endif
template<typename T>
inline constexpr bool is_trivially_destructible_v =
    is_trivially_destructible<T>::value;

template<typename T>
struct is_nothrow_destructible : bool_constant<
    is_reference_v<T> || is_trivially_destructible_v<T> ||
    noexcept(declval<T&>().~T())> {};
template<typename T>
inline constexpr bool is_nothrow_destructible_v =
    is_nothrow_destructible<T>::value;

// ============================================================
// Part 16: is_swappable
// ============================================================

// Forward-declare swap (defined in utility.h, included later by zstl.h)
// Required for two-phase name lookup in the traits below.
template<typename T>
constexpr void swap(T& a, T& b)
    noexcept(is_nothrow_move_constructible_v<T> &&
             is_nothrow_move_assignable_v<T>);

// is_swappable — use portable SFINAE implementation
// (GCC/Clang builtins __is_swappable/__is_nothrow_swappable are not
//  reliably available across all versions; using the standard idiom)
namespace detail {
template<typename T>
auto test_swappable(int) -> decltype(
    zstl::swap(declval<T&>(), declval<T&>()), true_type{});
template<typename>
auto test_swappable(...) -> false_type;
}
template<typename T>
struct is_swappable : decltype(detail::test_swappable<T>(0)) {};
template<typename T>
inline constexpr bool is_swappable_v = is_swappable<T>::value;

template<typename T>
struct is_nothrow_swappable
    : bool_constant<is_swappable_v<T> &&
          noexcept(zstl::swap(declval<T&>(), declval<T&>()))> {};
template<typename T>
inline constexpr bool is_nothrow_swappable_v = is_nothrow_swappable<T>::value;

// ============================================================
// Part 17: Type modifications — pointers
// ============================================================

template<typename T> struct remove_pointer { using type = T; };
template<typename T> struct remove_pointer<T*> { using type = T; };
template<typename T> struct remove_pointer<T* const> { using type = T; };
template<typename T> struct remove_pointer<T* volatile> { using type = T; };
template<typename T> struct remove_pointer<T* const volatile> { using type = T; };
template<typename T>
using remove_pointer_t = typename remove_pointer<T>::type;

template<typename T>
struct add_pointer {
    using type = remove_reference_t<T>*;
};
template<typename T>
using add_pointer_t = typename add_pointer<T>::type;

// ============================================================
// Part 18: Type modifications — signedness
// ============================================================

template<typename T> struct make_signed { using type = T; };
template<> struct make_signed<char> {
    using type = conditional_t<static_cast<char>(-1) < 0, char, signed char>;
};
template<> struct make_signed<signed char>       { using type = signed char; };
template<> struct make_signed<unsigned char>     { using type = signed char; };
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
template<> struct make_signed<char8_t>           { using type = signed char; };
#endif
template<> struct make_signed<char16_t>          { using type = short; };
template<> struct make_signed<char32_t>          { using type = int; };
template<> struct make_signed<wchar_t> {
    using type = conditional_t<sizeof(wchar_t) == 2, short, int>;
};
template<> struct make_signed<short>              { using type = short; };
template<> struct make_signed<unsigned short>     { using type = short; };
template<> struct make_signed<int>                { using type = int; };
template<> struct make_signed<unsigned int>       { using type = int; };
template<> struct make_signed<long>               { using type = long; };
template<> struct make_signed<unsigned long>      { using type = long; };
template<> struct make_signed<long long>          { using type = long long; };
template<> struct make_signed<unsigned long long> { using type = long long; };
template<typename T>
using make_signed_t = typename make_signed<T>::type;

template<typename T> struct make_unsigned { using type = T; };
template<> struct make_unsigned<char> {
    using type = conditional_t<static_cast<char>(-1) < 0, unsigned char, char>;
};
template<> struct make_unsigned<signed char>       { using type = unsigned char; };
template<> struct make_unsigned<unsigned char>     { using type = unsigned char; };
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
template<> struct make_unsigned<char8_t>           { using type = unsigned char; };
#endif
template<> struct make_unsigned<char16_t>          { using type = unsigned short; };
template<> struct make_unsigned<char32_t>          { using type = unsigned int; };
template<> struct make_unsigned<wchar_t> {
    using type = conditional_t<sizeof(wchar_t) == 2,
                               unsigned short, unsigned int>;
};
template<> struct make_unsigned<short>              { using type = unsigned short; };
template<> struct make_unsigned<unsigned short>     { using type = unsigned short; };
template<> struct make_unsigned<int>                { using type = unsigned int; };
template<> struct make_unsigned<unsigned int>       { using type = unsigned int; };
template<> struct make_unsigned<long>               { using type = unsigned long; };
template<> struct make_unsigned<unsigned long>      { using type = unsigned long; };
template<> struct make_unsigned<long long>          { using type = unsigned long long; };
template<> struct make_unsigned<unsigned long long> { using type = unsigned long long; };
template<typename T>
using make_unsigned_t = typename make_unsigned<T>::type;

// ============================================================
// Part 19: Type modifications — array extent removal
// ============================================================

template<typename T> struct remove_extent { using type = T; };
template<typename T> struct remove_extent<T[]> { using type = T; };
template<typename T, std::size_t N> struct remove_extent<T[N]> { using type = T; };
template<typename T>
using remove_extent_t = typename remove_extent<T>::type;

template<typename T> struct remove_all_extents { using type = T; };
template<typename T> struct remove_all_extents<T[]> {
    using type = typename remove_all_extents<T>::type;
};
template<typename T, std::size_t N> struct remove_all_extents<T[N]> {
    using type = typename remove_all_extents<T>::type;
};
template<typename T>
using remove_all_extents_t = typename remove_all_extents<T>::type;

// ============================================================
// Part 20: decay
// ============================================================

template<typename T>
struct decay {
private:
    using U = remove_reference_t<T>;
public:
    using type = conditional_t<
        is_array_v<U>,
        remove_extent_t<U>*,
        conditional_t<is_function_v<U>, add_pointer_t<U>, remove_cv_t<U>>>;
};
template<typename T>
using decay_t = typename decay<T>::type;

// ============================================================
// Part 21: common_type
// ============================================================

template<typename... T> struct common_type;

template<typename T>
struct common_type<T> { using type = decay_t<T>; };

template<typename T, typename U>
struct common_type<T, U> {
    using type = decay_t<decltype(true ? declval<T>() : declval<U>())>;
};

template<typename T, typename U, typename... Rest>
struct common_type<T, U, Rest...>
    : common_type<typename common_type<T, U>::type, Rest...> {};

template<typename... T>
using common_type_t = typename common_type<T...>::type;

// ============================================================
// Part 22: underlying_type
// ============================================================

#if defined(__GNUC__) || defined(__clang__)
template<typename T>
struct underlying_type {
    using type = __underlying_type(T);
};
#else
template<typename T>
struct underlying_type {
    using type = int;  // fallback
};
#endif
template<typename T>
using underlying_type_t = typename underlying_type<T>::type;

// ============================================================
// Part 23: invoke_result, is_invocable, is_nothrow_invocable
// ============================================================
//
// INVOKE protocol (C++17):
//   (1) If F is a pointer-to-member-function: (t1.*f)(t2, ..., tN)
//   (2) If F is a pointer-to-member-data and N == 1:      t1.*f
//   (3) Otherwise:                                         f(t1, ..., tN)
//
// We use void_t-based partial specializations of a class template,
// which guarantees correct SFINAE behavior.

namespace detail {

// Primary template — no matching INVOKE expression
template<typename AlwaysVoid, typename F, typename... Args>
struct invoke_result_impl {};

// Case 1: f(args...) — free function, functor, lambda
template<typename F, typename... Args>
struct invoke_result_impl<
    void_t<decltype(declval<F>()(declval<Args>()...))>,
    F, Args...>
{
    using type = decltype(declval<F>()(declval<Args>()...));
};

// Case 2: (obj.*pmf)(args...) — member function pointer
template<typename F, typename T, typename... Args>
struct invoke_result_impl<
    void_t<decltype((declval<T>().*declval<F>())(declval<Args>()...))>,
    F, T, Args...>
{
    using type = decltype((declval<T>().*declval<F>())(declval<Args>()...));
};

// Case 3: obj.*pmd — member data pointer
template<typename F, typename T>
struct invoke_result_impl<
    void_t<decltype(declval<T>().*declval<F>())>,
    F, T>
{
    using type = decltype(declval<T>().*declval<F>());
};

} // namespace detail

// invoke_result — obtains the result type of INVOKE(F, Args...)
template<typename F, typename... Args>
struct invoke_result
    : detail::invoke_result_impl<void, F, Args...> {};

template<typename F, typename... Args>
using invoke_result_t = typename invoke_result<F, Args...>::type;

// is_invocable — checks if INVOKE(F, Args...) is valid
namespace detail {
template<typename, typename = void_t<>, typename...>
struct is_invocable_impl : false_type {};

template<typename F, typename... Args>
struct is_invocable_impl<F, void_t<invoke_result_t<F, Args...>>, Args...>
    : true_type {};
} // namespace detail

template<typename F, typename... Args>
struct is_invocable : detail::is_invocable_impl<F, void, Args...> {};
template<typename F, typename... Args>
inline constexpr bool is_invocable_v = is_invocable<F, Args...>::value;

// is_nothrow_invocable — checks noexcept(INVOKE(F, Args...))
// Uses void_t-based partial specializations (same pattern as invoke_result_impl)
namespace detail {

template<typename AlwaysVoid, typename F, typename... Args>
struct invoke_noexcept_impl : false_type {};

// Case 1: f(args...)
template<typename F, typename... Args>
struct invoke_noexcept_impl<
    void_t<decltype(declval<F>()(declval<Args>()...))>,
    F, Args...>
    : bool_constant<noexcept(declval<F>()(declval<Args>()...))> {};

// Case 2: (obj.*pmf)(args...)
template<typename F, typename T, typename... Args>
struct invoke_noexcept_impl<
    void_t<decltype((declval<T>().*declval<F>())(declval<Args>()...))>,
    F, T, Args...>
    : bool_constant<noexcept((declval<T>().*declval<F>())(declval<Args>()...))> {};

// Case 3: obj.*pmd
template<typename F, typename T>
struct invoke_noexcept_impl<
    void_t<decltype(declval<T>().*declval<F>())>,
    F, T>
    : bool_constant<noexcept(declval<T>().*declval<F>())> {};

} // namespace detail

template<typename F, typename... Args>
struct is_nothrow_invocable
    : bool_constant<is_invocable_v<F, Args...> &&
          detail::invoke_noexcept_impl<void, F, Args...>::value> {};
template<typename F, typename... Args>
inline constexpr bool is_nothrow_invocable_v =
    is_nothrow_invocable<F, Args...>::value;

// ============================================================
// Part 24: alignment_of, rank, extent
// ============================================================

template<typename T>
struct alignment_of : integral_constant<std::size_t, alignof(T)> {};
template<typename T>
inline constexpr std::size_t alignment_of_v = alignment_of<T>::value;

template<typename T>
struct rank : integral_constant<std::size_t, 0> {};
template<typename T>
struct rank<T[]> : integral_constant<std::size_t, rank<T>::value + 1> {};
template<typename T, std::size_t N>
struct rank<T[N]> : integral_constant<std::size_t, rank<T>::value + 1> {};
template<typename T>
inline constexpr std::size_t rank_v = rank<T>::value;

template<typename T, unsigned I = 0>
struct extent : integral_constant<std::size_t, 0> {};
template<typename T>
struct extent<T[], 0> : integral_constant<std::size_t, 0> {};
template<typename T, std::size_t N>
struct extent<T[N], 0> : integral_constant<std::size_t, N> {};
template<typename T, unsigned I>
struct extent<T[], I> : extent<T, I - 1> {};
template<typename T, std::size_t N, unsigned I>
struct extent<T[N], I> : extent<T, I - 1> {};
template<typename T, unsigned I = 0>
inline constexpr std::size_t extent_v = extent<T, I>::value;

// ============================================================
// Part 25: is_trivially_relocatable — can move via memmove
// ============================================================

template<typename T>
struct is_trivially_relocatable
    : conjunction<is_trivially_copyable<T>, is_trivially_destructible<T>> {};
template<typename T>
inline constexpr bool is_trivially_relocatable_v =
    is_trivially_relocatable<T>::value;

// ============================================================
// Part 26: is_pair — detect pair types
// ============================================================

template<typename T>
struct is_pair : false_type {};

// Forward declaration for pair
template<typename T1, typename T2>
struct pair;

template<typename T1, typename T2>
struct is_pair<pair<T1, T2>> : true_type {};

template<typename T>
inline constexpr bool is_pair_v = is_pair<T>::value;

// ============================================================
// Part 27: is_specialization_of
// ============================================================

template<typename T, template<typename...> class Template>
struct is_specialization_of : false_type {};

template<template<typename...> class Template, typename... Args>
struct is_specialization_of<Template<Args...>, Template> : true_type {};

template<typename T, template<typename...> class Template>
inline constexpr bool is_specialization_of_v =
    is_specialization_of<T, Template>::value;

// ============================================================
// Part 28: remove_cvref
// ============================================================

template<typename T>
using remove_cvref_t = remove_cv_t<remove_reference_t<T>>;

// ============================================================
// Part 29: has_trivial_destructor convenience alias
// ============================================================

template<typename T>
inline constexpr bool has_trivial_destructor_v = is_trivially_destructible_v<T>;

// ============================================================
// Part 30: Size class utilities (used by pool and allocator)
// ============================================================

constexpr std::size_t kNumSizeClasses = 28;
constexpr std::size_t kMaxPoolSize = 8192;

// Map allocation size to size class index [0, 27]
// Uses binary-search-like chain for constexpr evaluation.
constexpr std::size_t size_class_index(std::size_t n) noexcept {
    if (n <= 8)   return 0;
    if (n <= 16)  return 1;
    if (n <= 32)  return 2;
    if (n <= 48)  return 3;
    if (n <= 64)  return 4;
    if (n <= 80)  return 5;
    if (n <= 96)  return 6;
    if (n <= 112) return 7;
    if (n <= 128) return 8;
    if (n <= 160) return 9;
    if (n <= 192) return 10;
    if (n <= 224) return 11;
    if (n <= 256) return 12;
    if (n <= 320) return 13;
    if (n <= 384) return 14;
    if (n <= 448) return 15;
    if (n <= 512) return 16;
    if (n <= 768) return 17;
    if (n <= 1024) return 18;
    if (n <= 1536) return 19;
    if (n <= 2048) return 20;
    if (n <= 2560) return 21;
    if (n <= 3072) return 22;
    if (n <= 3584) return 23;
    if (n <= 4096) return 24;
    if (n <= 5120) return 25;
    if (n <= 6144) return 26;
    return 27;  // <= 8192
}

// Size class index -> actual block size in bytes
constexpr std::size_t size_class_block_size(std::size_t idx) noexcept {
    constexpr std::size_t sizes[] = {
        8,   16,  32,  48,  64,  80,  96,  112,
        128, 160, 192, 224, 256, 320, 384, 448,
        512, 768, 1024, 1536, 2048, 2560, 3072, 3584,
        4096, 5120, 6144, 8192
    };
    return sizes[idx];
}

// Maximum objects per tcache bin for each size class
// Smaller sizes get more entries since they're more frequently
// allocated and have lower memory overhead per entry.
constexpr std::size_t tcache_capacity(std::size_t idx) noexcept {
    constexpr std::size_t caps[] = {
        64,  64,  64,  64,  64,  48,  48,  48,
        32,  32,  32,  32,  16,  16,  16,  16,
        16,  8,   8,   8,   8,   4,   4,   4,
        4,   4,   4,   4
    };
    return caps[idx];
}

// Tcache low watermark: refill when count drops below this
constexpr std::size_t tcache_low_watermark(std::size_t idx) noexcept {
    return tcache_capacity(idx) / 4;
}

} // namespace zstl
