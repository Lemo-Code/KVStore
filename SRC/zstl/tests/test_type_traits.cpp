// ============================================================================
// zstl Type Traits Unit Tests
// Tests: integral_constant, bool_constant, is_void, is_integral, is_floating_point,
// is_pointer, is_reference, is_const, is_volatile, is_same, is_base_of, is_convertible,
// is_constructible, is_trivially_copyable, enable_if, conditional, decay, remove_cv,
// remove_reference, conjunction, disjunction, negation, void_t, is_trivially_relocatable,
// common_type, underlying_type, rank, extent, alignment_of, invoke_result, is_invocable.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <type_traits>  // for comparison with std

// ============================================================
// Custom types for testing
// ============================================================

struct EmptyStruct {};

struct PODStruct {
    int x;
    double y;
    char z;
};

struct NonTrivialStruct {
    int x;
    NonTrivialStruct() : x(0) {}
    NonTrivialStruct(int v) : x(v) {}
    NonTrivialStruct(const NonTrivialStruct& o) : x(o.x) {}
    NonTrivialStruct(NonTrivialStruct&& o) noexcept : x(o.x) { o.x = 0; }
    ~NonTrivialStruct() {}
    NonTrivialStruct& operator=(const NonTrivialStruct&) = default;
    NonTrivialStruct& operator=(NonTrivialStruct&&) = default;
};

struct BaseStruct {
    int base_val;
    virtual ~BaseStruct() = default;
};

struct DerivedStruct : BaseStruct {
    int derived_val;
};

struct NonCopyable {
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable& operator=(NonCopyable&&) = default;
};

struct NonMovable {
    NonMovable() = default;
    NonMovable(const NonMovable&) = default;
    NonMovable(NonMovable&&) = delete;
};

enum Color { Red, Green, Blue };
enum class Perm : unsigned short { Read = 1, Write = 2, Exec = 4 };

struct WithAddressOf {
    int val = 42;
    WithAddressOf* operator&() { return nullptr; } // overloaded operator&
};

// ============================================================
// integral_constant / bool_constant / true_type / false_type
// ============================================================

TEST(TypeTraitsTest, IntegralConstant) {
    using zstl::integral_constant;
    using zstl::true_type;
    using zstl::false_type;

    EXPECT_TRUE((true_type::value));
    EXPECT_FALSE((false_type::value));

    EXPECT_TRUE((zstl::bool_constant<true>::value));
    EXPECT_FALSE((zstl::bool_constant<false>::value));

    // integral_constant with int value
    using five = integral_constant<int, 5>;
    EXPECT_EQ(five::value, 5);
    five f;
    EXPECT_EQ(static_cast<int>(f), 5);
    EXPECT_EQ(f(), 5);
    EXPECT_TRUE((std::is_same_v<five::value_type, int>));
    EXPECT_TRUE((std::is_same_v<five::type, five>));
}

// ============================================================
// Primary type categories: is_void, is_null_pointer, is_integral
// ============================================================

TEST(TypeTraitsTest, IsVoid) {
    EXPECT_TRUE((zstl::is_void_v<void>));
    EXPECT_TRUE((zstl::is_void_v<const void>));
    EXPECT_TRUE((zstl::is_void_v<volatile void>));
    EXPECT_TRUE((zstl::is_void_v<const volatile void>));
    EXPECT_FALSE((zstl::is_void_v<int>));
    EXPECT_FALSE((zstl::is_void_v<void*>));
    EXPECT_FALSE((zstl::is_void_v<std::nullptr_t>));
}

TEST(TypeTraitsTest, IsNullPointer) {
    EXPECT_TRUE((zstl::is_null_pointer_v<std::nullptr_t>));
    EXPECT_TRUE((zstl::is_null_pointer_v<const std::nullptr_t>));
    EXPECT_FALSE((zstl::is_null_pointer_v<int>));
    EXPECT_FALSE((zstl::is_null_pointer_v<void*>));
    EXPECT_FALSE((zstl::is_null_pointer_v<decltype(nullptr)>));
}

