// ============================================================================
// zstl Functional Unit Tests
// Tests: all functors (less, greater, less_equal, greater_equal, not_equal_to,
// plus, minus, multiplies, divides, modulus, negate, logical_and, logical_or,
// logical_not, bit_and, bit_or, bit_xor, bit_not),
// reference_wrapper/ref/cref, function<R(Args...)> (construct from
// lambda/functor/free function, copy, move, operator(), operator bool,
// swap, target, empty function throwing bad_function_call),
// bind with placeholders.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

// ============================================================
// Free functions for testing
// ============================================================

static int free_add(int a, int b) { return a + b; }
static int free_mul(int a, int b) { return a * b; }
static std::string free_concat(const std::string& a, const std::string& b) {
    return a + b;
}

static int g_counter = 0;
static void free_increment() { ++g_counter; }

struct FunctorObject {
    int multiplier;
    explicit FunctorObject(int m) : multiplier(m) {}
    int operator()(int x) const { return x * multiplier; }
};

// ============================================================
// Comparison functors
// ============================================================

TEST(FunctionalTest, Less) {
    zstl::less<int> lt;
    EXPECT_TRUE(lt(1, 2));
    EXPECT_FALSE(lt(2, 1));
    EXPECT_FALSE(lt(2, 2));

    zstl::less<void> ltv;
    EXPECT_TRUE(ltv(1, 2));
    EXPECT_TRUE(ltv(3.0f, 5.0));
    EXPECT_FALSE(ltv(5, 2));
}

TEST(FunctionalTest, Greater) {
    zstl::greater<int> gt;
    EXPECT_TRUE(gt(3, 2));
    EXPECT_FALSE(gt(1, 2));
    EXPECT_FALSE(gt(5, 5));

    zstl::greater<void> gtv;
    EXPECT_TRUE(gtv(5.0, 3.14));
    EXPECT_FALSE(gtv(1, 10));
}

TEST(FunctionalTest, LessEqual) {
    zstl::less_equal<int> le;
    EXPECT_TRUE(le(1, 2));
    EXPECT_TRUE(le(2, 2));
    EXPECT_FALSE(le(3, 2));

    zstl::less_equal<void> lev;
    EXPECT_TRUE(lev(3, 3));
    EXPECT_TRUE(lev(1, 5));
    EXPECT_FALSE(lev(10, 5));
}

TEST(FunctionalTest, GreaterEqual) {
    zstl::greater_equal<int> ge;
    EXPECT_TRUE(ge(3, 2));
    EXPECT_TRUE(ge(2, 2));
    EXPECT_FALSE(ge(1, 2));

    zstl::greater_equal<void> gev;
    EXPECT_TRUE(gev(10, 5));
    EXPECT_FALSE(gev(3, 8));
}

TEST(FunctionalTest, NotEqualTo) {
    zstl::not_equal_to<int> ne;
    EXPECT_TRUE(ne(1, 2));
    EXPECT_FALSE(ne(5, 5));

    zstl::not_equal_to<void> nev;
    EXPECT_TRUE(nev("a", "b"));
    EXPECT_FALSE(nev(42, 42));
}

// ============================================================
// Arithmetic functors
// ============================================================

TEST(FunctionalTest, Plus) {
    zstl::plus<int> p;
    EXPECT_EQ(p(3, 4), 7);

    zstl::plus<void> pv;
    EXPECT_EQ(pv(10, 20), 30);
    EXPECT_DOUBLE_EQ(pv(2.5, 3.5), 6.0);
}

TEST(FunctionalTest, Minus) {
    zstl::minus<int> m;
    EXPECT_EQ(m(10, 3), 7);

    zstl::minus<void> mv;
    EXPECT_EQ(mv(5, 2), 3);
    EXPECT_DOUBLE_EQ(mv(10.0, 2.5), 7.5);
}

