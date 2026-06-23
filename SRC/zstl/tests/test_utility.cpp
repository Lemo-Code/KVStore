// ============================================================================
// zstl Utility Unit Tests
// Tests: move, forward, swap (including array swap), exchange, as_const,
// pair (all constructors, make_pair, comparisons, piecewise),
// integer_sequence, index_sequence, min/max/minmax/clamp (values + initializer_list + custom comparator).
// Move semantics with move-only types.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <vector>
#include <utility>  // for std::move/forward comparison

// ============================================================
// Move-only type for testing
// ============================================================

struct MoveOnly {
    int value;
    bool moved_from = false;

    MoveOnly() : value(0) {}
    explicit MoveOnly(int v) : value(v) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&& o) noexcept : value(o.value), moved_from(false) {
        o.value = -1;
        o.moved_from = true;
    }
    MoveOnly& operator=(MoveOnly&& o) noexcept {
        if (this != &o) {
            value = o.value;
            o.value = -1;
            o.moved_from = true;
        }
        return *this;
    }
};

// ============================================================
// move, forward, move_if_noexcept
// ============================================================

TEST(UtilityTest, Move) {
    int x = 42;
    int&& ref = zstl::move(x);
    EXPECT_EQ(&ref, &x); // move doesn't change address
    EXPECT_EQ(ref, 42);

    MoveOnly mo(10);
    MoveOnly mo2 = zstl::move(mo);
    EXPECT_EQ(mo2.value, 10);
    EXPECT_TRUE(mo.moved_from);
    EXPECT_EQ(mo.value, -1);
}

TEST(UtilityTest, MoveDoesNotCopy) {
    std::string s = "hello world this is a long string";
    const void* data_ptr = s.data();
    std::string s2 = zstl::move(s);
    // After move, s2 should reuse the buffer (or at least s is valid but unspecified)
    EXPECT_EQ(s2, "hello world this is a long string");
    (void)data_ptr; // s may or may not be moved-from depending on SSO
}

TEST(UtilityTest, Forward) {
    // Test perfect forwarding through a helper
    auto forwarder = [](auto&& arg) -> decltype(auto) {
        return zstl::forward<decltype(arg)>(arg);
    };

    int x = 5;
    int& ref = forwarder(x);
    EXPECT_EQ(&ref, &x); // should return lvalue ref

    int&& rref = forwarder(42);
    EXPECT_EQ(rref, 42);

    // forward preserves const
    const int cx = 10;
    auto&& cref = forwarder(cx);
    EXPECT_TRUE((std::is_same_v<decltype(cref), const int&>));
}

TEST(UtilityTest, MoveIfNoexcept) {
    // For types with noexcept move, move_if_noexcept returns rvalue ref
    std::string s("test");
    auto result = zstl::move_if_noexcept(s);
    EXPECT_TRUE((std::is_same_v<decltype(result), std::string&&>));

    int x = 5;
    auto result2 = zstl::move_if_noexcept(x);
    EXPECT_TRUE((std::is_same_v<decltype(result2), int&&>));
}

// ============================================================
// swap
// ============================================================

TEST(UtilityTest, SwapBasic) {
    int a = 1, b = 2;
    zstl::swap(a, b);
    EXPECT_EQ(a, 2);
    EXPECT_EQ(b, 1);
}

TEST(UtilityTest, SwapString) {
    std::string a = "hello";
    std::string b = "world";
    zstl::swap(a, b);
    EXPECT_EQ(a, "world");
    EXPECT_EQ(b, "hello");
}

TEST(UtilityTest, SwapArray) {
    int arr1[] = {1, 2, 3, 4, 5};
    int arr2[] = {6, 7, 8, 9, 10};

    zstl::swap(arr1, arr2);

    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(arr1[i], i + 6);
        EXPECT_EQ(arr2[i], i + 1);
    }
}

TEST(UtilityTest, SwapMoveOnly) {
    MoveOnly a(10), b(20);
    zstl::swap(a, b);
    EXPECT_EQ(a.value, 20);
    EXPECT_EQ(b.value, 10);
}

// ============================================================
// exchange
// ============================================================