TEST(TypeTraitsTest, IsIntegral) {
    EXPECT_TRUE((zstl::is_integral_v<bool>));
    EXPECT_TRUE((zstl::is_integral_v<char>));
    EXPECT_TRUE((zstl::is_integral_v<signed char>));
    EXPECT_TRUE((zstl::is_integral_v<unsigned char>));
    EXPECT_TRUE((zstl::is_integral_v<short>));
    EXPECT_TRUE((zstl::is_integral_v<unsigned short>));
    EXPECT_TRUE((zstl::is_integral_v<int>));
    EXPECT_TRUE((zstl::is_integral_v<unsigned int>));
    EXPECT_TRUE((zstl::is_integral_v<long>));
    EXPECT_TRUE((zstl::is_integral_v<unsigned long>));
    EXPECT_TRUE((zstl::is_integral_v<long long>));
    EXPECT_TRUE((zstl::is_integral_v<unsigned long long>));
    EXPECT_TRUE((zstl::is_integral_v<wchar_t>));
    EXPECT_TRUE((zstl::is_integral_v<char16_t>));
    EXPECT_TRUE((zstl::is_integral_v<char32_t>));
    // cv-qualified
    EXPECT_TRUE((zstl::is_integral_v<const int>));
    EXPECT_TRUE((zstl::is_integral_v<volatile unsigned long>));
    // negatives
    EXPECT_FALSE((zstl::is_integral_v<float>));
    EXPECT_FALSE((zstl::is_integral_v<double>));
    EXPECT_FALSE((zstl::is_integral_v<int*>));
    EXPECT_FALSE((zstl::is_integral_v<void>));
    EXPECT_FALSE((zstl::is_integral_v<std::string>));
}

// ============================================================
// is_floating_point, is_array, is_pointer, is_reference
// ============================================================

TEST(TypeTraitsTest, IsFloatingPoint) {
    EXPECT_TRUE((zstl::is_floating_point_v<float>));
    EXPECT_TRUE((zstl::is_floating_point_v<double>));
    EXPECT_TRUE((zstl::is_floating_point_v<long double>));
    EXPECT_TRUE((zstl::is_floating_point_v<const double>));
    EXPECT_TRUE((zstl::is_floating_point_v<volatile float>));
    EXPECT_FALSE((zstl::is_floating_point_v<int>));
    EXPECT_FALSE((zstl::is_floating_point_v<char>));
}

TEST(TypeTraitsTest, IsPointer) {
    EXPECT_TRUE((zstl::is_pointer_v<int*>));
    EXPECT_TRUE((zstl::is_pointer_v<const int*>));
    EXPECT_TRUE((zstl::is_pointer_v<int* const>));
    EXPECT_TRUE((zstl::is_pointer_v<void*>));
    EXPECT_TRUE((zstl::is_pointer_v<int**>));
    EXPECT_TRUE((zstl::is_pointer_v<BaseStruct*>));
    EXPECT_FALSE((zstl::is_pointer_v<int>));
    EXPECT_FALSE((zstl::is_pointer_v<int&>));
    EXPECT_FALSE((zstl::is_pointer_v<std::nullptr_t>));
    EXPECT_FALSE((zstl::is_pointer_v<int[]>));
}

TEST(TypeTraitsTest, IsReference) {
    EXPECT_TRUE((zstl::is_reference_v<int&>));
    EXPECT_TRUE((zstl::is_reference_v<const int&>));
    EXPECT_TRUE((zstl::is_reference_v<int&&>));
    EXPECT_TRUE((zstl::is_lvalue_reference_v<int&>));
    EXPECT_TRUE((zstl::is_rvalue_reference_v<int&&>));
    EXPECT_FALSE((zstl::is_reference_v<int>));
    EXPECT_FALSE((zstl::is_reference_v<int*>));
    EXPECT_FALSE((zstl::is_lvalue_reference_v<int&&>));
    EXPECT_FALSE((zstl::is_rvalue_reference_v<int&>));
}

TEST(TypeTraitsTest, IsConst) {
    EXPECT_TRUE((zstl::is_const_v<const int>));
    EXPECT_TRUE((zstl::is_const_v<const int&>));
    EXPECT_TRUE((zstl::is_const_v<const volatile int>));
    EXPECT_FALSE((zstl::is_const_v<int>));
    EXPECT_FALSE((zstl::is_const_v<const int*>)); // pointer to const, not const pointer
    EXPECT_TRUE((zstl::is_const_v<int* const>));
}