TEST(FunctionalTest, Multiplies) {
    zstl::multiplies<int> m;
    EXPECT_EQ(m(6, 7), 42);

    zstl::multiplies<void> mv;
    EXPECT_EQ(mv(3, 4), 12);
    EXPECT_DOUBLE_EQ(mv(2.5, 2.0), 5.0);
}

TEST(FunctionalTest, Divides) {
    zstl::divides<int> d;
    EXPECT_EQ(d(10, 2), 5);
    EXPECT_EQ(d(7, 2), 3); // integer division

    zstl::divides<void> dv;
    EXPECT_DOUBLE_EQ(dv(10.0, 4.0), 2.5);
}

TEST(FunctionalTest, Modulus) {
    zstl::modulus<int> m;
    EXPECT_EQ(m(10, 3), 1);
    EXPECT_EQ(m(7, 2), 1);

    zstl::modulus<void> mv;
    EXPECT_EQ(mv(13, 5), 3);
}

TEST(FunctionalTest, Negate) {
    zstl::negate<int> n;
    EXPECT_EQ(n(5), -5);
    EXPECT_EQ(n(-3), 3);
    EXPECT_EQ(n(0), 0);

    zstl::negate<void> nv;
    EXPECT_EQ(nv(10), -10);
}

// ============================================================
// Logical functors
// ============================================================

TEST(FunctionalTest, LogicalAnd) {
    zstl::logical_and<bool> la;
    EXPECT_TRUE(la(true, true));
    EXPECT_FALSE(la(true, false));
    EXPECT_FALSE(la(false, true));
    EXPECT_FALSE(la(false, false));
}

TEST(FunctionalTest, LogicalOr) {
    zstl::logical_or<bool> lo;
    EXPECT_TRUE(lo(true, true));
    EXPECT_TRUE(lo(true, false));
    EXPECT_TRUE(lo(false, true));
    EXPECT_FALSE(lo(false, false));
}

TEST(FunctionalTest, LogicalNot) {
    zstl::logical_not<bool> ln;
    EXPECT_TRUE(ln(false));
    EXPECT_FALSE(ln(true));
}

// ============================================================
// Bitwise functors
// ============================================================

TEST(FunctionalTest, BitAnd) {
    zstl::bit_and<int> ba;
    EXPECT_EQ(ba(0xFF, 0x0F), 0x0F);
}

TEST(FunctionalTest, BitOr) {
    zstl::bit_or<int> bo;
    EXPECT_EQ(bo(0xF0, 0x0F), 0xFF);
}

TEST(FunctionalTest, BitXor) {
    zstl::bit_xor<int> bx;
    EXPECT_EQ(bx(0xFF, 0x0F), 0xF0);
}

TEST(FunctionalTest, BitNot) {
    zstl::bit_not<unsigned char> bn;
    EXPECT_EQ(bn(0x00u), 0xFFu);
    EXPECT_EQ(bn(0x55u), 0xAAu);
}

// ============================================================
// reference_wrapper / ref / cref
// ============================================================

TEST(FunctionalTest, ReferenceWrapperGet) {
    int x = 42;
    zstl::reference_wrapper<int> rw(x);
    EXPECT_EQ(rw.get(), 42);
    rw.get() = 100;
    EXPECT_EQ(x, 100);
}

TEST(FunctionalTest, ReferenceWrapperImplicitConversion) {
    int x = 10;
    zstl::reference_wrapper<int> rw(x);
    int& ref = rw;
    EXPECT_EQ(&ref, &x);
    EXPECT_EQ(ref, 10);
}

TEST(FunctionalTest, Ref) {
    int x = 5;
    auto rw = zstl::ref(x);
    EXPECT_EQ(rw.get(), 5);
    rw.get() = 99;
    EXPECT_EQ(x, 99);
}

TEST(FunctionalTest, Cref) {
    int x = 5;
    auto rw = zstl::cref(x);
    EXPECT_EQ(rw.get(), 5);
    // cref gives const reference: should not compile to modify through it
    EXPECT_TRUE((std::is_same_v<decltype(rw), zstl::reference_wrapper<const int>>));
}