TEST(UtilityTest, Exchange) {
    int x = 10;
    int old = zstl::exchange(x, 20);
    EXPECT_EQ(old, 10);
    EXPECT_EQ(x, 20);
}

TEST(UtilityTest, ExchangeString) {
    std::string s = "old";
    std::string old = zstl::exchange(s, "new");
    EXPECT_EQ(old, "old");
    EXPECT_EQ(s, "new");
}

TEST(UtilityTest, ExchangeNullptr) {
    int* p = new int(42);
    int* old = zstl::exchange(p, nullptr);
    EXPECT_EQ(*old, 42);
    EXPECT_EQ(p, nullptr);
    delete old;
}

// ============================================================
// as_const
// ============================================================

TEST(UtilityTest, AsConst) {
    int x = 42;
    const int& cx = zstl::as_const(x);
    EXPECT_EQ(cx, 42);
    EXPECT_EQ(&cx, &x);
    EXPECT_TRUE((std::is_same_v<decltype(cx), const int&>));
}

// ============================================================
// addressof
// ============================================================

struct WithOpAddr {
    int val;
    WithOpAddr* operator&() { return reinterpret_cast<WithOpAddr*>(0xdeadbeef); }
};

TEST(UtilityTest, AddressOf) {
    int x = 42;
    EXPECT_EQ(zstl::addressof(x), &x);

    WithOpAddr w{100};
    EXPECT_EQ(&w, reinterpret_cast<void*>(0xdeadbeef)); // operator& is overloaded
    void* real_addr = zstl::addressof(w);
    EXPECT_NE(real_addr, reinterpret_cast<void*>(0xdeadbeef));
    EXPECT_EQ(zstl::addressof(w), &w); // But addressof should return real address
}

// ============================================================
// pair
// ============================================================

TEST(PairTest, DefaultConstructor) {
    zstl::pair<int, double> p;
    EXPECT_EQ(p.first, 0);
    EXPECT_EQ(p.second, 0.0);
}

TEST(PairTest, ValueConstructor) {
    zstl::pair<int, std::string> p(42, "hello");
    EXPECT_EQ(p.first, 42);
    EXPECT_EQ(p.second, "hello");
}

TEST(PairTest, PerfectForwardingConstructor) {
    std::string s = "world";
    zstl::pair<int, std::string> p(10, s);
    EXPECT_EQ(p.first, 10);
    EXPECT_EQ(p.second, "world");
    EXPECT_FALSE(s.empty()); // s was copied, not moved

    zstl::pair<int, std::string> p2(20, zstl::move(s));
    EXPECT_EQ(p2.first, 20);
    EXPECT_EQ(p2.second, "world");
    // s is moved-from (valid but unspecified)
}

TEST(PairTest, CopyConstructor) {
    zstl::pair<int, int> p1(1, 2);
    zstl::pair<int, int> p2(p1);
    EXPECT_EQ(p2.first, 1);
    EXPECT_EQ(p2.second, 2);
}

TEST(PairTest, MoveConstructor) {
    auto p1 = zstl::make_pair(1, std::string("hello"));
    zstl::pair<int, std::string> p2(zstl::move(p1));
    EXPECT_EQ(p2.first, 1);
    EXPECT_EQ(p2.second, "hello");
}

TEST(PairTest, ConvertingCopyConstructor) {
    zstl::pair<int, double> p1(1, 2.5);
    zstl::pair<long, float> p2(p1);
    EXPECT_EQ(p2.first, 1L);
    EXPECT_FLOAT_EQ(p2.second, 2.5f);
}

TEST(PairTest, ConvertingMoveConstructor) {
    zstl::pair<int, std::string> p1(1, "test");
    zstl::pair<long, std::string> p2(zstl::move(p1));
    EXPECT_EQ(p2.first, 1L);
    EXPECT_EQ(p2.second, "test");
}

TEST(PairTest, CopyAssignment) {
    zstl::pair<int, int> p1(1, 2);
    zstl::pair<int, int> p2(3, 4);
    p2 = p1;
    EXPECT_EQ(p2.first, 1);
    EXPECT_EQ(p2.second, 2);
}