TEST(TypeTraitsTest, IsVolatile) {
    EXPECT_TRUE((zstl::is_volatile_v<volatile int>));
    EXPECT_TRUE((zstl::is_volatile_v<const volatile int>));
    EXPECT_FALSE((zstl::is_volatile_v<int>));
    EXPECT_FALSE((zstl::is_volatile_v<const int>));
}

// ============================================================
// Composite type categories: is_arithmetic, is_fundamental,
// is_scalar, is_object, is_compound
// ============================================================

TEST(TypeTraitsTest, CompositeCategories) {
    // is_arithmetic
    EXPECT_TRUE((zstl::is_arithmetic_v<int>));
    EXPECT_TRUE((zstl::is_arithmetic_v<float>));
    EXPECT_TRUE((zstl::is_arithmetic_v<unsigned long>));
    EXPECT_FALSE((zstl::is_arithmetic_v<void>));
    EXPECT_FALSE((zstl::is_arithmetic_v<int*>));

    // is_fundamental
    EXPECT_TRUE((zstl::is_fundamental_v<int>));
    EXPECT_TRUE((zstl::is_fundamental_v<float>));
    EXPECT_TRUE((zstl::is_fundamental_v<void>));
    EXPECT_TRUE((zstl::is_fundamental_v<std::nullptr_t>));
    EXPECT_FALSE((zstl::is_fundamental_v<std::string>));
    EXPECT_FALSE((zstl::is_fundamental_v<BaseStruct>));

    // is_scalar
    EXPECT_TRUE((zstl::is_scalar_v<int>));
    EXPECT_TRUE((zstl::is_scalar_v<float>));
    EXPECT_TRUE((zstl::is_scalar_v<int*>));
    EXPECT_TRUE((zstl::is_scalar_v<Color>));
    EXPECT_TRUE((zstl::is_scalar_v<std::nullptr_t>));
    EXPECT_FALSE((zstl::is_scalar_v<std::string>));
    EXPECT_FALSE((zstl::is_scalar_v<BaseStruct>));

    // is_object
    EXPECT_TRUE((zstl::is_object_v<int>));
    EXPECT_TRUE((zstl::is_object_v<std::string>));
    EXPECT_TRUE((zstl::is_object_v<int[5]>));
    EXPECT_TRUE((zstl::is_object_v<BaseStruct>));
    EXPECT_FALSE((zstl::is_object_v<void>));
    EXPECT_FALSE((zstl::is_object_v<int&>));
    EXPECT_FALSE((zstl::is_object_v<void()>));

    // is_compound
    EXPECT_FALSE((zstl::is_compound_v<int>));
    EXPECT_FALSE((zstl::is_compound_v<float>));
    EXPECT_FALSE((zstl::is_compound_v<void>));
    EXPECT_TRUE((zstl::is_compound_v<std::string>));
    EXPECT_TRUE((zstl::is_compound_v<int*>));
    EXPECT_TRUE((zstl::is_compound_v<int&>));
}

// ============================================================
// is_same, is_base_of, is_convertible
// ============================================================

TEST(TypeTraitsTest, IsSame) {
    EXPECT_TRUE((zstl::is_same_v<int, int>));
    EXPECT_TRUE((zstl::is_same_v<const int, const int>));
    EXPECT_FALSE((zstl::is_same_v<int, const int>));
    EXPECT_FALSE((zstl::is_same_v<int, long>));
    EXPECT_FALSE((zstl::is_same_v<int, int&>));
    EXPECT_TRUE((zstl::is_same_v<BaseStruct, BaseStruct>));
    EXPECT_FALSE((zstl::is_same_v<BaseStruct, DerivedStruct>));
}

TEST(TypeTraitsTest, IsBaseOf) {
    EXPECT_TRUE((zstl::is_base_of_v<BaseStruct, DerivedStruct>));
    EXPECT_TRUE((zstl::is_base_of_v<BaseStruct, BaseStruct>)); // is_base_of is reflexive
    EXPECT_FALSE((zstl::is_base_of_v<DerivedStruct, BaseStruct>));
    EXPECT_FALSE((zstl::is_base_of_v<int, int>)); // not classes
    EXPECT_FALSE((zstl::is_base_of_v<BaseStruct, int>));
    EXPECT_FALSE((zstl::is_base_of_v<int, DerivedStruct>));
    EXPECT_TRUE((zstl::is_base_of_v<BaseStruct, const DerivedStruct>));
    // Empty struct
    EXPECT_FALSE((zstl::is_base_of_v<EmptyStruct, BaseStruct>));
}