TEST(FunctionalTest, ReferenceWrapperCall) {
    // reference_wrapper can call a stored functor via operator()
    FunctorObject fo(3);
    zstl::reference_wrapper<FunctorObject> rw(fo);
    EXPECT_EQ(rw(5), 15); // calls fo(5) = 3 * 5
}

// ============================================================
// function<R(Args...)>
// ============================================================

TEST(FunctionalTest, FunctionDefaultConstructor) {
    zstl::function<int(int, int)> f;
    EXPECT_FALSE(f);
}

TEST(FunctionalTest, FunctionNullptrConstructor) {
    zstl::function<int(int, int)> f(nullptr);
    EXPECT_FALSE(f);
}

TEST(FunctionalTest, FunctionFromFreeFunction) {
    zstl::function<int(int, int)> f(free_add);
    EXPECT_TRUE(f);
    EXPECT_EQ(f(3, 4), 7);
}

TEST(FunctionalTest, FunctionFromLambda) {
    auto lam = [](int x, int y) { return x * y; };
    zstl::function<int(int, int)> f(lam);
    EXPECT_TRUE(f);
    EXPECT_EQ(f(3, 4), 12);
}

TEST(FunctionalTest, FunctionFromFunctor) {
    zstl::function<int(int)> f(FunctorObject(5));
    EXPECT_TRUE(f);
    EXPECT_EQ(f(10), 50);
}

TEST(FunctionalTest, FunctionCopyConstructor) {
    zstl::function<int(int, int)> f1(free_add);
    zstl::function<int(int, int)> f2(f1);
    EXPECT_TRUE(f2);
    EXPECT_EQ(f2(5, 3), 8);
}

TEST(FunctionalTest, FunctionMoveConstructor) {
    zstl::function<int(int, int)> f1(free_add);
    zstl::function<int(int, int)> f2(zstl::move(f1));
    EXPECT_TRUE(f2);
    EXPECT_EQ(f2(2, 3), 5);
    // f1 is moved-from (might be null)
}

TEST(FunctionalTest, FunctionCopyAssignment) {
    zstl::function<int(int, int)> f1(free_add);
    zstl::function<int(int, int)> f2(free_mul);
    f2 = f1;
    EXPECT_EQ(f2(2, 3), 5);
}

TEST(FunctionalTest, FunctionMoveAssignment) {
    zstl::function<int(int, int)> f1(free_add);
    zstl::function<int(int, int)> f2(free_mul);
    f2 = zstl::move(f1);
    EXPECT_EQ(f2(2, 3), 5);
}

TEST(FunctionalTest, FunctionAssignCallable) {
    zstl::function<int(int, int)> f;
    f = [](int x, int y) { return x + y; };
    EXPECT_TRUE(f);
    EXPECT_EQ(f(10, 20), 30);
}

TEST(FunctionalTest, FunctionAssignNullptr) {
    zstl::function<int(int, int)> f(free_add);
    EXPECT_TRUE(f);
    f = nullptr;
    EXPECT_FALSE(f);
}

TEST(FunctionalTest, FunctionOperatorBool) {
    zstl::function<void()> f;
    EXPECT_FALSE(f);

    f = free_increment;
    EXPECT_TRUE(f);
}

TEST(FunctionalTest, FunctionBadFunctionCall) {
    zstl::function<int(int)> f;
    EXPECT_THROW(f(42), std::bad_function_call);
}

TEST(FunctionalTest, FunctionVoidReturn) {
    g_counter = 0;
    zstl::function<void()> f(free_increment);
    f();
    EXPECT_EQ(g_counter, 1);
}

TEST(FunctionalTest, FunctionSwap) {
    zstl::function<int(int, int)> f1(free_add);
    zstl::function<int(int, int)> f2(free_mul);
    f1.swap(f2);
    EXPECT_EQ(f1(2, 3), 6);  // now multiply
    EXPECT_EQ(f2(2, 3), 5);  // now add
}