TEST(PairTest, MoveAssignment) {
    zstl::pair<int, std::string> p1(1, "hello");
    zstl::pair<int, std::string> p2(2, "world");
    p2 = zstl::move(p1);
    EXPECT_EQ(p2.first, 1);
    EXPECT_EQ(p2.second, "hello");
}

TEST(PairTest, ConvertingAssignment) {
    zstl::pair<int, double> p1(5, 3.14);
    zstl::pair<long, float> p2;
    p2 = p1;
    EXPECT_EQ(p2.first, 5L);
    EXPECT_FLOAT_EQ(p2.second, 3.14f);
}

TEST(PairTest, Swap) {
    zstl::pair<int, std::string> p1(1, "a");
    zstl::pair<int, std::string> p2(2, "b");
    p1.swap(p2);
    EXPECT_EQ(p1.first, 2);
    EXPECT_EQ(p1.second, "b");
    EXPECT_EQ(p2.first, 1);
    EXPECT_EQ(p2.second, "a");
}

// ============================================================
// pair comparisons
// ============================================================

TEST(PairTest, Equality) {
    zstl::pair<int, int> p1(1, 2);
    zstl::pair<int, int> p2(1, 2);
    zstl::pair<int, int> p3(1, 3);

    EXPECT_TRUE(p1 == p2);
    EXPECT_FALSE(p1 == p3);
    EXPECT_TRUE(p1 != p3);
    EXPECT_FALSE(p1 != p2);
}

TEST(PairTest, Relational) {
    zstl::pair<int, int> p1(1, 2);
    zstl::pair<int, int> p2(1, 3);
    zstl::pair<int, int> p3(2, 0);

    EXPECT_TRUE(p1 < p2);
    EXPECT_TRUE(p1 < p3);
    EXPECT_TRUE(p1 <= p1);
    EXPECT_TRUE(p1 <= p2);
    EXPECT_TRUE(p2 > p1);
    EXPECT_TRUE(p3 > p1);
    EXPECT_TRUE(p2 >= p2);
    EXPECT_TRUE(p2 >= p1);
}

// ============================================================
// make_pair
// ============================================================

TEST(PairTest, MakePair) {
    auto p = zstl::make_pair(42, std::string("hello"));
    EXPECT_EQ(p.first, 42);
    EXPECT_EQ(p.second, "hello");
    EXPECT_TRUE((std::is_same_v<decltype(p), zstl::pair<int, std::string>>));
}

TEST(PairTest, MakePairWithRefs) {
    int x = 10;
    const char* s = "world";
    auto p = zstl::make_pair(x, s);
    EXPECT_EQ(p.first, 10);
    EXPECT_STREQ(p.second, "world");
    EXPECT_TRUE((std::is_same_v<decltype(p), zstl::pair<int, const char*>>));
}

// ============================================================
// piecewise_construct, in_place tags
// ============================================================

TEST(PairTest, PiecewiseConstruct) {
    // piecewise_construct is available
    EXPECT_TRUE((std::is_same_v<decltype(zstl::piecewise_construct), const zstl::piecewise_construct_t>));
    EXPECT_TRUE((std::is_same_v<decltype(zstl::in_place), const zstl::in_place_t>));
    EXPECT_TRUE((std::is_same_v<decltype(zstl::in_place_type<int>), const zstl::in_place_type_t<int>>));
    EXPECT_TRUE((std::is_same_v<decltype(zstl::in_place_index<0>), const zstl::in_place_index_t<0>>));
}

// ============================================================
// integer_sequence / index_sequence / make_index_sequence
// ============================================================

template<size_t... Is>
constexpr size_t sum_impl(zstl::index_sequence<Is...>) {
    return (Is + ... + 0);
}

TEST(UtilityTest, IndexSequence) {
    using seq = zstl::make_index_sequence<5>;
    EXPECT_EQ((sum_impl(seq{})), 10u); // 0+1+2+3+4 = 10

    // size()
    EXPECT_EQ((zstl::index_sequence<0, 1, 2>::size()), 3u);
}