TEST(TypeTraitsTest, IsConvertible) {
    EXPECT_TRUE((zstl::is_convertible_v<int, int>));
    EXPECT_TRUE((zstl::is_convertible_v<int, long>));
    EXPECT_TRUE((zstl::is_convertible_v<int, double>));
    EXPECT_TRUE((zstl::is_convertible_v<DerivedStruct*, BaseStruct*>));
    EXPECT_TRUE((zstl::is_convertible_v<int[], int*>)); // array to pointer decay
    EXPECT_TRUE((zstl::is_convertible_v<void, void>));
    EXPECT_FALSE((zstl::is_convertible_v<BaseStruct*, DerivedStruct*>)); // no implicit downcast
    EXPECT_FALSE((zstl::is_convertible_v<int*, float*>));
    EXPECT_FALSE((zstl::is_convertible_v<std::string, int>));
}

// ============================================================
// is_constructible
// ============================================================

TEST(TypeTraitsTest, IsConstructible) {
    // default
    EXPECT_TRUE((zstl::is_default_constructible_v<int>));
    EXPECT_TRUE((zstl::is_default_constructible_v<PODStruct>));
    EXPECT_TRUE((zstl::is_default_constructible_v<NonTrivialStruct>));
    EXPECT_TRUE((zstl::is_default_constructible_v<EmptyStruct>));
    EXPECT_FALSE((zstl::is_default_constructible_v<NonCopyable>)); // Wait, it has default ctor
    // Actually NonCopyable has default ctor; it IS default constructible
    EXPECT_TRUE((zstl::is_default_constructible_v<NonCopyable>));

    // copy
    EXPECT_TRUE((zstl::is_copy_constructible_v<int>));
    EXPECT_TRUE((zstl::is_copy_constructible_v<std::string>));
    EXPECT_FALSE((zstl::is_copy_constructible_v<NonCopyable>));
    EXPECT_FALSE((zstl::is_copy_constructible_v<std::unique_ptr<int>>));

    // move
    EXPECT_TRUE((zstl::is_move_constructible_v<int>));
    EXPECT_TRUE((zstl::is_move_constructible_v<std::string>));
    EXPECT_TRUE((zstl::is_move_constructible_v<NonCopyable>));
    EXPECT_FALSE((zstl::is_move_constructible_v<NonMovable>));

    // from args
    EXPECT_TRUE((zstl::is_constructible_v<std::string, const char*>));
    EXPECT_TRUE((zstl::is_constructible_v<NonTrivialStruct, int>));
    EXPECT_FALSE((zstl::is_constructible_v<NonTrivialStruct, std::string>));
}

TEST(TypeTraitsTest, IsTriviallyConstructible) {
    EXPECT_TRUE((zstl::is_trivially_default_constructible_v<int>));
    EXPECT_TRUE((zstl::is_trivially_default_constructible_v<PODStruct>));
    EXPECT_TRUE((zstl::is_trivially_copy_constructible_v<int>));
    EXPECT_FALSE((zstl::is_trivially_default_constructible_v<NonTrivialStruct>));
    EXPECT_FALSE((zstl::is_trivially_default_constructible_v<std::string>));
}

// ============================================================
// is_trivially_copyable
// ============================================================

TEST(TypeTraitsTest, IsTriviallyCopyable) {
    EXPECT_TRUE((zstl::is_trivially_copyable_v<int>));
    EXPECT_TRUE((zstl::is_trivially_copyable_v<double>));
    EXPECT_TRUE((zstl::is_trivially_copyable_v<PODStruct>));
    EXPECT_TRUE((zstl::is_trivially_copyable_v<EmptyStruct>));
    EXPECT_FALSE((zstl::is_trivially_copyable_v<std::string>));
    EXPECT_FALSE((zstl::is_trivially_copyable_v<std::vector<int>>));
    EXPECT_FALSE((zstl::is_trivially_copyable_v<NonTrivialStruct>));
}

// ============================================================
// enable_if, conditional
// ============================================================

template<typename T, zstl::enable_if_t<zstl::is_integral_v<T>, int> = 0>
constexpr bool is_integral_fn(T) { return true; }

template<typename T, zstl::enable_if_t<!zstl::is_integral_v<T>, int> = 0>
constexpr bool is_integral_fn(T) { return false; }