TEST(FunctionalTest, FunctionTarget) {
    zstl::function<int(int, int)> f(free_add);
    auto* target = f.target<int(*)(int, int)>();
    EXPECT_NE(target, nullptr);
    EXPECT_EQ((*target)(2, 3), 5);

    // Wrong type returns null
    auto* wrong_target = f.target<int(*)(double, double)>();
    EXPECT_EQ(wrong_target, nullptr);
}

TEST(FunctionalTest, FunctionTargetEmpty) {
    zstl::function<int(int, int)> f;
    auto* target = f.target<int(*)(int, int)>();
    EXPECT_EQ(target, nullptr);
}

TEST(FunctionalTest, FunctionComparisonNullptr) {
    zstl::function<int(int)> f;
    EXPECT_TRUE(f == nullptr);
    EXPECT_TRUE(nullptr == f);
    EXPECT_FALSE(f != nullptr);
    EXPECT_FALSE(nullptr != f);

    f = [](int x) { return x; };
    EXPECT_FALSE(f == nullptr);
    EXPECT_TRUE(f != nullptr);
}

// ============================================================
// bind
// ============================================================

TEST(FunctionalTest, BindFreeFunction) {
    auto bound = zstl::bind(free_add, zstl::placeholders::_1, zstl::placeholders::_2);
    EXPECT_EQ(bound(10, 20), 30);
}

TEST(FunctionalTest, BindWithFixedArg) {
    auto bound = zstl::bind(free_add, zstl::placeholders::_1, 5);
    EXPECT_EQ(bound(10), 15);
}

TEST(FunctionalTest, BindSwapArgs) {
    auto bound = zstl::bind(free_add, zstl::placeholders::_2, zstl::placeholders::_1);
    EXPECT_EQ(bound(5, 10), 15);
}

TEST(FunctionalTest, BindPlaceholderOrder) {
    auto bound = zstl::bind(free_add, zstl::placeholders::_2, zstl::placeholders::_1);
    EXPECT_EQ(bound(100, 200), 300);
}

TEST(FunctionalTest, BindWithMultiplePlaceholders) {
    auto bound = zstl::bind(free_add,
        zstl::bind(free_add, zstl::placeholders::_1, zstl::placeholders::_2),
        zstl::placeholders::_3);
    // bound(a,b,c) = (a+b) + c
    EXPECT_EQ(bound(1, 2, 3), 6);
}

TEST(FunctionalTest, BindLambda) {
    auto lam = [](int a, const std::string& b) -> std::string {
        return b + std::to_string(a);
    };
    auto bound = zstl::bind(lam, zstl::placeholders::_1, std::string("num:"));
    EXPECT_EQ(bound(42), "num:42");
}

TEST(FunctionalTest, BindNoPlaceholders) {
    auto bound = zstl::bind(free_add, 3, 4);
    EXPECT_EQ(bound(), 7);
}

// ============================================================
// is_placeholder
// ============================================================

TEST(FunctionalTest, IsPlaceholder) {
    EXPECT_EQ(zstl::is_placeholder_v<decltype(zstl::placeholders::_1)>, 1);
    EXPECT_EQ(zstl::is_placeholder_v<decltype(zstl::placeholders::_5)>, 5);
    EXPECT_EQ(zstl::is_placeholder_v<int>, 0);
    EXPECT_EQ(zstl::is_placeholder_v<std::string>, 0);
}

// ============================================================
// Void specializations of functors
// ============================================================

TEST(FunctionalTest, TransparentFunctors) {
    // All void-specialized functors should be transparent (heterogeneous lookup)
    zstl::less<void> ltv;
    EXPECT_TRUE(ltv(1, 2L));
    EXPECT_TRUE(ltv(1.0f, 2.0));

    zstl::plus<void> pv;
    EXPECT_EQ(pv(1, 2L), 3L);
    EXPECT_DOUBLE_EQ(pv(1.5, 2.5), 4.0);
}