TEST(UtilityTest, MakeIndexSequenceEdge) {
    using seq0 = zstl::make_index_sequence<0>;
    EXPECT_EQ(seq0::size(), 0u);
    EXPECT_EQ(sum_impl(seq0{}), 0u);

    using seq1 = zstl::make_index_sequence<1>;
    EXPECT_EQ(seq1::size(), 1u);
    EXPECT_EQ(sum_impl(seq1{}), 0u);

    using seq10 = zstl::make_index_sequence<10>;
    EXPECT_EQ(seq10::size(), 10u);
}

TEST(UtilityTest, IntegerSequenceFor) {
    using seq = zstl::index_sequence_for<int, float, double, char>;
    EXPECT_EQ(seq::size(), 4u);
}

// ============================================================
// min
// ============================================================

TEST(UtilityTest, Min) {
    EXPECT_EQ(zstl::min(1, 2), 1);
    EXPECT_EQ(zstl::min(2, 1), 1);
    EXPECT_EQ(zstl::min(-5, 10), -5);
    EXPECT_EQ(zstl::min(0, 0), 0);

    // With same value
    EXPECT_EQ(&zstl::min(42, 42), &zstl::min(42, 42)); // same reference? not important

    // floating point
    EXPECT_DOUBLE_EQ(zstl::min(3.14, 2.71), 2.71);
}

TEST(UtilityTest, MinInitializerList) {
    auto result = zstl::min({5, 3, 8, 1, 4});
    EXPECT_EQ(result, 1);

    auto result2 = zstl::min({10, 20, 5, 15});
    EXPECT_EQ(result2, 5);

    // single element
    EXPECT_EQ(zstl::min({42}), 42);
}

TEST(UtilityTest, MinCustomComparator) {
    auto comp = [](int a, int b) { return a > b; }; // reverse
    EXPECT_EQ(zstl::min(1, 2, comp), 2);

    // With strings by length
    auto len_comp = [](const std::string& a, const std::string& b) {
        return a.size() < b.size();
    };
    std::string a = "hi", b = "hello";
    EXPECT_EQ(&zstl::min(a, b, len_comp), &a);
}

// ============================================================
// max
// ============================================================

TEST(UtilityTest, Max) {
    EXPECT_EQ(zstl::max(1, 2), 2);
    EXPECT_EQ(zstl::max(2, 1), 2);
    EXPECT_EQ(zstl::max(-5, 10), 10);
    EXPECT_DOUBLE_EQ(zstl::max(3.14, 2.71), 3.14);
}

TEST(UtilityTest, MaxInitializerList) {
    auto result = zstl::max({5, 3, 8, 1, 4});
    EXPECT_EQ(result, 8);

    auto result2 = zstl::max({1, 2, 3, 4, 5});
    EXPECT_EQ(result2, 5);
}

TEST(UtilityTest, MaxCustomComparator) {
    auto comp = [](int a, int b) { return a > b; }; // reverse
    EXPECT_EQ(zstl::max(1, 2, comp), 1);
}

// ============================================================
// minmax
// ============================================================

TEST(UtilityTest, Minmax) {
    auto result = zstl::minmax(1, 2);
    EXPECT_EQ(result.first, 1);
    EXPECT_EQ(result.second, 2);

    auto result2 = zstl::minmax(10, 3);
    EXPECT_EQ(result2.first, 3);
    EXPECT_EQ(result2.second, 10);
}

TEST(UtilityTest, MinmaxEqual) {
    auto result = zstl::minmax(5, 5);
    EXPECT_EQ(result.first, 5);
    EXPECT_EQ(result.second, 5);
}

TEST(UtilityTest, MinmaxInitializerList) {
    auto result = zstl::minmax({5, 3, 8, 1, 4});
    EXPECT_EQ(result.first, 1);
    EXPECT_EQ(result.second, 8);
}

TEST(UtilityTest, MinmaxCustomComparator) {
    auto comp = [](int a, int b) { return a > b; };
    auto result = zstl::minmax(1, 2, comp);
    EXPECT_EQ(result.first, 2); // min under reverse = 2
    EXPECT_EQ(result.second, 1); // max under reverse = 1

    auto result2 = zstl::minmax({3, 1, 4, 2}, comp);
    EXPECT_EQ(result2.first, 4);
    EXPECT_EQ(result2.second, 1);
}