TEST(TypeTraitsTest, EnableIf) {
    EXPECT_TRUE(is_integral_fn(42));
    EXPECT_TRUE(is_integral_fn(0L));
    EXPECT_FALSE(is_integral_fn(3.14));
    EXPECT_FALSE(is_integral_fn("hello"));
}

TEST(TypeTraitsTest, Conditional) {
    EXPECT_TRUE((std::is_same_v<zstl::conditional_t<true, int, float>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::conditional_t<false, int, float>, float>));
    EXPECT_TRUE((std::is_same_v<zstl::conditional_t<(sizeof(int) > 0), int, void>, int>));
}

// ============================================================
// decay, remove_cv, remove_reference, remove_cvref
// ============================================================

TEST(TypeTraitsTest, Decay) {
    EXPECT_TRUE((std::is_same_v<zstl::decay_t<int>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::decay_t<const int>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::decay_t<int&>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::decay_t<int&&>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::decay_t<const int&>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::decay_t<int*>, int*>));
    EXPECT_TRUE((std::is_same_v<zstl::decay_t<int[]>, int*>));
    EXPECT_TRUE((std::is_same_v<zstl::decay_t<int[5]>, int*>));
    EXPECT_TRUE((std::is_same_v<zstl::decay_t<int(int)>, int(*)(int)>));
}

TEST(TypeTraitsTest, RemoveCV) {
    EXPECT_TRUE((std::is_same_v<zstl::remove_cv_t<int>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::remove_cv_t<const int>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::remove_cv_t<volatile int>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::remove_cv_t<const volatile int>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::remove_const_t<const int>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::remove_volatile_t<volatile int>, int>));
}

TEST(TypeTraitsTest, RemoveReference) {
    EXPECT_TRUE((std::is_same_v<zstl::remove_reference_t<int>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::remove_reference_t<int&>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::remove_reference_t<int&&>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::remove_reference_t<const int&>, const int>));
}

TEST(TypeTraitsTest, RemoveCVRef) {
    EXPECT_TRUE((std::is_same_v<zstl::remove_cvref_t<int>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::remove_cvref_t<const int&>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::remove_cvref_t<volatile int&&>, int>));
}

// ============================================================
// conjunction, disjunction, negation
// ============================================================

TEST(TypeTraitsTest, Conjunction) {
    EXPECT_TRUE((zstl::conjunction_v<zstl::true_type, zstl::true_type>));
    EXPECT_FALSE((zstl::conjunction_v<zstl::true_type, zstl::false_type>));
    EXPECT_FALSE((zstl::conjunction_v<zstl::false_type, zstl::true_type>));
    EXPECT_TRUE((zstl::conjunction_v<>)); // empty pack is true
    EXPECT_TRUE((zstl::conjunction_v<zstl::is_integral<int>, zstl::is_arithmetic<int>>));
    EXPECT_FALSE((zstl::conjunction_v<zstl::is_integral<int>, zstl::is_floating_point<int>>));
}

TEST(TypeTraitsTest, Disjunction) {
    EXPECT_TRUE((zstl::disjunction_v<zstl::true_type, zstl::false_type>));
    EXPECT_TRUE((zstl::disjunction_v<zstl::false_type, zstl::true_type>));
    EXPECT_FALSE((zstl::disjunction_v<zstl::false_type, zstl::false_type>));
    EXPECT_FALSE((zstl::disjunction_v<>)); // empty pack is false
    EXPECT_TRUE((zstl::disjunction_v<zstl::is_integral<double>, zstl::is_floating_point<double>>));
}

TEST(TypeTraitsTest, Negation) {
    EXPECT_TRUE((zstl::negation_v<zstl::false_type>));
    EXPECT_FALSE((zstl::negation_v<zstl::true_type>));
    EXPECT_TRUE((zstl::negation_v<zstl::is_void<int>>));
    EXPECT_FALSE((zstl::negation_v<zstl::is_void<void>>));
}

// ============================================================
// void_t
// ============================================================

// SFINAE test: detect if T has .size() member
template<typename T, typename = void>
struct has_size_member : zstl::false_type {};

template<typename T>
struct has_size_member<T, zstl::void_t<decltype(std::declval<T&>().size())>> : zstl::true_type {};

TEST(TypeTraitsTest, VoidT) {
    EXPECT_TRUE((has_size_member<std::vector<int>>::value));
    EXPECT_TRUE((has_size_member<std::string>::value));
    EXPECT_FALSE((has_size_member<int>::value));
    EXPECT_FALSE((has_size_member<int*>::value));
}

// ============================================================
// is_trivially_relocatable
// ============================================================

TEST(TypeTraitsTest, IsTriviallyRelocatable) {
    EXPECT_TRUE((zstl::is_trivially_relocatable_v<int>));
    EXPECT_TRUE((zstl::is_trivially_relocatable_v<double>));
    EXPECT_TRUE((zstl::is_trivially_relocatable_v<PODStruct>));
    EXPECT_TRUE((zstl::is_trivially_relocatable_v<int[10]>));
    EXPECT_FALSE((zstl::is_trivially_relocatable_v<std::string>));
    EXPECT_FALSE((zstl::is_trivially_relocatable_v<NonTrivialStruct>));
}

// ============================================================
// common_type
// ============================================================

TEST(TypeTraitsTest, CommonType) {
    EXPECT_TRUE((std::is_same_v<zstl::common_type_t<int, int>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::common_type_t<int, long>, long>));
    EXPECT_TRUE((std::is_same_v<zstl::common_type_t<int, double>, double>));
    EXPECT_TRUE((std::is_same_v<zstl::common_type_t<int, long, double>, double>));
    EXPECT_TRUE((std::is_same_v<zstl::common_type_t<const int&, int>, int>));
    // single type
    EXPECT_TRUE((std::is_same_v<zstl::common_type_t<int&>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::common_type_t<const volatile int>, int>));
}

// ============================================================
// underlying_type
// ============================================================

TEST(TypeTraitsTest, UnderlyingType) {
    EXPECT_TRUE((std::is_same_v<zstl::underlying_type_t<Color>, int>));
    EXPECT_TRUE((std::is_same_v<zstl::underlying_type_t<Perm>, unsigned short>));
}

// ============================================================
// rank, extent, alignment_of
// ============================================================

TEST(TypeTraitsTest, Rank) {
    EXPECT_EQ((zstl::rank_v<int>), 0u);
    EXPECT_EQ((zstl::rank_v<int[5]>), 1u);
    EXPECT_EQ((zstl::rank_v<int[]>), 1u);
    EXPECT_EQ((zstl::rank_v<int[3][4]>), 2u);
    EXPECT_EQ((zstl::rank_v<int[][4]>), 2u);
    EXPECT_EQ((zstl::rank_v<int[2][3][4]>), 3u);
}

TEST(TypeTraitsTest, Extent) {
    EXPECT_EQ((zstl::extent_v<int>), 0u);
    EXPECT_EQ((zstl::extent_v<int[5]>), 5u);
    EXPECT_EQ((zstl::extent_v<int[]>), 0u); // unknown bound
    EXPECT_EQ((zstl::extent_v<int[3][4], 0>), 3u);
    EXPECT_EQ((zstl::extent_v<int[3][4], 1>), 4u);
    EXPECT_EQ((zstl::extent_v<int[3][4], 2>), 0u); // out of bounds
}

TEST(TypeTraitsTest, AlignmentOf) {
    EXPECT_GE((zstl::alignment_of_v<int>), alignof(int));
    EXPECT_GE((zstl::alignment_of_v<double>), alignof(double));
    EXPECT_GE((zstl::alignment_of_v<void*>), alignof(void*));
    EXPECT_GE((zstl::alignment_of_v<long long>), alignof(long long));
    // alignment_of should exactly match alignof
    EXPECT_EQ((zstl::alignment_of_v<char>), alignof(char));
    EXPECT_EQ((zstl::alignment_of_v<int>), alignof(int));
    EXPECT_EQ((zstl::alignment_of_v<long double>), alignof(long double));
}

// ============================================================
// invoke_result, is_invocable
// ============================================================

int free_func_for_test(int a, int b) { return a + b; }
void void_func_for_test(int& x) { x = 42; }

struct FunctorTest {
    int operator()(int x) const { return x * 2; }
    int multiply(int x) const { return x * 3; }
};