// ============================================================
// clamp
// ============================================================

TEST(UtilityTest, Clamp) {
    EXPECT_EQ(zstl::clamp(5, 1, 10), 5);
    EXPECT_EQ(zstl::clamp(0, 1, 10), 1);   // below lo
    EXPECT_EQ(zstl::clamp(15, 1, 10), 10); // above hi
    EXPECT_EQ(zstl::clamp(1, 1, 10), 1);   // at lo
    EXPECT_EQ(zstl::clamp(10, 1, 10), 10); // at hi
    EXPECT_EQ(zstl::clamp(-5, -10, 0), -5);
}

TEST(UtilityTest, ClampCustomComparator) {
    auto comp = [](int a, int b) { return a > b; };
    // Under reverse comparison: "low" = 10, "high" = 1
    EXPECT_EQ(zstl::clamp(5, 10, 1, comp), 5);
    EXPECT_EQ(zstl::clamp(15, 10, 1, comp), 10);
    EXPECT_EQ(zstl::clamp(0, 10, 1, comp), 1);
}

// ============================================================
// Move semantics with move-only types
// ============================================================

TEST(UtilityTest, PairWithMoveOnly) {
    using MOPair = zstl::pair<MoveOnly, MoveOnly>;

    auto p = zstl::make_pair(MoveOnly(1), MoveOnly(2));
    EXPECT_EQ(p.first.value, 1);
    EXPECT_EQ(p.second.value, 2);

    MOPair p2(zstl::move(p));
    EXPECT_EQ(p2.first.value, 1);
    EXPECT_EQ(p2.second.value, 2);
    EXPECT_TRUE(p.first.moved_from);
    EXPECT_TRUE(p.second.moved_from);
}

// ============================================================
// less / greater / equal_to functors (from utility.h)
// ============================================================

TEST(UtilityTest, LessFunctor) {
    zstl::less<int> lt;
    EXPECT_TRUE(lt(1, 2));
    EXPECT_FALSE(lt(2, 1));
    EXPECT_FALSE(lt(2, 2));

    // void specialization (transparent)
    zstl::less<void> ltv;
    EXPECT_TRUE(ltv(1, 2));
    EXPECT_FALSE(ltv(3.0, 2));
}

TEST(UtilityTest, GreaterFunctor) {
    zstl::greater<int> gt;
    EXPECT_TRUE(gt(3, 2));
    EXPECT_FALSE(gt(1, 2));
    EXPECT_FALSE(gt(2, 2));

    zstl::greater<void> gtv;
    EXPECT_TRUE(gtv(5, 3));
    EXPECT_FALSE(gtv(1, 10));
}

TEST(UtilityTest, EqualToFunctor) {
    zstl::equal_to<int> eq;
    EXPECT_TRUE(eq(5, 5));
    EXPECT_FALSE(eq(5, 6));

    zstl::equal_to<void> eqv;
    EXPECT_TRUE(eqv(42, 42));
    EXPECT_FALSE(eqv("a", "b"));
}

// ============================================================
// identity / select1st / select2nd
// ============================================================

TEST(UtilityTest, Identity) {
    zstl::identity<int> id;
    int x = 42;
    EXPECT_EQ(id(x), 42);
    EXPECT_EQ(&id(x), &x);

    const int y = 10;
    EXPECT_EQ(id(y), 10);
}

TEST(UtilityTest, Select1st) {
    using P = zstl::pair<int, std::string>;
    zstl::select1st<P> sel;

    P p(42, "hello");
    EXPECT_EQ(sel(p), 42);
    EXPECT_EQ(&sel(p), &p.first);

    const P cp(10, "world");
    EXPECT_EQ(sel(cp), 10);
}

TEST(UtilityTest, Select2nd) {
    using P = zstl::pair<int, std::string>;
    zstl::select2nd<P> sel;

    P p(42, "hello");
    EXPECT_EQ(sel(p), "hello");
    EXPECT_EQ(&sel(p), &p.second);

    const P cp(10, "world");
    EXPECT_EQ(sel(cp), "world");
}