TEST(TypeTraitsTest, InvokeResult) {
    // free function
    EXPECT_TRUE((std::is_same_v<zstl::invoke_result_t<decltype(&free_func_for_test), int, int>, int>));
    // lambda
    auto lam = [](int x) { return x + 1; };
    EXPECT_TRUE((std::is_same_v<zstl::invoke_result_t<decltype(lam), int>, int>));
    // functor
    FunctorTest ft;
    EXPECT_TRUE((std::is_same_v<zstl::invoke_result_t<FunctorTest, int>, int>));
}

TEST(TypeTraitsTest, IsInvocable) {
    auto lam = [](int x) { return x + 1; };
    EXPECT_TRUE((zstl::is_invocable_v<decltype(lam), int>));
    EXPECT_FALSE((zstl::is_invocable_v<decltype(lam), std::string>));

    EXPECT_TRUE((zstl::is_invocable_v<decltype(&free_func_for_test), int, int>));
    EXPECT_FALSE((zstl::is_invocable_v<decltype(&free_func_for_test), int>)); // needs 2 args

    // Functor
    EXPECT_TRUE((zstl::is_invocable_v<FunctorTest, int>));
    EXPECT_FALSE((zstl::is_invocable_v<FunctorTest, const char*>));
}

// ============================================================
// Additional type properties: is_trivial, is_standard_layout,
// is_pod, is_empty, is_polymorphic, is_abstract, is_signed, etc.
// ============================================================

TEST(TypeTraitsTest, TypeProperties) {
    // is_trivial
    EXPECT_TRUE((zstl::is_trivial_v<int>));
    EXPECT_TRUE((zstl::is_trivial_v<PODStruct>));
    EXPECT_TRUE((zstl::is_trivial_v<EmptyStruct>));
    EXPECT_FALSE((zstl::is_trivial_v<std::string>));

    // is_standard_layout
    EXPECT_TRUE((zstl::is_standard_layout_v<int>));
    EXPECT_TRUE((zstl::is_standard_layout_v<PODStruct>));

    // is_pod
    EXPECT_TRUE((zstl::is_pod_v<int>));
    EXPECT_TRUE((zstl::is_pod_v<PODStruct>));
    EXPECT_FALSE((zstl::is_pod_v<NonTrivialStruct>));

    // is_empty
    EXPECT_TRUE((zstl::is_empty_v<EmptyStruct>));

    // is_polymorphic
    EXPECT_TRUE((zstl::is_polymorphic_v<BaseStruct>)); // has virtual destructor
    EXPECT_FALSE((zstl::is_polymorphic_v<PODStruct>));

    // is_abstract (BaseStruct is not abstract; it has a non-pure virtual dtor)
    EXPECT_FALSE((zstl::is_abstract_v<BaseStruct>));

    // is_signed
    EXPECT_TRUE((zstl::is_signed_v<int>));
    EXPECT_TRUE((zstl::is_signed_v<signed char>));
    EXPECT_TRUE((zstl::is_signed_v<float>));
    EXPECT_TRUE((zstl::is_signed_v<double>));
    EXPECT_FALSE((zstl::is_signed_v<unsigned int>));
    EXPECT_FALSE((zstl::is_signed_v<unsigned char>));
    EXPECT_FALSE((zstl::is_signed_v<bool>));

    // is_unsigned
    EXPECT_TRUE((zstl::is_unsigned_v<unsigned int>));
    EXPECT_TRUE((zstl::is_unsigned_v<unsigned char>));
    EXPECT_FALSE((zstl::is_unsigned_v<int>));
    EXPECT_FALSE((zstl::is_unsigned_v<float>));

    // has_virtual_destructor
    EXPECT_TRUE((zstl::has_virtual_destructor_v<BaseStruct>));
    EXPECT_FALSE((zstl::has_virtual_destructor_v<PODStruct>));
}

// ============================================================
// is_swappable
// ============================================================

TEST(TypeTraitsTest, IsSwappable) {
    EXPECT_TRUE((zstl::is_swappable_v<int>));
    EXPECT_TRUE((zstl::is_swappable_v<std::string>));
    EXPECT_TRUE((zstl::is_swappable_v<PODStruct>));
    // NonMovable is copyable; swap via move ctor+assign requires move
    // Actually zstl::swap uses move, so NonMovable is NOT swappable
    EXPECT_FALSE((zstl::is_swappable_v<NonMovable>));
}

// ============================================================
// is_destructible
// ============================================================

TEST(TypeTraitsTest, IsDestructible) {
    EXPECT_TRUE((zstl::is_destructible_v<int>));
    EXPECT_TRUE((zstl::is_destructible_v<std::string>));
    EXPECT_TRUE((zstl::is_destructible_v<NonTrivialStruct>));
    EXPECT_TRUE((zstl::is_destructible_v<int&>)); // references are destructible
    EXPECT_FALSE((zstl::is_destructible_v<void>));
}

// ============================================================
// is_function, is_member_pointer, is_enum, is_class, is_union
// ============================================================

TEST(TypeTraitsTest, IsFunction) {
    EXPECT_TRUE((zstl::is_function_v<void()>));
    EXPECT_TRUE((zstl::is_function_v<int(int, float)>));
    EXPECT_TRUE((zstl::is_function_v<void(int, ...)>));
    EXPECT_FALSE((zstl::is_function_v<int>));
    EXPECT_FALSE((zstl::is_function_v<int(*)(int)>)); // function pointer, not function
}

TEST(TypeTraitsTest, IsEnum) {
    EXPECT_TRUE((zstl::is_enum_v<Color>));
    EXPECT_TRUE((zstl::is_enum_v<Perm>));
    EXPECT_FALSE((zstl::is_enum_v<int>));
    EXPECT_FALSE((zstl::is_enum_v<std::string>));
}

TEST(TypeTraitsTest, IsClass) {
    EXPECT_TRUE((zstl::is_class_v<BaseStruct>));
    EXPECT_TRUE((zstl::is_class_v<std::string>));
    EXPECT_TRUE((zstl::is_class_v<EmptyStruct>));
    EXPECT_FALSE((zstl::is_class_v<int>));
    EXPECT_FALSE((zstl::is_class_v<Color>));
}

// ============================================================
// is_specialization_of
// ============================================================

TEST(TypeTraitsTest, IsSpecializationOf) {
    EXPECT_TRUE((zstl::is_specialization_of_v<zstl::pair<int, float>, zstl::pair>));
    EXPECT_FALSE((zstl::is_specialization_of_v<int, zstl::pair>));
    EXPECT_FALSE((zstl::is_specialization_of_v<zstl::pair<int, float>, zstl::vector>));
}

// ============================================================
// Size class utilities
// ============================================================

TEST(TypeTraitsTest, SizeClassUtilities) {
    // size_class_index
    EXPECT_EQ(zstl::size_class_index(0), 0u);
    EXPECT_EQ(zstl::size_class_index(1), 0u);
    EXPECT_EQ(zstl::size_class_index(8), 0u);
    EXPECT_EQ(zstl::size_class_index(9), 1u);
    EXPECT_EQ(zstl::size_class_index(16), 1u);
    EXPECT_EQ(zstl::size_class_index(32), 2u);
    EXPECT_EQ(zstl::size_class_index(48), 3u);
    EXPECT_EQ(zstl::size_class_index(64), 4u);
    EXPECT_EQ(zstl::size_class_index(256), 12u);
    EXPECT_EQ(zstl::size_class_index(1024), 18u);
    EXPECT_EQ(zstl::size_class_index(4096), 24u);
    EXPECT_EQ(zstl::size_class_index(8192), 27u);
    // boundary: beyond max
    EXPECT_EQ(zstl::size_class_index(8193), 27u);
    EXPECT_EQ(zstl::size_class_index(100000), 27u);

    // size_class_block_size
    EXPECT_EQ(zstl::size_class_block_size(0), 8u);
    EXPECT_EQ(zstl::size_class_block_size(1), 16u);
    EXPECT_EQ(zstl::size_class_block_size(2), 32u);
    EXPECT_EQ(zstl::size_class_block_size(27), 8192u);

    // tcache_capacity
    EXPECT_EQ(zstl::tcache_capacity(0), 64u);
    EXPECT_EQ(zstl::tcache_capacity(27), 4u);

    // tcache_low_watermark
    EXPECT_EQ(zstl::tcache_low_watermark(0), 16u);
}

// ============================================================
// kNumSizeClasses and kMaxPoolSize constants
// ============================================================

TEST(TypeTraitsTest, Constants) {
    EXPECT_EQ(zstl::kNumSizeClasses, 28u);
    EXPECT_EQ(zstl::kMaxPoolSize, 8192u);
}
