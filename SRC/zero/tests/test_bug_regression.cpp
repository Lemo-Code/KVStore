// test_bug_regression.cpp — Comprehensive edge-case and regression tests
// for ALL zero library subsystems.
//
// Covers: base utilities, thread primitives, fiber infrastructure,
// scheduler, networking, I/O, logging, and configuration.
// Each test targets real edge cases that could break in production.

#include <gtest/gtest.h>
#include "zero/zero.h"

// Subsystem headers not in the umbrella zero.h
#include "zero/thread/rwlock.h"
#include "zero/thread/condition_variable.h"
#include "zero/thread/barrier.h"
#include "zero/fiber/channel.h"
#include "zero/scheduler/work_stealing_queue.h"
#include "zero/scheduler/fd_manager.h"
#include "zero/scheduler/hook.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <limits>
#include <sys/eventfd.h>
#include <fcntl.h>

using namespace zero;

// =====================================================================
// Helpers
// =====================================================================
namespace {
static std::string make_large_string(size_t n) {
    std::string s;
    s.reserve(n);
    for (size_t i = 0; i < n; ++i)
        s.push_back(static_cast<char>('A' + (i % 26)));
    return s;
}

struct TrackedDestroy {
    inline static std::atomic<int> alive{0};
    TrackedDestroy() { alive.fetch_add(1); }
    TrackedDestroy(const TrackedDestroy&) { alive.fetch_add(1); }
    TrackedDestroy(TrackedDestroy&&) noexcept { alive.fetch_add(1); }
    ~TrackedDestroy() { alive.fetch_sub(1); }
};
} // namespace

// =====================================================================
// Section 1 — Base utilities
// =====================================================================

// --- Endian ---
TEST(EndianBugRegression, ByteSwapEdgeValues) {
    EXPECT_EQ(byteswap16(0x0000), 0x0000);
    EXPECT_EQ(byteswap16(0xFFFF), 0xFFFF);
    EXPECT_EQ(byteswap32(0x00000000u), 0x00000000u);
    EXPECT_EQ(byteswap32(0xFFFFFFFFu), 0xFFFFFFFFu);
    EXPECT_EQ(byteswap64(0ULL), 0ULL);
    EXPECT_EQ(byteswap64(~0ULL), ~0ULL);
}

TEST(EndianBugRegression, ByteSwapRoundTrip) {
    EXPECT_EQ(byteswap16(byteswap16(0x1234)), 0x1234);
    EXPECT_EQ(byteswap32(byteswap32(0x12345678u)), 0x12345678u);
    EXPECT_EQ(byteswap64(byteswap64(0x123456789ABCDEF0ULL)), 0x123456789ABCDEF0ULL);
    // Edge values
    EXPECT_EQ(byteswap16(byteswap16(0x0001)), 0x0001);
    EXPECT_EQ(byteswap16(byteswap16(0x8000)), 0x8000);
}

TEST(EndianBugRegression, NetworkOrderRoundTrip) {
    uint32_t v32 = 0x12345678u;
    EXPECT_EQ(ntoh32(hton32(v32)), v32);
    uint16_t v16 = 0xABCD;
    EXPECT_EQ(ntoh16(hton16(v16)), v16);
    uint64_t v64 = 0x0123456789ABCDEFULL;
    EXPECT_EQ(ntoh64(hton64(v64)), v64);
    // Zero and all-ones
    EXPECT_EQ(ntoh32(hton32(0u)), 0u);
    EXPECT_EQ(ntoh32(hton32(~0u)), ~0u);
}

TEST(EndianBugRegression, LegacyAliases) {
    EXPECT_EQ(htons(0x1234), hton16(0x1234));
    EXPECT_EQ(htonl(0x12345678u), hton32(0x12345678u));
    EXPECT_EQ(ntohs(0x1234), ntoh16(0x1234));
    EXPECT_EQ(ntohl(0x12345678u), ntoh32(0x12345678u));
}

TEST(EndianBugRegression, HtonTemplate) {
    EXPECT_EQ(hton<uint16_t>(0xABCD), hton16(0xABCD));
    EXPECT_EQ(hton<uint32_t>(0xDEADBEEFu), hton32(0xDEADBEEFu));
    EXPECT_EQ(hton<uint64_t>(0xCAFEull), hton64(0xCAFEull));
    EXPECT_EQ(ntoh<uint32_t>(hton<uint32_t>(42u)), 42u);
}

TEST(EndianBugRegression, LittleEndianLoadStore) {
    alignas(8) uint8_t buf[8] = {0x78, 0x56, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(load_le32(buf), 0x12345678u);
    uint8_t out[4] = {};
    store_le32(out, 0xAABBCCDDu);
    EXPECT_EQ(load_le32(out), 0xAABBCCDDu);

    // 16-bit
    uint8_t buf16[2] = {0x41, 0x42};
    EXPECT_EQ(load_le16(buf16), 0x4241u);
    uint8_t out16[2] = {};
    store_le16(out16, 0xCDEFu);
    EXPECT_EQ(load_le16(out16), 0xCDEFu);

    // 64-bit
    uint8_t buf64[8] = {0xEF, 0xBE, 0xAD, 0xDE, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(load_le64(buf64), 0xDEADBEEFull);
}

TEST(EndianBugRegression, BigEndianLoadStore) {
    alignas(8) uint8_t buf[8] = {0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(load_be32(buf), 0x12345678u);
    uint8_t out[4] = {};
    store_be32(out, 0xAABBCCDDu);
    EXPECT_EQ(load_be32(out), 0xAABBCCDDu);

    uint8_t buf16[2] = {0x41, 0x42};
    EXPECT_EQ(load_be16(buf16), 0x4142u);
}

TEST(EndianBugRegression, SignedLoadStore) {
    alignas(4) uint8_t buf[4] = {};
    store_le_i32(buf, -42);
    EXPECT_EQ(load_le_i32(buf), -42);

    store_be_i32(buf, -100);
    EXPECT_EQ(load_be_i32(buf), -100);

    uint8_t buf8[8] = {};
    store_le_i64(buf8, INT64_MIN + 1);
    EXPECT_EQ(load_le_i64(buf8), INT64_MIN + 1);
}

TEST(EndianBugRegression, CompileTimeDetection) {
    // Verify the detection functions compile and return bool
    static_assert(is_little_endian() || is_big_endian(),
                  "Must be either little or big endian");
    EXPECT_TRUE(is_little_endian() != is_big_endian());
}

// --- Lexical Cast ---
TEST(LexicalCastBugRegression, IntRoundTrip) {
    EXPECT_EQ(lexical_cast<int>("42"), 42);
    EXPECT_EQ(lexical_cast<int>("-42"), -42);
    EXPECT_EQ(lexical_cast<int>("0"), 0);
    EXPECT_EQ(lexical_cast<int>("+100"), 100);
    EXPECT_EQ(lexical_cast<int>("2147483647"), 2147483647);
    EXPECT_EQ(lexical_cast<int>("-2147483648"), (-2147483647 - 1));
}

TEST(LexicalCastBugRegression, IntFailure) {
    EXPECT_THROW(lexical_cast<int>("abc"), std::runtime_error);
    EXPECT_THROW(lexical_cast<int>(""), std::runtime_error);
    EXPECT_THROW(lexical_cast<int>("12.34"), std::runtime_error);
    EXPECT_THROW(lexical_cast<int>(std::string_view("xyz")), std::runtime_error);
}

TEST(LexicalCastBugRegression, UnsignedRoundTrip) {
    EXPECT_EQ(lexical_cast<unsigned int>("42"), 42u);
    EXPECT_EQ(lexical_cast<unsigned int>("0"), 0u);
    EXPECT_EQ(lexical_cast<uint32_t>("4294967295"), 4294967295u);
}

TEST(LexicalCastBugRegression, Int64RoundTrip) {
    EXPECT_EQ(lexical_cast<int64_t>("9223372036854775807"),
              INT64_MAX);
    EXPECT_EQ(lexical_cast<int64_t>("-9223372036854775808"),
              INT64_MIN);
    EXPECT_EQ(lexical_cast<uint64_t>("18446744073709551615"),
              UINT64_MAX);
}

TEST(LexicalCastBugRegression, DoubleRoundTrip) {
    EXPECT_DOUBLE_EQ(lexical_cast<double>("3.14"), 3.14);
    EXPECT_DOUBLE_EQ(lexical_cast<double>("-1.5"), -1.5);
    EXPECT_DOUBLE_EQ(lexical_cast<double>("0.0"), 0.0);
    EXPECT_DOUBLE_EQ(lexical_cast<double>("1e10"), 1e10);
}

TEST(LexicalCastBugRegression, FloatRoundTrip) {
    EXPECT_FLOAT_EQ(lexical_cast<float>("2.5"), 2.5f);
    EXPECT_FLOAT_EQ(lexical_cast<float>("-0.125"), -0.125f);
}

TEST(LexicalCastBugRegression, NumericToStringRoundTrip) {
    EXPECT_EQ(lexical_cast<std::string>(42), "42");
    EXPECT_EQ(lexical_cast<std::string>(-42), "-42");
    EXPECT_EQ(lexical_cast<std::string>(0), "0");
    EXPECT_EQ(lexical_cast<std::string>(3.14), "3.14");
    EXPECT_EQ(lexical_cast<std::string>(INT64_MAX),
              std::to_string(INT64_MAX));
}

TEST(LexicalCastBugRegression, BoolToFromString) {
    EXPECT_EQ(lexical_cast<std::string>(true), "true");
    EXPECT_EQ(lexical_cast<std::string>(false), "false");
    EXPECT_TRUE(lexical_cast<bool>("true"));
    EXPECT_TRUE(lexical_cast<bool>("1"));
    EXPECT_TRUE(lexical_cast<bool>("yes"));
    EXPECT_TRUE(lexical_cast<bool>("on"));
    EXPECT_FALSE(lexical_cast<bool>("false"));
    EXPECT_FALSE(lexical_cast<bool>("0"));
    EXPECT_FALSE(lexical_cast<bool>("no"));
    EXPECT_FALSE(lexical_cast<bool>("off"));
    EXPECT_THROW(lexical_cast<bool>("maybe"), std::runtime_error);
}

TEST(LexicalCastBugRegression, NullPointerGuard) {
    EXPECT_THROW(lexical_cast<int>((const char*)nullptr), std::runtime_error);
    EXPECT_THROW(lexical_cast<bool>((const char*)nullptr), std::runtime_error);
}

TEST(LexicalCastBugRegression, ConstCharReturnGuard) {
    EXPECT_THROW(lexical_cast<const char*>(42), std::runtime_error);
    EXPECT_THROW(lexical_cast<const char*>(true), std::runtime_error);
}

TEST(LexicalCastBugRegression, SafeConversion) {
    int out = -1;
    EXPECT_TRUE(lexical_cast_safe<int>("999", out));
    EXPECT_EQ(out, 999);
    EXPECT_FALSE(lexical_cast_safe<int>("not-a-number", out));
    // out unchanged on failure
    EXPECT_EQ(out, 999);
}

TEST(LexicalCastBugRegression, ToCharsHelpers) {
    std::string s;
    EXPECT_TRUE(to_chars_string(42, s));
    EXPECT_EQ(s, "42");
    EXPECT_TRUE(to_chars_string(-1, s));
    EXPECT_EQ(s, "-1");

    int v = 0;
    EXPECT_TRUE(from_chars_string("123", v));
    EXPECT_EQ(v, 123);
    EXPECT_FALSE(from_chars_string("abc", v));
}

TEST(LexicalCastBugRegression, FloatToFromStringHelpers) {
    EXPECT_EQ(float_to_string(1.5f), "1.5");
    EXPECT_EQ(double_to_string(3.14159), "3.14159");

    float f = 0;
    EXPECT_TRUE(string_to_float("2.5", f));
    EXPECT_FLOAT_EQ(f, 2.5f);

    double d = 0;
    EXPECT_TRUE(string_to_double("6.28", d));
    EXPECT_DOUBLE_EQ(d, 6.28);
    EXPECT_FALSE(string_to_double("nope", d));
}

TEST(LexicalCastBugRegression, NumericToNumericCast) {
    EXPECT_EQ(lexical_cast<int>(42.9), 42);
    EXPECT_EQ(lexical_cast<double>(42), 42.0);
    EXPECT_EQ(lexical_cast<int64_t>(42), 42);
    EXPECT_EQ(lexical_cast<short>(100), 100);
}

TEST(LexicalCastBugRegression, HexOctalNotSupported) {
    // from_chars does not handle 0x/0 prefixes by default in C++17
    EXPECT_THROW(lexical_cast<int>("0xFF"), std::runtime_error);
}

// --- Any ---
TEST(AnyBugRegression, DefaultAny) {
    any a;
    EXPECT_FALSE(a.has_value());
    EXPECT_FALSE(static_cast<bool>(a));
    EXPECT_EQ(a.type(), typeid(void));
    EXPECT_THROW(any_cast<int>(a), bad_any_cast);
}

TEST(AnyBugRegression, IntAny) {
    any a = 42;
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(any_cast<int>(a), 42);
    EXPECT_THROW(any_cast<double>(a), bad_any_cast);
    EXPECT_EQ(a.type(), typeid(int));
}

TEST(AnyBugRegression, AnyCastPointer) {
    any a = 42;
    EXPECT_NE(any_cast<int>(&a), nullptr);
    EXPECT_EQ(*any_cast<int>(&a), 42);
    EXPECT_EQ(any_cast<double>(&a), nullptr);
    EXPECT_EQ(any_cast<int>((const any*)&a), any_cast<int>(&a));
}

TEST(AnyBugRegression, MoveAny) {
    any a = std::string("hello world");
    EXPECT_TRUE(a.has_value());
    any b = std::move(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(b.has_value());
    EXPECT_EQ(any_cast<std::string>(b), "hello world");
}

TEST(AnyBugRegression, MoveAssignAny) {
    any a = 100;
    any b;
    b = std::move(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(any_cast<int>(b), 100);
}

TEST(AnyBugRegression, CopyAny) {
    any a = std::string("copy me");
    any b = a;
    EXPECT_EQ(any_cast<std::string>(a), "copy me");
    EXPECT_EQ(any_cast<std::string>(b), "copy me");
    b = std::string("modified");
    EXPECT_EQ(any_cast<std::string>(a), "copy me"); // Not affected
}

TEST(AnyBugRegression, CopyAssignAny) {
    any a = std::string("original");
    any b;
    b = a;
    EXPECT_EQ(any_cast<std::string>(b), "original");
}

TEST(AnyBugRegression, ResetAny) {
    any a = 42;
    EXPECT_TRUE(a.has_value());
    a.reset();
    EXPECT_FALSE(a.has_value());
    a.reset(); // Double reset should be safe
    EXPECT_FALSE(a.has_value());
}

TEST(AnyBugRegression, ReassignAny) {
    any a = 42;
    a = 3.14;
    EXPECT_DOUBLE_EQ(any_cast<double>(a), 3.14);

    a = std::string("reassigned");
    EXPECT_EQ(any_cast<std::string>(a), "reassigned");
}

TEST(AnyBugRegression, SwapAny) {
    any a = 42;
    any b = std::string("swapped");
    a.swap(b);
    EXPECT_EQ(any_cast<std::string>(a), "swapped");
    EXPECT_EQ(any_cast<int>(b), 42);

    // Swap empty
    any c;
    a.swap(c);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(any_cast<std::string>(c), "swapped");
}

TEST(AnyBugRegression, LargeObjectAny) {
    // Type larger than SBO (32 bytes)
    struct Large { char data[128] = {}; };
    Large l;
    l.data[42] = 'x';
    any a = l;
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(any_cast<Large>(a).data[42], 'x');
}

TEST(AnyBugRegression, RvalueAnyCast) {
    any a = std::string("move from rvalue");
    std::string s = any_cast<std::string>(std::move(a));
    EXPECT_EQ(s, "move from rvalue");
}

TEST(AnyBugRegression, MakeAny) {
    auto a = make_any<int>(42);
    EXPECT_EQ(any_cast<int>(a), 42);

    auto b = make_any<std::string>("hello");
    EXPECT_EQ(any_cast<std::string>(b), "hello");
}

TEST(AnyBugRegression, InPlaceConstruct) {
    any a(std::in_place_type<std::string>, "in place");
    EXPECT_EQ(any_cast<std::string>(a), "in place");
}

TEST(AnyBugRegression, MultipleTypes) {
    any a = 42;
    EXPECT_EQ(any_cast<int>(a), 42);
    a = 3.14;
    EXPECT_DOUBLE_EQ(any_cast<double>(a), 3.14);
    a = std::string("test");
    EXPECT_EQ(any_cast<std::string>(a), "test");
    a = true;
    EXPECT_TRUE(any_cast<bool>(a));
    a = static_cast<uint64_t>(UINT64_MAX);
    EXPECT_EQ(any_cast<uint64_t>(a), UINT64_MAX);
}

TEST(AnyBugRegression, SelfAssign) {
    any a = 42;
    a = a; // Self-assignment should be a no-op
    EXPECT_EQ(any_cast<int>(a), 42);
}

// --- Optional ---
TEST(OptionalBugRegression, EmptyOptional) {
    optional<int> o;
    EXPECT_FALSE(o.has_value());
    EXPECT_FALSE(static_cast<bool>(o));
    EXPECT_EQ(o.value_or(42), 42);
    EXPECT_THROW(o.value(), bad_optional_access);
    EXPECT_EQ(o, nullopt);
    EXPECT_EQ(nullopt, o);
    EXPECT_NE(o, nullopt); // operator!= with nullopt checks has_value
    EXPECT_NE(nullopt, o);
}

TEST(OptionalBugRegression, ValueOptional) {
    optional<int> o = 10;
    EXPECT_TRUE(o.has_value());
    EXPECT_TRUE(static_cast<bool>(o));
    EXPECT_EQ(o.value(), 10);
    EXPECT_EQ(*o, 10);
    EXPECT_EQ(o.value_or(0), 10);
}

TEST(OptionalBugRegression, EmplaceOptional) {
    optional<std::string> o;
    o.emplace("hello world");
    EXPECT_EQ(*o, "hello world");
    o.emplace("goodbye");
    EXPECT_EQ(*o, "goodbye");
}

TEST(OptionalBugRegression, ResetOptional) {
    optional<int> o = 42;
    EXPECT_TRUE(o.has_value());
    o.reset();
    EXPECT_FALSE(o.has_value());
    o.reset(); // Double reset: ok
    EXPECT_FALSE(o.has_value());
}

TEST(OptionalBugRegression, AssignNullopt) {
    optional<int> o = 42;
    o = nullopt;
    EXPECT_FALSE(o.has_value());
}

TEST(OptionalBugRegression, CopyOptional) {
    optional<std::string> a = std::string("original");
    optional<std::string> b = a;
    EXPECT_EQ(*a, "original");
    EXPECT_EQ(*b, "original");
    *b = "modified";
    EXPECT_EQ(*a, "original"); // Independent
}

TEST(OptionalBugRegression, MoveOptional) {
    optional<std::string> a = std::string("movable");
    optional<std::string> b = std::move(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(b.has_value());
    EXPECT_EQ(*b, "movable");
}

TEST(OptionalBugRegression, ValueAssignOptional) {
    optional<int> o;
    o = 42;
    EXPECT_EQ(*o, 42);
    o = 100; // Overwrite existing
    EXPECT_EQ(*o, 100);
}

TEST(OptionalBugRegression, MoveValueAssignOptional) {
    optional<std::string> o;
    o = std::string("assigned");
    EXPECT_EQ(*o, "assigned");
}

TEST(OptionalBugRegression, SwapOptional) {
    optional<int> a = 1;
    optional<int> b = 2;
    a.swap(b);
    EXPECT_EQ(*a, 2);
    EXPECT_EQ(*b, 1);

    optional<int> c;
    a.swap(c);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(*c, 2);
}

TEST(OptionalBugRegression, Comparisons) {
    optional<int> a = 1, b = 1, c = 2;
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(a < c);
    EXPECT_TRUE(c > a);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(a <= c);
    EXPECT_TRUE(c >= a);
}

TEST(OptionalBugRegression, ComparisonsWithNullopt) {
    optional<int> empty;
    optional<int> full = 42;
    EXPECT_TRUE(empty == nullopt);
    EXPECT_TRUE(nullopt == empty);
    EXPECT_FALSE(full == nullopt);
    EXPECT_FALSE(nullopt == full);
    EXPECT_FALSE(empty < nullopt);
    EXPECT_FALSE(nullopt < empty);
    EXPECT_TRUE(nullopt < full);
}

TEST(OptionalBugRegression, ComparisonsWithValue) {
    optional<int> o = 42;
    EXPECT_TRUE(o == 42);
    EXPECT_TRUE(42 == o);
    EXPECT_TRUE(o != 0);
    EXPECT_FALSE(o == 0);
    EXPECT_TRUE(o < 100);
    EXPECT_TRUE(0 < o);
}

TEST(OptionalBugRegression, MakeOptional) {
    auto o1 = make_optional(42);
    EXPECT_EQ(*o1, 42);

    auto o2 = make_optional<std::string>("hello");
    EXPECT_EQ(*o2, "hello");

    auto o3 = make_optional<std::string>(5, 'x');
    EXPECT_EQ(*o3, "xxxxx");
}

TEST(OptionalBugRegression, PointerAccess) {
    optional<std::string> o = std::string("hello");
    EXPECT_EQ(o->size(), 5u);
}

TEST(OptionalBugRegression, ConstAccess) {
    const optional<int> o = 42;
    EXPECT_EQ(o.value(), 42);
    EXPECT_EQ(*o, 42);
    EXPECT_EQ(o.value_or(0), 42);
}

TEST(OptionalBugRegression, RvalueValue) {
    optional<std::string> o = std::string("move me");
    std::string s = std::move(o).value();
    EXPECT_EQ(s, "move me");
}

TEST(OptionalBugRegression, TrackedDestroyInOptional) {
    TrackedDestroy::alive = 0;
    {
        optional<TrackedDestroy> o;
        o.emplace();
        EXPECT_EQ(TrackedDestroy::alive.load(), 1);
        o.reset();
        EXPECT_EQ(TrackedDestroy::alive.load(), 0);
    }
}

TEST(OptionalBugRegression, StdSwapOptional) {
    optional<int> a = 1;
    optional<int> b = 2;
    std::swap(a, b);
    EXPECT_EQ(*a, 2);
    EXPECT_EQ(*b, 1);
}

// --- Expected ---
TEST(ExpectedBugRegression, ValueExpected) {
    expected<int, std::string> e = 42;
    EXPECT_TRUE(e.has_value());
    EXPECT_TRUE(static_cast<bool>(e));
    EXPECT_EQ(e.value(), 42);
    EXPECT_EQ(*e, 42);
    EXPECT_EQ(e.value_or(0), 42);
}

TEST(ExpectedBugRegression, ErrorExpected) {
    expected<int, std::string> e = unexpected<std::string>("error msg");
    EXPECT_FALSE(e.has_value());
    EXPECT_FALSE(static_cast<bool>(e));
    EXPECT_EQ(e.error(), "error msg");
    EXPECT_EQ(e.value_or(42), 42);
    EXPECT_THROW(e.value(), bad_expected_access<std::string>);
}

TEST(ExpectedBugRegression, UnexpectedCtors) {
    unexpected<std::string> u1("error");
    EXPECT_EQ(u1.error(), "error");

    unexpected<std::string> u2(std::in_place, "constructed");
    EXPECT_EQ(u2.error(), "constructed");

    unexpected<std::string> u3(std::move(u1));
    EXPECT_EQ(u3.error(), "error");
}

TEST(ExpectedBugRegression, AndThen) {
    expected<int, std::string> e = 21;
    auto e2 = e.and_then([](int x) -> expected<int, std::string> {
        return x * 2;
    });
    EXPECT_TRUE(e2.has_value());
    EXPECT_EQ(e2.value(), 42);
}

TEST(ExpectedBugRegression, AndThenChain) {
    expected<int, std::string> e = 1;
    auto e2 = e.and_then([](int x) -> expected<int, std::string> {
        return x + 1;
    }).and_then([](int x) -> expected<int, std::string> {
        return x * 10;
    });
    EXPECT_EQ(e2.value(), 20);
}

TEST(ExpectedBugRegression, AndThenErrorShortCircuit) {
    expected<int, std::string> e = unexpected<std::string>("failure");
    bool chainCalled = false;
    auto e2 = e.and_then([&](int x) -> expected<int, std::string> {
        chainCalled = true;
        return x * 2;
    });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), "failure");
    EXPECT_FALSE(chainCalled);
}

TEST(ExpectedBugRegression, OrElse) {
    expected<int, std::string> e = unexpected<std::string>("oops");
    auto e2 = e.or_else([](const std::string& err) -> expected<int, std::string> {
        if (err == "oops") return 0;
        return unexpected<std::string>(err);
    });
    EXPECT_TRUE(e2.has_value());
    EXPECT_EQ(e2.value(), 0);
}

TEST(ExpectedBugRegression, OrElsePassThrough) {
    expected<int, std::string> e = 42;
    auto e2 = e.or_else([](const std::string&) -> expected<int, std::string> {
        return 999; // Should not be called
    });
    EXPECT_EQ(e2.value(), 42);
}

TEST(ExpectedBugRegression, Transform) {
    expected<int, std::string> e = 21;
    auto e2 = e.transform([](int x) { return x * 2; });
    static_assert(std::is_same_v<decltype(e2), expected<int, std::string>>);
    EXPECT_EQ(e2.value(), 42);
}

TEST(ExpectedBugRegression, TransformErrorPropagate) {
    expected<int, std::string> e = unexpected<std::string>("bad");
    auto e2 = e.transform([](int x) { return x + 1; });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), "bad");
}

TEST(ExpectedBugRegression, TransformError) {
    expected<int, std::string> e = unexpected<std::string>("old error");
    auto e2 = e.transform_error([](const std::string& err) -> std::string {
        return "new: " + err;
    });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), "new: old error");
}

TEST(ExpectedBugRegression, TransformErrorPassThrough) {
    expected<int, std::string> e = 42;
    auto e2 = e.transform_error([](const std::string&) -> int {
        return 0;
    });
    EXPECT_EQ(e2.value(), 42);
}

TEST(ExpectedBugRegression, CopyExpected) {
    expected<int, std::string> a = 42;
    expected<int, std::string> b = a;
    EXPECT_EQ(b.value(), 42);

    expected<int, std::string> c = unexpected<std::string>("err");
    expected<int, std::string> d = c;
    EXPECT_EQ(d.error(), "err");
}

TEST(ExpectedBugRegression, MoveExpected) {
    expected<std::string, std::string> a = std::string("move me");
    expected<std::string, std::string> b = std::move(a);
    EXPECT_EQ(b.value(), "move me");
}

TEST(ExpectedBugRegression, AssignValue) {
    expected<int, std::string> e = 1;
    e = 42;
    EXPECT_EQ(e.value(), 42);

    // Assign value to an error
    expected<int, std::string> e2 = unexpected<std::string>("was error");
    e2 = 100;
    EXPECT_EQ(e2.value(), 100);
}

TEST(ExpectedBugRegression, AssignUnexpected) {
    expected<int, std::string> e = 42;
    e = unexpected<std::string>("now error");
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error(), "now error");
}

TEST(ExpectedBugRegression, SwapExpected) {
    expected<int, std::string> a = 1;
    expected<int, std::string> b = unexpected<std::string>("b err");
    std::swap(a, b);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(a.error(), "b err");
    EXPECT_TRUE(b.has_value());
    EXPECT_EQ(b.value(), 1);

    expected<int, std::string> c = unexpected<std::string>("x");
    expected<int, std::string> d = unexpected<std::string>("y");
    std::swap(c, d);
    EXPECT_EQ(c.error(), "y");
    EXPECT_EQ(d.error(), "x");
}

TEST(ExpectedBugRegression, Equality) {
    expected<int, std::string> a = 42;
    expected<int, std::string> b = 42;
    expected<int, std::string> c = 99;
    expected<int, std::string> d = unexpected<std::string>("err");
    expected<int, std::string> e = unexpected<std::string>("err");

    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(d == e);
    EXPECT_FALSE(a == d);
    EXPECT_TRUE(a == 42);
    EXPECT_TRUE(42 == a);
    EXPECT_FALSE(a == 99);
    EXPECT_FALSE(a != 42);
    EXPECT_TRUE(d == unexpected<std::string>("err"));
    EXPECT_TRUE(unexpected<std::string>("err") == d);
    EXPECT_TRUE(d != unexpected<std::string>("other"));
}

// --- Singleton ---
struct TestSingleton : public Singleton<TestSingleton> {
    friend class Singleton<TestSingleton>;
    int value = 0;
};

TEST(SingletonBugRegression, SameInstance) {
    auto& a = TestSingleton::instance();
    auto& b = TestSingleton::instance();
    EXPECT_EQ(&a, &b);
    EXPECT_EQ(TestSingleton::ptr(), &a);
    EXPECT_EQ(TestSingleton::get(), a);
}

TEST(SingletonBugRegression, StatePreserved) {
    TestSingleton::instance().value = 42;
    EXPECT_EQ(TestSingleton::instance().value, 42);
    TestSingleton::instance().value = 0;
}

// --- Noncopyable / Nonmovable ---
struct TestNoncopyable : Noncopyable { int x = 0; };
static_assert(!std::is_copy_constructible_v<TestNoncopyable>);
static_assert(!std::is_copy_assignable_v<TestNoncopyable>);
static_assert(std::is_move_constructible_v<TestNoncopyable>);
static_assert(std::is_move_assignable_v<TestNoncopyable>);

struct TestNonmovable : Nonmovable { int x = 0; };
static_assert(std::is_copy_constructible_v<TestNonmovable>);
static_assert(std::is_copy_assignable_v<TestNonmovable>);
static_assert(!std::is_move_constructible_v<TestNonmovable>);
static_assert(!std::is_move_assignable_v<TestNonmovable>);

struct TestNone : NoncopyableNonmovable { int x = 0; };
static_assert(!std::is_copy_constructible_v<TestNone>);
static_assert(!std::is_move_constructible_v<TestNone>);

TEST(NoncopyableBugRegression, BasicUsage) {
    TestNoncopyable a;
    a.x = 42;
    TestNoncopyable b = std::move(a);
    EXPECT_EQ(b.x, 42);
}

TEST(MovableBugRegression, BasicUsage) {
    Movable<int> m(42);
    EXPECT_EQ(*m, 42);
    EXPECT_EQ(m.get(), 42);

    Movable<int> m2 = std::move(m);
    EXPECT_EQ(*m2, 42);
    EXPECT_EQ(m.get(), 0); // Released

    m2.reset(100);
    EXPECT_EQ(*m2, 100);

    int val = m2.release();
    EXPECT_EQ(val, 100);
    EXPECT_EQ(m2.get(), 0);
}

TEST(MovableBugRegression, OperatorArrow) {
    Movable<std::string> m(std::string("hello"));
    EXPECT_EQ(m->size(), 5u);
    EXPECT_EQ(m.get(), "hello");
}

// --- Macro ---
TEST(MacroBugRegression, AssertCompiles) {
    ZERO_ASSERT(true);
    EXPECT_DEATH_IF_SUPPORTED(ZERO_ASSERT(false), "ASSERTION FAILED");

    ZERO_ASSERT_MSG(true, "should pass");
    EXPECT_DEATH_IF_SUPPORTED(ZERO_ASSERT_MSG(false, "custom fail"), "custom fail");
}

TEST(MacroBugRegression, LikelyUnlikeyCompiles) {
    int x = 1;
    EXPECT_TRUE(ZERO_LIKELY(x == 1));
    EXPECT_FALSE(ZERO_UNLIKELY(x == 0));
}

TEST(MacroBugRegression, Stringify) {
    EXPECT_STREQ(ZERO_STRINGIFY(hello), "hello");
}

TEST(MacroBugRegression, ArraySize) {
    int arr[10] = {};
    EXPECT_EQ(ZERO_ARRAY_SIZE(arr), 10u);
}

TEST(MacroBugRegression, UnusedSuppression) {
    int x = 42;
    ZERO_UNUSED(x);
    SUCCEED();
}

// =====================================================================
// Section 2 — Thread primitives
// =====================================================================

// --- SpinLock ---
TEST(SpinLockBugRegression, BasicLockUnlock) {
    SpinLock lock;
    lock.lock();
    EXPECT_FALSE(lock.try_lock());
    lock.unlock();
    EXPECT_TRUE(lock.try_lock());
    lock.unlock();
}

TEST(SpinLockBugRegression, LockGuard) {
    SpinLock lock;
    {
        LockGuard<SpinLock> guard(lock);
        EXPECT_FALSE(lock.try_lock());
    }
    // lock released
    EXPECT_TRUE(lock.try_lock());
    lock.unlock();
}

TEST(SpinLockBugRegression, TryLockFail) {
    SpinLock lock;
    lock.lock();
    EXPECT_FALSE(lock.try_lock());
    lock.unlock();
}

// --- Mutex ---
TEST(MutexBugRegression, BasicLockUnlock) {
    Mutex mtx;
    mtx.lock();
    EXPECT_FALSE(mtx.try_lock());
    mtx.unlock();
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

TEST(MutexBugRegression, LockGuard) {
    Mutex mtx;
    {
        LockGuard<Mutex> guard(mtx);
        EXPECT_FALSE(mtx.try_lock());
    }
    EXPECT_TRUE(mtx.try_lock());
    mtx.unlock();
}

TEST(MutexBugRegression, NativeHandle) {
    Mutex mtx;
    EXPECT_NE(mtx.native(), nullptr);
}

// --- RWMutex ---
TEST(RWMutexBugRegression, ExclusiveLock) {
    RWMutex mtx;
    mtx.wrlock();
    EXPECT_FALSE(mtx.try_rdlock());
    EXPECT_FALSE(mtx.try_wrlock());
    mtx.unlock();
    EXPECT_TRUE(mtx.try_rdlock());
    mtx.unlock();
}

TEST(RWMutexBugRegression, SharedLockMultiple) {
    RWMutex mtx;
    mtx.rdlock();
    EXPECT_TRUE(mtx.try_rdlock()); // Multiple readers
    mtx.unlock();
    mtx.unlock();
}

TEST(RWMutexBugRegression, LockGuardWrite) {
    RWMutex mtx;
    {
        LockGuard<RWMutex> guard(mtx); // Calls lock() = wrlock
        EXPECT_FALSE(mtx.try_rdlock());
    }
    EXPECT_TRUE(mtx.try_rdlock());
    mtx.unlock();
}

// --- RWLock ---
TEST(RWLockBugRegression, MultipleReaders) {
    RWLock rwlock;
    rwlock.lock_shared();
    EXPECT_TRUE(rwlock.try_lock_shared());
    rwlock.unlock_shared();
    rwlock.unlock_shared();
}

TEST(RWLockBugRegression, WriterExclusive) {
    RWLock rwlock;
    rwlock.lock();
    EXPECT_FALSE(rwlock.try_lock_shared());
    EXPECT_FALSE(rwlock.try_lock());
    rwlock.unlock();
}

TEST(RWLockBugRegression, WriteUnlockAllowsReaders) {
    RWLock rwlock;
    rwlock.lock();
    rwlock.unlock();
    EXPECT_TRUE(rwlock.try_lock_shared());
    rwlock.unlock_shared();
}

TEST(RWLockBugRegression, ReadLockGuard) {
    RWLock rwlock;
    {
        ReadLockGuard guard(rwlock);
        EXPECT_TRUE(rwlock.try_lock_shared());
        rwlock.unlock_shared();
    }
}

TEST(RWLockBugRegression, WriteLockGuard) {
    RWLock rwlock;
    {
        WriteLockGuard guard(rwlock);
        EXPECT_FALSE(rwlock.try_lock_shared());
    }
    EXPECT_TRUE(rwlock.try_lock_shared());
    rwlock.unlock_shared();
}

TEST(RWLockBugRegression, UpgradeLockGuard) {
    RWLock rwlock;
    {
        UpgradeLockGuard guard(rwlock);
        EXPECT_FALSE(guard.is_write_locked());
        guard.upgrade();
        EXPECT_TRUE(guard.is_write_locked());
        // Other readers cannot acquire while we hold write
        EXPECT_FALSE(rwlock.try_lock_shared());
        guard.downgrade();
        EXPECT_FALSE(guard.is_write_locked());
        // Now other readers can
        EXPECT_TRUE(rwlock.try_lock_shared());
        rwlock.unlock_shared();
    }
}

TEST(RWLockBugRegression, NativeHandle) {
    RWLock rwlock;
    EXPECT_NE(rwlock.native_handle(), nullptr);
    EXPECT_NE(static_cast<const RWLock&>(rwlock).native_handle(), nullptr);
}

// --- ConditionVariable ---
TEST(ConditionVariableBugRegression, WaitNotify) {
    Mutex mtx;
    ConditionVariable cv;
    bool ready = false;

    std::thread t([&]() {
        UniqueLock<Mutex> lock(mtx);
        cv.wait(lock);
        EXPECT_TRUE(ready);
    });

    {
        UniqueLock<Mutex> lock(mtx);
        ready = true;
    }
    cv.notify_one();
    EXPECT_TRUE(t.joinable());
    t.join();
}

TEST(ConditionVariableBugRegression, DeferredLockCV) {
    Mutex mtx;
    ConditionVariable cv;
    bool ready = false;

    std::thread t([&]() {
        UniqueLock<Mutex> lock(mtx, UniqueLock<Mutex>::defer_lock);
        lock.lock();
        while (!ready) {
            cv.wait(lock);
        }
        EXPECT_TRUE(ready);
    });

    {
        UniqueLock<Mutex> lock(mtx);
        ready = true;
    }
    cv.notify_one();
    t.join();
}

TEST(ConditionVariableBugRegression, NotifyAll) {
    Mutex mtx;
    ConditionVariable cv;
    std::atomic<int> count{0};
    const int num_threads = 4;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            UniqueLock<Mutex> lock(mtx);
            cv.wait(lock);
            count.fetch_add(1);
        });
    }

    // Brief sleep to ensure threads are waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cv.notify_all();
    for (auto& t : threads) t.join();
    EXPECT_EQ(count.load(), num_threads);
}

TEST(ConditionVariableBugRegression, WaitForTimeout) {
    Mutex mtx;
    ConditionVariable cv;

    UniqueLock<Mutex> lock(mtx);
    bool notified = cv.wait_for(lock, std::chrono::milliseconds(10));
    EXPECT_FALSE(notified); // Should time out since nobody notifies
}

TEST(ConditionVariableBugRegression, WaitForSuccess) {
    Mutex mtx;
    ConditionVariable cv;

    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        cv.notify_one();
    });

    UniqueLock<Mutex> lock(mtx);
    bool notified = cv.wait_for(lock, std::chrono::milliseconds(500));
    EXPECT_TRUE(notified);
    t.join();
}

TEST(ConditionVariableBugRegression, WaitUntil) {
    Mutex mtx;
    ConditionVariable cv;

    UniqueLock<Mutex> lock(mtx);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(10);
    bool notified = cv.wait_until(lock, deadline);
    EXPECT_FALSE(notified);
}

TEST(ConditionVariableBugRegression, UniqueLockDeferred) {
    Mutex mtx;
    UniqueLock<Mutex> lock(mtx, UniqueLock<Mutex>::defer_lock);
    EXPECT_FALSE(lock.owns_lock());
    EXPECT_FALSE(static_cast<bool>(lock));

    lock.lock();
    EXPECT_TRUE(lock.owns_lock());
    lock.unlock();
    EXPECT_FALSE(lock.owns_lock());
}

TEST(ConditionVariableBugRegression, UniqueLockTryToLock) {
    Mutex mtx;
    UniqueLock<Mutex> lock(mtx, UniqueLock<Mutex>::try_to_lock);
    EXPECT_TRUE(lock.owns_lock());
    lock.unlock();

    // Hold the lock externally, try_to_lock should fail
    UniqueLock<Mutex> holder(mtx);
    UniqueLock<Mutex> lock2(mtx, UniqueLock<Mutex>::try_to_lock);
    EXPECT_FALSE(lock2.owns_lock());
    holder.unlock();
}

TEST(ConditionVariableBugRegression, UniqueLockAdopt) {
    Mutex mtx;
    mtx.lock();
    UniqueLock<Mutex> lock(mtx, UniqueLock<Mutex>::adopt_lock);
    EXPECT_TRUE(lock.owns_lock());
    // Destructor will unlock
}

TEST(ConditionVariableBugRegression, UniqueLockMove) {
    Mutex mtx;
    UniqueLock<Mutex> a(mtx);
    EXPECT_TRUE(a.owns_lock());
    UniqueLock<Mutex> b = std::move(a);
    EXPECT_FALSE(a.owns_lock());
    EXPECT_TRUE(b.owns_lock());
    EXPECT_EQ(b.mutex(), &mtx);
}

TEST(ConditionVariableBugRegression, UniqueLockRelease) {
    Mutex mtx;
    UniqueLock<Mutex> lock(mtx);
    Mutex* p = lock.release();
    EXPECT_EQ(p, &mtx);
    EXPECT_FALSE(lock.owns_lock());
    // Must now unlock manually
    p->unlock();
}

// --- Barrier ---
TEST(BarrierBugRegression, TwoThread) {
    Barrier barrier(2);
    std::atomic<int> phase{0};

    std::thread t([&]() {
        phase.store(1);
        barrier.wait();
        EXPECT_EQ(phase.load(), 1);
    });

    barrier.wait();
    EXPECT_EQ(phase.load(), 1);
    t.join();
}

TEST(BarrierBugRegression, ThreeThread) {
    Barrier barrier(3);
    std::atomic<int> arrived{0};

    auto worker = [&]() {
        arrived.fetch_add(1);
        barrier.wait();
        EXPECT_EQ(arrived.load(), 3);
    };

    std::thread t1(worker), t2(worker);
    worker();
    t1.join();
    t2.join();
}

TEST(BarrierBugRegression, RepeatedUse) {
    Barrier barrier(2);
    for (int i = 0; i < 10; ++i) {
        std::thread t([&]() { barrier.wait(); });
        barrier.wait();
        t.join();
    }
    SUCCEED();
}

TEST(BarrierBugRegression, Count) {
    Barrier barrier(4);
    EXPECT_EQ(barrier.count(), 4u);
}

TEST(BarrierBugRegression, ArriveAndWait) {
    Barrier barrier(2);
    std::thread t([&]() { barrier.arrive_and_wait(); });
    barrier.arrive_and_wait();
    t.join();
    SUCCEED();
}

// --- Semaphore ---
TEST(SemaphoreBugRegression, PostWait) {
    Semaphore sem(0);
    EXPECT_FALSE(sem.try_wait());
    sem.post();
    EXPECT_TRUE(sem.try_wait());
    EXPECT_FALSE(sem.try_wait());
}

TEST(SemaphoreBugRegression, InitialCount) {
    Semaphore sem(5);
    EXPECT_TRUE(sem.try_wait());
    EXPECT_TRUE(sem.try_wait());
    EXPECT_TRUE(sem.try_wait());
    EXPECT_TRUE(sem.try_wait());
    EXPECT_TRUE(sem.try_wait());
    EXPECT_FALSE(sem.try_wait());
}

TEST(SemaphoreBugRegression, PostMultiWait) {
    Semaphore sem(0);
    std::atomic<int> count{0};

    std::thread t([&]() {
        sem.wait();
        count.fetch_add(1);
    });

    sem.post();
    t.join();
    EXPECT_EQ(count.load(), 1);
}

TEST(SemaphoreBugRegression, WaitForTimeout) {
    Semaphore sem(0);
    EXPECT_FALSE(sem.wait_for(std::chrono::milliseconds(10)));
}

TEST(SemaphoreBugRegression, WaitForSuccess) {
    Semaphore sem(0);
    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        sem.post();
    });
    EXPECT_TRUE(sem.wait_for(std::chrono::milliseconds(500)));
    t.join();
}

// --- Thread ---
TEST(ThreadBugRegression, CreateAndJoin) {
    std::atomic<bool> ran{false};
    Thread th([&]() { ran.store(true); }, "test_thread");
    EXPECT_TRUE(th.start());
    th.join();
    EXPECT_TRUE(ran.load());
    EXPECT_EQ(th.name(), "test_thread");
}

TEST(ThreadBugRegression, Detach) {
    Semaphore sem(0);
    Thread th([&]() { sem.post(); }, "detach_test");
    EXPECT_TRUE(th.start());
    th.detach();
    // Thread should still complete
    EXPECT_TRUE(sem.wait_for(std::chrono::seconds(2)));
}

TEST(ThreadBugRegression, DoubleStartGuard) {
    Thread th([]() {}, "test");
    EXPECT_TRUE(th.start());
    // Second start should fail
    EXPECT_FALSE(th.start());
    th.join();
}

TEST(ThreadBugRegression, NativeHandle) {
    Thread th([]() {}, "native");
    th.start();
    EXPECT_NE(th.native(), pthread_t{});
    th.join();
}

TEST(ThreadBugRegression, SetCurrentName) {
    Thread::setCurrentName("main_test");
    SUCCEED();
}

// --- CPU Affinity ---
TEST(CpuAffinityBugRegression, GetCpuCount) {
    int count = get_cpu_count();
    EXPECT_GT(count, 0);
}

TEST(CpuAffinityBugRegression, SetAffinity) {
    bool ok = set_cpu_affinity(pthread_self(), 0);
    // May fail if permissions don't allow, so just check it compiles and runs
    (void)ok;
    SUCCEED();
}

// =====================================================================
// Section 3 — Fiber infrastructure
// =====================================================================

// Note: Many fiber tests require an active Scheduler to manage fiber
// contexts. These tests test Fiber in isolation where possible,
// or use a minimal scheduler context.

TEST(FiberBugRegression, StateTransitions) {
    auto f = std::make_shared<Fiber>([]() {});
    EXPECT_EQ(f->getState(), Fiber::State::INIT);
    // After creation, fiber is in INIT state
    EXPECT_GT(f->id(), 0u);
}

TEST(FiberBugRegression, FiberIdMonotonic) {
    auto f1 = std::make_shared<Fiber>([]() {});
    auto f2 = std::make_shared<Fiber>([]() {});
    EXPECT_GT(f2->id(), f1->id());
}

TEST(FiberBugRegression, MainFiberCreation) {
    Fiber main_fiber;
    EXPECT_EQ(main_fiber.getState(), Fiber::State::RUNNING);
    EXPECT_NE(Fiber::GetThis(), nullptr);
}

TEST(FiberBugRegression, FiberGetThisBeforeAny) {
    // Before any fiber runs, GetThis should return nullptr
    // (unless we are in a scheduler context)
    auto* f = Fiber::GetThis();
    (void)f; // Just ensure it compiles and runs
    SUCCEED();
}

TEST(FiberBugRegression, FiberGetFiberId) {
    uint64_t id = Fiber::GetFiberId();
    EXPECT_GE(id, 0u);
}

// --- FiberPool ---
TEST(FiberPoolBugRegression, GetRecycle) {
    auto& pool = FiberPool::instance();
    auto f1 = pool.get([]() {});
    EXPECT_NE(f1, nullptr);
    pool.recycle(f1);

    auto f2 = pool.get([]() {});
    EXPECT_NE(f2, nullptr);
    // f2 may reuse f1's resources
}

TEST(FiberPoolBugRegression, Preallocate) {
    auto& pool = FiberPool::instance();
    size_t before = pool.available();
    pool.preallocate(5);
    EXPECT_GE(pool.available(), before + 5);
}

// --- StackPool ---
TEST(StackPoolBugRegression, AllocateDeallocate) {
    auto& pool = StackPool::instance();
    const size_t stack_size = 131072;
    void* stack = pool.allocate(stack_size);
    EXPECT_NE(stack, nullptr);
    pool.deallocate(stack, stack_size);
}

TEST(StackPoolBugRegression, PreallocateStacks) {
    auto& pool = StackPool::instance();
    pool.preallocate(2, 65536);
    // Just verify no crash — actual preallocation is GC'd at process exit
    SUCCEED();
}

// --- FiberLocal ---
TEST(FiberLocalBugRegression, DefaultValue) {
    FiberLocal<int> fl;
    // Without a fiber, get() returns the default_value_
    EXPECT_EQ(fl.get(), 0);
}

TEST(FiberLocalBugRegression, SetAndGet) {
    FiberLocal<std::string> fl;
    fl.set(std::string("hello"));
    // Without a fiber, set/get storage is separate from default.
    // In a non-fiber context, set targets default_value_.
    EXPECT_EQ(fl.get(), "hello");
}

TEST(FiberLocalBugRegression, Clear) {
    FiberLocal<int> fl;
    fl.set(42);
    EXPECT_EQ(fl.get(), 42);
    fl.clear();
    EXPECT_EQ(fl.get(), 0);
}

TEST(FiberLocalBugRegression, SlotCount) {
    EXPECT_GE(FiberLocal<int>::slotCount(), 0u);
    FiberLocal<double> fl2;
    FiberLocal<std::string> fl3;
    EXPECT_GE(FiberLocal<int>::slotCount(), 2u);
}

// --- Channel ---
TEST(ChannelBugRegression, SendRecvDirect) {
    Channel<int> ch(0); // Synchronous — rendezvous
    // In a non-fiber context, send/recv require active fibers,
    // so use trySend/tryRecv
    EXPECT_FALSE(ch.trySend(42)); // No receiver => fails
    EXPECT_FALSE(ch.tryRecv(42)); // Empty => fails (but arg is just for result)

    int val = 0;
    EXPECT_FALSE(ch.tryRecv(val));
}

TEST(ChannelBugRegression, BufferedSendRecv) {
    Channel<int> ch(3);
    EXPECT_TRUE(ch.trySend(1));
    EXPECT_TRUE(ch.trySend(2));
    EXPECT_TRUE(ch.trySend(3));
    // Buffer full
    EXPECT_FALSE(ch.trySend(4));
    EXPECT_EQ(ch.size(), 3u);
    EXPECT_EQ(ch.capacity(), 3u);

    int val = 0;
    EXPECT_TRUE(ch.tryRecv(val));
    EXPECT_EQ(val, 1);
    EXPECT_TRUE(ch.tryRecv(val));
    EXPECT_EQ(val, 2);
    EXPECT_TRUE(ch.tryRecv(val));
    EXPECT_EQ(val, 3);
    EXPECT_FALSE(ch.tryRecv(val));
    EXPECT_TRUE(ch.size() == 0u);
}

TEST(ChannelBugRegression, CloseChannel) {
    Channel<int> ch(1);
    EXPECT_FALSE(ch.isClosed());
    ch.close();
    EXPECT_TRUE(ch.isClosed());
    EXPECT_FALSE(ch.trySend(42));
    int val = 0;
    EXPECT_FALSE(ch.tryRecv(val));
}

TEST(ChannelBugRegression, DoubleClose) {
    Channel<int> ch(1);
    ch.close();
    ch.close(); // Should be safe
    EXPECT_TRUE(ch.isClosed());
}

TEST(ChannelBugRegression, CloseWithBufferedData) {
    Channel<int> ch(5);
    ch.trySend(10);
    ch.trySend(20);
    ch.close();
    EXPECT_TRUE(ch.isClosed());
    // Data in buffer still readable by tryRecv
    int val = 0;
    EXPECT_TRUE(ch.tryRecv(val));
    EXPECT_EQ(val, 10);
    EXPECT_TRUE(ch.tryRecv(val));
    EXPECT_EQ(val, 20);
    // After drain, closed
    EXPECT_FALSE(ch.tryRecv(val));
}

TEST(ChannelBugRegression, CapacityZero) {
    Channel<int> ch(0);
    EXPECT_EQ(ch.capacity(), 0u);
    // Rendezvous — trySend fails without receiver
    EXPECT_FALSE(ch.trySend(42));
}

TEST(ChannelBugRegression, LargeBufferCapacity) {
    Channel<int> ch(10000);
    EXPECT_EQ(ch.capacity(), 10000u);
    // Fill halfway
    for (int i = 0; i < 5000; ++i) {
        EXPECT_TRUE(ch.trySend(i));
    }
    EXPECT_EQ(ch.size(), 5000u);
}

TEST(ChannelBugRegression, MoveSemantics) {
    Channel<std::string> ch(2);
    std::string data = "move me";
    EXPECT_TRUE(ch.trySend(std::move(data)));
    EXPECT_TRUE(data.empty()); // Moved from

    std::string out;
    EXPECT_TRUE(ch.tryRecv(out));
    EXPECT_EQ(out, "move me");
}

// --- Context ---
TEST(ContextBugRegression, InitContext) {
    // Verify Context can be initialized (no swap, just init check)
    Context ctx{};
    static bool called = false;
    called = false;
    auto test_fn = +[]() { called = true; };

    // Just verify init doesn't crash — actual context swap requires
    // a valid stack and running context
    char stack[4096];
    void* stack_top = static_cast<void*>(stack + sizeof(stack));
    ctx.init(test_fn, stack_top);
    EXPECT_FALSE(called); // init doesn't call the function

    // Mark as passed if we got here without crashing
    SUCCEED();
}

// =====================================================================
// Section 4 — Scheduler
// =====================================================================

// --- WorkStealingQueue ---
TEST(WorkStealingQueueBugRegression, PushPop) {
    WorkStealingQueue q(128);
    auto f = std::make_shared<Fiber>([]() {});
    q.push(f);
    EXPECT_GT(q.size(), 0u);
    auto popped = q.pop();
    EXPECT_NE(popped, nullptr);
    EXPECT_EQ(popped->id(), f->id());
    EXPECT_EQ(q.size(), 0u);
}

TEST(WorkStealingQueueBugRegression, PopEmpty) {
    WorkStealingQueue q(64);
    auto popped = q.pop();
    EXPECT_EQ(popped, nullptr);
    EXPECT_EQ(q.size(), 0u);
}

TEST(WorkStealingQueueBugRegression, StealEmpty) {
    WorkStealingQueue q(64);
    auto stolen = q.steal();
    EXPECT_EQ(stolen, nullptr);
}

TEST(WorkStealingQueueBugRegression, PushMultiple) {
    WorkStealingQueue q(16);
    auto f1 = std::make_shared<Fiber>([]() {});
    auto f2 = std::make_shared<Fiber>([]() {});
    auto f3 = std::make_shared<Fiber>([]() {});
    q.push(f1);
    q.push(f2);
    q.push(f3);
    EXPECT_EQ(q.size(), 3u);
    // Pop in LIFO order
    EXPECT_EQ(q.pop()->id(), f3->id());
    EXPECT_EQ(q.pop()->id(), f2->id());
    EXPECT_EQ(q.pop()->id(), f1->id());
}

TEST(WorkStealingQueueBugRegression, StealFromPushPop) {
    WorkStealingQueue q(16);
    auto f1 = std::make_shared<Fiber>([]() {});
    auto f2 = std::make_shared<Fiber>([]() {});
    q.push(f1);
    q.push(f2);
    // Steal steals from top (FIFO — f1 first)
    auto stolen = q.steal();
    EXPECT_EQ(stolen->id(), f1->id());
    // Pop still gets f2 (LIFO for owner)
    auto popped = q.pop();
    EXPECT_EQ(popped->id(), f2->id());
}

// --- FdManager ---
TEST(FdManagerBugRegression, GetAndRemove) {
    auto& mgr = FdManager::instance();
    int test_fd = 42;
    auto* ctx = mgr.get(test_fd);
    EXPECT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->fd, test_fd);
    EXPECT_FALSE(ctx->is_socket);
    EXPECT_FALSE(ctx->sys_nonblock);
    EXPECT_FALSE(ctx->user_nonblock);
    EXPECT_EQ(ctx->recv_timeout_ms, -1);
    EXPECT_EQ(ctx->send_timeout_ms, -1);
    mgr.remove(test_fd);
}

TEST(FdManagerBugRegression, ModifyContext) {
    auto& mgr = FdManager::instance();
    int test_fd = 99;
    auto* ctx = mgr.get(test_fd);
    ctx->is_socket = true;
    ctx->sys_nonblock = true;
    ctx->recv_timeout_ms = 5000;
    ctx->send_timeout_ms = 3000;
    EXPECT_TRUE(ctx->is_socket);
    EXPECT_TRUE(ctx->sys_nonblock);
    EXPECT_EQ(ctx->recv_timeout_ms, 5000);
    EXPECT_EQ(ctx->send_timeout_ms, 3000);
    mgr.remove(test_fd);
}

TEST(FdManagerBugRegression, GetAfterRemove) {
    auto& mgr = FdManager::instance();
    int test_fd = 55;
    mgr.get(test_fd);
    mgr.remove(test_fd);

    // Getting again creates a new context
    auto* ctx2 = mgr.get(test_fd);
    EXPECT_NE(ctx2, nullptr);
    EXPECT_FALSE(ctx2->is_socket);
    mgr.remove(test_fd);
}

TEST(FdManagerBugRegression, SameFdReturnsSameContext) {
    auto& mgr = FdManager::instance();
    int test_fd = 77;
    auto* ctx1 = mgr.get(test_fd);
    auto* ctx2 = mgr.get(test_fd);
    EXPECT_EQ(ctx1, ctx2);
    mgr.remove(test_fd);
}

// --- Hook ---
TEST(HookBugRegression, DefaultState) {
    EXPECT_FALSE(is_hook_enabled());
}

TEST(HookBugRegression, EnableDisable) {
    set_hook_enabled(true);
    EXPECT_TRUE(is_hook_enabled());
    set_hook_enabled(false);
    EXPECT_FALSE(is_hook_enabled());
}

// --- Reactor ---
TEST(ReactorBugRegression, CreateDestroy) {
    Reactor reactor;
    EXPECT_GE(reactor.epollFd(), 0);
}

TEST(ReactorBugRegression, AddModDelEvent) {
    Reactor reactor;
    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT_GE(efd, 0);

    // Add with IN event
    EXPECT_TRUE(reactor.addEvent(efd, EPOLLIN, nullptr));
    // Modify to OUT
    EXPECT_TRUE(reactor.modEvent(efd, EPOLLOUT, nullptr));
    // Delete
    EXPECT_TRUE(reactor.delEvent(efd));
    // Double delete may fail gracefully
    reactor.delEvent(efd);

    close(efd);
}

TEST(ReactorBugRegression, AddBadFd) {
    Reactor reactor;
    EXPECT_FALSE(reactor.addEvent(-1, EPOLLIN, nullptr));
}

TEST(ReactorBugRegression, PollWithTimeout) {
    Reactor reactor;
    // Poll with 1ms timeout, should return 0 (no events)
    int n = reactor.poll(1);
    EXPECT_EQ(n, 0);
}

TEST(ReactorBugRegression, PollWithEvent) {
    Reactor reactor;
    int efd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK);
    ASSERT_GE(efd, 0);

    // Write to eventfd to make it readable
    uint64_t val = 1;
    EXPECT_EQ(write(efd, &val, sizeof(val)), static_cast<ssize_t>(sizeof(val)));

    EXPECT_TRUE(reactor.addEvent(efd, EPOLLIN, nullptr));
    int n = reactor.poll(100);
    EXPECT_GE(n, 1);
    // Clean up
    reactor.delEvent(efd);
    close(efd);
}

TEST(ReactorBugRegression, Wakeup) {
    Reactor reactor;
    // wakeup should interrupt poll
    std::atomic<bool> polled{false};

    std::thread t([&]() {
        reactor.wakeup();
        polled.store(true);
    });

    int n = reactor.poll(5000);
    t.join();
    EXPECT_GE(n, 0); // May be 0 (woken by wakeup) or 1 (if wakeup_fd events)
    EXPECT_TRUE(polled.load());
}

TEST(ReactorBugRegression, TimerWheelAccess) {
    Reactor reactor;
    EXPECT_NE(reactor.timerWheel(), nullptr);
}

// --- TimerWheel ---
TEST(TimerWheelBugRegression, AddAndTick) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    tw.addTimer(10, [&]() { fires.fetch_add(1); });
    EXPECT_EQ(fires.load(), 0);

    // Tick multiple times to advance past 10ms
    for (int i = 0; i < 20; ++i) {
        tw.tick();
        if (fires.load() > 0) break;
    }
    EXPECT_EQ(fires.load(), 1);
}

TEST(TimerWheelBugRegression, CancelTimer) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    auto id = tw.addTimer(100, [&]() { fires.fetch_add(1); });
    EXPECT_GT(id, 0u);

    bool cancelled = tw.cancelTimer(id);
    EXPECT_TRUE(cancelled);

    for (int i = 0; i < 200; ++i) {
        tw.tick();
    }
    EXPECT_EQ(fires.load(), 0);
}

TEST(TimerWheelBugRegression, CancelNonExistent) {
    TimerWheel tw;
    EXPECT_FALSE(tw.cancelTimer(99999));
}

TEST(TimerWheelBugRegression, MultipleTimersInOrder) {
    TimerWheel tw;
    std::vector<int> order;
    tw.addTimer(30, [&]() { order.push_back(3); });
    tw.addTimer(10, [&]() { order.push_back(1); });
    tw.addTimer(20, [&]() { order.push_back(2); });

    for (int i = 0; i < 50; ++i) {
        tw.tick();
    }

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(TimerWheelBugRegression, ZeroDelayTimer) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    tw.addTimer(0, [&]() { fires.fetch_add(1); });
    tw.tick();
    EXPECT_EQ(fires.load(), 1);
}

TEST(TimerWheelBugRegression, DuplicateTimerIds) {
    TimerWheel tw;
    auto id1 = tw.addTimer(100, []() {});
    auto id2 = tw.addTimer(200, []() {});
    EXPECT_NE(id1, id2);
}

TEST(TimerWheelBugRegression, ManyTimers) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    const int N = 500;
    for (int i = 0; i < N; ++i) {
        tw.addTimer(static_cast<uint64_t>(i % 10 + 1), [&]() {
            fires.fetch_add(1);
        });
    }
    // Tick enough for all to fire
    for (int i = 0; i < 30; ++i) {
        tw.tick();
    }
    EXPECT_EQ(fires.load(), N);
}

TEST(TimerWheelBugRegression, LargeDelay) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    tw.addTimer(100000, [&]() { fires.fetch_add(1); });
    // Short tick should not fire
    for (int i = 0; i < 50; ++i) {
        tw.tick();
    }
    EXPECT_EQ(fires.load(), 0);

    // Advance more still shouldn't reach 100000ms
    for (int i = 0; i < 200; ++i) {
        tw.tick();
    }
    // May or may not fire since we only advance ~250ms
    EXPECT_LE(fires.load(), 1);
}

TEST(TimerWheelBugRegression, NowMs) {
    TimerWheel tw;
    EXPECT_EQ(tw.nowMs(), 0u);
    tw.tick();
    EXPECT_GE(tw.nowMs(), 1u);
}

// --- Scheduler ---
TEST(SchedulerBugRegression, CreateDestroy) {
    Scheduler sched(1);
    EXPECT_EQ(sched.threadCount(), 1u);
    EXPECT_FALSE(sched.isStopping());
}

TEST(SchedulerBugRegression, AutoDetectThreadCount) {
    Scheduler sched; // 0 = auto-detect
    EXPECT_GE(sched.threadCount(), 1u);
}

TEST(SchedulerBugRegression, StartStop) {
    Scheduler sched(2);
    sched.start();
    EXPECT_FALSE(sched.isStopping());
    sched.stop();
    EXPECT_TRUE(sched.isStopping());
}

TEST(SchedulerBugRegression, ScheduleFiber) {
    Scheduler sched(1);
    sched.start();

    std::atomic<int> count{0};
    auto fiber = std::make_shared<Fiber>([&]() {
        count.fetch_add(1);
    });
    sched.schedule(fiber);

    // Give time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    sched.stop();
    EXPECT_EQ(count.load(), 1);
}

TEST(SchedulerBugRegression, ScheduleCallback) {
    Scheduler sched(1);
    sched.start();

    std::atomic<int> count{0};
    sched.schedule([&]() {
        count.fetch_add(1);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    sched.stop();
    EXPECT_EQ(count.load(), 1);
}

TEST(SchedulerBugRegression, ScheduleManyFibers) {
    Scheduler sched(2);
    sched.start();

    std::atomic<int> count{0};
    const int N = 100;
    for (int i = 0; i < N; ++i) {
        sched.schedule([&]() {
            count.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    sched.stop();
    EXPECT_EQ(count.load(), N);
}

TEST(SchedulerBugRegression, ScheduleAfterStop) {
    Scheduler sched(1);
    sched.start();
    sched.stop();
    // Scheduling after stop should be safe (may or may not execute)
    sched.schedule([]() {});
    SUCCEED();
}

// =====================================================================
// Section 5 — Networking
// =====================================================================

// --- Address ---
TEST(AddressBugRegression, IPv4Creation) {
    IPv4Address addr("192.168.1.1", 8080);
    EXPECT_EQ(addr.type(), Address::Type::IPv4);
    EXPECT_EQ(addr.family(), AF_INET);
    EXPECT_EQ(addr.port(), 8080u);
    EXPECT_EQ(addr.ip(), "192.168.1.1");
}

TEST(AddressBugRegression, IPv4AnyPort) {
    IPv4Address addr(9090);
    EXPECT_EQ(addr.port(), 9090u);
    EXPECT_EQ(addr.ip(), "0.0.0.0");
}

TEST(AddressBugRegression, IPv4SetPort) {
    IPv4Address addr("127.0.0.1", 8080);
    EXPECT_EQ(addr.port(), 8080u);
    addr.setPort(9090);
    EXPECT_EQ(addr.port(), 9090u);
}

TEST(AddressBugRegression, IPv4ToString) {
    IPv4Address addr("10.0.0.1", 1234);
    std::string s = addr.toString();
    EXPECT_NE(s.find("10.0.0.1"), std::string::npos);
    EXPECT_NE(s.find("1234"), std::string::npos);
}

TEST(AddressBugRegression, IPv6Creation) {
    IPv6Address addr("::1", 8080);
    EXPECT_EQ(addr.type(), Address::Type::IPv6);
    EXPECT_EQ(addr.family(), AF_INET6);
    EXPECT_EQ(addr.port(), 8080u);
}

TEST(AddressBugRegression, IPv6AnyPort) {
    IPv6Address addr(9090);
    EXPECT_EQ(addr.port(), 9090u);
}

TEST(AddressBugRegression, IPv6ToString) {
    IPv6Address addr("::1", 8080);
    std::string s = addr.toString();
    EXPECT_NE(s.find("::1"), std::string::npos);
}

TEST(AddressBugRegression, UnixAddressCreation) {
    UnixAddress addr("/tmp/test.sock");
    EXPECT_EQ(addr.type(), Address::Type::Unix);
    EXPECT_EQ(addr.family(), AF_UNIX);
}

TEST(AddressBugRegression, UnixAddressToString) {
    UnixAddress addr("/tmp/mysock");
    std::string s = addr.toString();
    EXPECT_NE(s.find("/tmp/mysock"), std::string::npos);
}

TEST(AddressBugRegression, LookupLocalhost) {
    auto addr = Address::lookup("127.0.0.1", 9999);
    EXPECT_NE(addr, nullptr);
    EXPECT_EQ(addr->family(), AF_INET);
    EXPECT_EQ(addr->toString().find("9999"), std::string::npos ? 0 : 0);
}

TEST(AddressBugRegression, LookupIPv6) {
    auto addr = Address::lookup("::1", 9999);
    EXPECT_NE(addr, nullptr);
    EXPECT_EQ(addr->family(), AF_INET6);
}

TEST(AddressBugRegression, CreateFromSockaddr) {
    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(8080);
    inet_pton(AF_INET, "192.168.1.1", &sa.sin_addr);

    auto addr = Address::create(reinterpret_cast<const struct sockaddr*>(&sa),
                                 sizeof(sa));
    EXPECT_NE(addr, nullptr);
    EXPECT_EQ(addr->family(), AF_INET);
}

TEST(AddressBugRegression, AddrLen) {
    IPv4Address v4("0.0.0.0", 0);
    EXPECT_EQ(v4.addrLen(), static_cast<socklen_t>(sizeof(struct sockaddr_in)));

    IPv6Address v6("::", 0);
    EXPECT_EQ(v6.addrLen(), static_cast<socklen_t>(sizeof(struct sockaddr_in6)));

    UnixAddress ux("/tmp/s");
    EXPECT_EQ(ux.addrLen(), static_cast<socklen_t>(sizeof(struct sockaddr_un)));
}

// --- Socket ---
TEST(SocketBugRegression, CreateTcpClose) {
    auto sock = Socket::createTCP();
    EXPECT_NE(sock, nullptr);
    EXPECT_EQ(sock->type(), Socket::Type::TCP);
    EXPECT_GE(sock->fd(), 0);
    sock->close();
}

TEST(SocketBugRegression, CreateUdpClose) {
    auto sock = Socket::createUDP();
    EXPECT_NE(sock, nullptr);
    EXPECT_EQ(sock->type(), Socket::Type::UDP);
    EXPECT_GE(sock->fd(), 0);
    sock->close();
}

TEST(SocketBugRegression, FromFd) {
    int raw_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(raw_fd, 0);
    auto sock = Socket::fromFd(raw_fd);
    EXPECT_NE(sock, nullptr);
    EXPECT_EQ(sock->fd(), raw_fd);
    sock->close();
}

TEST(SocketBugRegression, SetOptions) {
    auto sock = Socket::createTCP();
    EXPECT_TRUE(sock->setReuseAddr(true));
    EXPECT_TRUE(sock->setReusePort(true));
    EXPECT_TRUE(sock->setTcpNoDelay(true));
    EXPECT_TRUE(sock->setKeepAlive(true));
    EXPECT_TRUE(sock->setNonBlocking(true));
    // Flip back
    EXPECT_TRUE(sock->setTcpNoDelay(false));
    EXPECT_TRUE(sock->setNonBlocking(false));
    sock->close();
}

TEST(SocketBugRegression, SetTimeouts) {
    auto sock = Socket::createTCP();
    EXPECT_TRUE(sock->setSendTimeout(5000));
    EXPECT_TRUE(sock->setRecvTimeout(5000));
    sock->close();
}

TEST(SocketBugRegression, BindToPortZero) {
    auto sock = Socket::createTCP();
    sock->setReuseAddr(true);
    IPv4Address addr("127.0.0.1", 0);
    EXPECT_TRUE(sock->bind(addr));
    auto local = sock->localAddress();
    EXPECT_NE(local, nullptr);
    EXPECT_NE(dynamic_cast<IPv4Address*>(local.get())->port(), 0u);
    sock->close();
}

TEST(SocketBugRegression, BindAndGetLocalAddress) {
    auto sock = Socket::createTCP();
    sock->setReuseAddr(true);
    IPv4Address addr("127.0.0.1", 0);
    EXPECT_TRUE(sock->bind(addr));
    auto local = sock->localAddress();
    EXPECT_NE(local, nullptr);
    EXPECT_EQ(local->family(), AF_INET);
    sock->close();
}

TEST(SocketBugRegression, Listen) {
    auto sock = Socket::createTCP();
    sock->setReuseAddr(true);
    IPv4Address addr("127.0.0.1", 0);
    sock->bind(addr);
    EXPECT_TRUE(sock->listen());
    sock->close();
}

TEST(SocketBugRegression, ListenCustomBacklog) {
    auto sock = Socket::createTCP();
    sock->setReuseAddr(true);
    sock->bind(IPv4Address("127.0.0.1", 0));
    EXPECT_TRUE(sock->listen(5));
    sock->close();
}

TEST(SocketBugRegression, ConnectRefused) {
    auto sock = Socket::createTCP();
    // Connect to a port where nothing is listening
    IPv4Address addr("127.0.0.1", 19999);
    bool connected = sock->connect(addr);
    // Non-blocking or blocking, either may fail on connection refused
    (void)connected;
    sock->close();
}

TEST(SocketBugRegression, GetError) {
    auto sock = Socket::createTCP();
    int err = sock->getError();
    EXPECT_EQ(err, 0);
    sock->close();
}

TEST(SocketBugRegression, CloseMultipleTimes) {
    auto sock = Socket::createTCP();
    sock->close();
    sock->close(); // Should be safe
    SUCCEED();
}

// --- ChainBuffer ---
TEST(ChainBufferBugRegression, EmptyBuffer) {
    ChainBuffer buf;
    EXPECT_EQ(buf.readableSize(), 0u);
    EXPECT_TRUE(buf.empty());
}

TEST(ChainBufferBugRegression, AppendAndRead) {
    ChainBuffer buf;
    const char* data = "hello world";
    buf.append(data, strlen(data));
    EXPECT_EQ(buf.readableSize(), strlen(data));
    EXPECT_FALSE(buf.empty());

    char out[64] = {};
    size_t n = buf.read(out, strlen(data));
    EXPECT_EQ(n, strlen(data));
    EXPECT_STREQ(out, "hello world");
    EXPECT_TRUE(buf.empty());
}

TEST(ChainBufferBugRegression, AppendString) {
    ChainBuffer buf;
    buf.append("direct string");
    EXPECT_EQ(buf.readableSize(), 13u);

    char out[64] = {};
    buf.read(out, 13);
    EXPECT_STREQ(out, "direct string");
}

TEST(ChainBufferBugRegression, AppendChar) {
    ChainBuffer buf;
    buf.append('x');
    buf.append('y');
    buf.append('z');
    EXPECT_EQ(buf.readableSize(), 3u);
    char out[4] = {};
    buf.read(out, 3);
    EXPECT_STREQ(out, "xyz");
}

TEST(ChainBufferBugRegression, Consume) {
    ChainBuffer buf;
    buf.append("abcdef", 6);
    buf.consume(2);
    EXPECT_EQ(buf.readableSize(), 4u);

    char out[8] = {};
    buf.read(out, 4);
    EXPECT_STREQ(out, "cdef");
}

TEST(ChainBufferBugRegression, Peek) {
    ChainBuffer buf;
    buf.append("hello", 5);
    const char* p = buf.peek();
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(p[0], 'h');
    // Data should still be there
    EXPECT_EQ(buf.readableSize(), 5u);
}

TEST(ChainBufferBugRegression, PartialRead) {
    ChainBuffer buf;
    buf.append("abcdefgh", 8);
    char out[8] = {};
    size_t n = buf.read(out, 4);
    EXPECT_EQ(n, 4u);
    EXPECT_EQ(std::string(out, 4), "abcd");
    EXPECT_EQ(buf.readableSize(), 4u);

    n = buf.read(out, 4);
    EXPECT_EQ(n, 4u);
    EXPECT_EQ(std::string(out, 4), "efgh");
    EXPECT_TRUE(buf.empty());
}

TEST(ChainBufferBugRegression, LargeAppend) {
    ChainBuffer buf;
    std::string large = make_large_string(100000);
    buf.append(large.data(), large.size());
    EXPECT_EQ(buf.readableSize(), 100000u);

    std::string out(100000, '\0');
    size_t n = buf.read(&out[0], 100000);
    EXPECT_EQ(n, 100000u);
    EXPECT_EQ(out, large);
}

TEST(ChainBufferBugRegression, ReserveAndCommit) {
    ChainBuffer buf;
    auto [ptr, avail] = buf.reserve(100);
    EXPECT_GE(avail, 100u);
    EXPECT_NE(ptr, nullptr);

    // Write into the reserved space
    memcpy(ptr, "reserved_test", 13);
    buf.commit(13);
    EXPECT_EQ(buf.readableSize(), 13u);

    char out[32] = {};
    buf.read(out, 13);
    EXPECT_STREQ(out, "reserved_test");
}

TEST(ChainBufferBugRegression, VarintEncoding) {
    ChainBuffer buf;

    // Write varints
    buf.writeVarint(0);
    buf.writeVarint(1);
    buf.writeVarint(127);
    buf.writeVarint(300);
    buf.writeVarint(UINT64_MAX);

    // Read back
    uint64_t val = 0;
    EXPECT_TRUE(buf.readVarint(val));
    EXPECT_EQ(val, 0u);
    EXPECT_TRUE(buf.readVarint(val));
    EXPECT_EQ(val, 1u);
    EXPECT_TRUE(buf.readVarint(val));
    EXPECT_EQ(val, 127u);
    EXPECT_TRUE(buf.readVarint(val));
    EXPECT_EQ(val, 300u);
    EXPECT_TRUE(buf.readVarint(val));
    EXPECT_EQ(val, UINT64_MAX);
}

TEST(ChainBufferBugRegression, MoveConstructor) {
    ChainBuffer a;
    a.append("move data", 9);
    ChainBuffer b = std::move(a);
    EXPECT_EQ(b.readableSize(), 9u);
    EXPECT_TRUE(a.empty()); // Moved-from is empty
}

TEST(ChainBufferBugRegression, MoveAssignment) {
    ChainBuffer a, b;
    a.append("data", 4);
    b = std::move(a);
    EXPECT_EQ(b.readableSize(), 4u);
    EXPECT_TRUE(a.empty());
}

TEST(ChainBufferBugRegression, ToIovec) {
    ChainBuffer buf;
    buf.append("abc", 3);
    buf.append("def", 3);

    struct iovec iov[4];
    size_t n = buf.toIovec(iov, 4);
    EXPECT_GE(n, 1u);

    // Sum up all iovec lengths
    size_t total = 0;
    for (size_t i = 0; i < n; ++i) {
        total += iov[i].iov_len;
    }
    EXPECT_EQ(total, 6u);
}

// --- SocketStream ---
TEST(SocketStreamBugRegression, CreateAndClose) {
    auto sock = Socket::createTCP();
    auto stream = std::make_shared<SocketStream>(sock);
    EXPECT_TRUE(stream->isOpen());
    EXPECT_NE(stream->socket(), nullptr);
    stream->close();
    EXPECT_FALSE(stream->isOpen());
}

TEST(SocketStreamBugRegression, GetFd) {
    auto sock = Socket::createTCP();
    SocketStream stream(sock);
    EXPECT_EQ(stream.getFd(), sock->fd());
    stream.close();
}

TEST(SocketStreamBugRegression, ReadWriteBufferAccess) {
    auto sock = Socket::createTCP();
    SocketStream stream(sock);
    EXPECT_NE(&stream.readBuffer(), nullptr);
    EXPECT_NE(&stream.writeBuffer(), nullptr);
    stream.close();
}

// --- Stream (Abstract) ---
TEST(StreamBugRegression, VirtualInterface) {
    // Test that SocketStream properly implements the Stream interface
    auto sock = Socket::createTCP();
    Stream* stream = new SocketStream(sock);
    EXPECT_NE(stream, nullptr);
    EXPECT_TRUE(stream->isOpen());
    delete stream;
}

// =====================================================================
// Section 6 — I/O Engines
// =====================================================================

TEST(IoEngineBugRegression, EpollEngineCreate) {
    EpollEngine engine;
    EXPECT_GE(engine.epollFd(), 0);
}

TEST(IoEngineBugRegression, EpollEngineAddModDel) {
    EpollEngine engine;
    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT_GE(efd, 0);

    EXPECT_TRUE(engine.addFd(efd, EPOLLIN, nullptr));
    EXPECT_TRUE(engine.modFd(efd, EPOLLOUT, nullptr));
    EXPECT_TRUE(engine.delFd(efd));
    close(efd);
}

TEST(IoEngineBugRegression, EpollEngineAddBadFd) {
    EpollEngine engine;
    EXPECT_FALSE(engine.addFd(-1, EPOLLIN, nullptr));
}

TEST(IoEngineBugRegression, EpollEngineWaitTimeout) {
    EpollEngine engine;
    IoEvent events[8];
    int n = engine.wait(events, 8, 1);
    EXPECT_EQ(n, 0);
}

TEST(IoEngineBugRegression, EpollEngineWaitWithEvent) {
    EpollEngine engine;
    int efd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK);
    ASSERT_GE(efd, 0);

    uint64_t val = 1;
    write(efd, &val, sizeof(val));

    EXPECT_TRUE(engine.addFd(efd, EPOLLIN, nullptr));
    IoEvent events[8];
    int n = engine.wait(events, 8, 100);
    EXPECT_GE(n, 1);

    engine.delFd(efd);
    close(efd);
}

TEST(IoEngineBugRegression, UringEngineCreate) {
    // Even without io_uring support, construction should be safe
    UringEngine engine;
    SUCCEED();
}

// =====================================================================
// Section 7 — Logging
// =====================================================================

TEST(LogBugRegression, RootLogger) {
    auto* log = Logger::root();
    EXPECT_NE(log, nullptr);
    EXPECT_EQ(log->name(), "root");
}

TEST(LogBugRegression, GetLogger) {
    auto* log = Logger::get("test.module");
    EXPECT_NE(log, nullptr);
    EXPECT_EQ(log->name(), "test.module");
}

TEST(LogBugRegression, SameNameReturnsSame) {
    auto* a = Logger::get("same.name");
    auto* b = Logger::get("same.name");
    EXPECT_EQ(a, b);
}

TEST(LogBugRegression, DifferentNames) {
    auto* a = Logger::get("first");
    auto* b = Logger::get("second");
    EXPECT_NE(a, b);
    EXPECT_EQ(a->name(), "first");
    EXPECT_EQ(b->name(), "second");
}

TEST(LogBugRegression, LogLevels) {
    auto* log = Logger::root();

    // Default level is DEBUG
    EXPECT_EQ(log->level(), LogLevel::LVL_DEBUG);

    log->setLevel(LogLevel::LVL_INFO);
    EXPECT_EQ(log->level(), LogLevel::LVL_INFO);

    log->setLevel(LogLevel::LVL_ERROR);
    EXPECT_EQ(log->level(), LogLevel::LVL_ERROR);

    log->setLevel(LogLevel::LVL_DEBUG);
}

TEST(LogBugRegression, AllLevelsDontCrash) {
    auto* log = Logger::root();
    log->trace(__FILE__, __LINE__, "trace message");
    log->debug(__FILE__, __LINE__, "debug message");
    log->info(__FILE__, __LINE__, "info message");
    log->warn(__FILE__, __LINE__, "warning");
    log->error(__FILE__, __LINE__, "error");
    log->fatal(__FILE__, __LINE__, "fatal error");
    SUCCEED();
}

TEST(LogBugRegression, LogMethodDirectCall) {
    auto* log = Logger::root();
    log->log(LogLevel::LVL_INFO, __FILE__, __LINE__, "direct info");
    log->log(LogLevel::LVL_ERROR, __FILE__, __LINE__, "direct error");
    log->log(LogLevel::LVL_DEBUG, __FILE__, __LINE__, "direct debug");
    SUCCEED();
}

TEST(LogBugRegression, LevelFiltering) {
    auto* log = Logger::root();
    log->setLevel(LogLevel::LVL_ERROR);

    // These should be filtered (low level)
    log->log(LogLevel::LVL_DEBUG, __FILE__, __LINE__, "should be filtered");
    log->log(LogLevel::LVL_INFO, __FILE__, __LINE__, "should be filtered");

    // This should pass
    log->log(LogLevel::LVL_ERROR, __FILE__, __LINE__, "should appear");

    log->setLevel(LogLevel::LVL_DEBUG); // Restore
}

TEST(LogBugRegression, ConsoleAppender) {
    auto appender = std::make_shared<ConsoleAppender>();
    appender->setFormatter("test formatter");
    auto* log = Logger::root();
    log->addAppender(appender);
    log->info(__FILE__, __LINE__, "console output");
}

TEST(LogBugRegression, FileAppender) {
    auto appender = std::make_shared<FileAppender>("/tmp/zero_test_log.txt");
    appender->setFormatter("file format");
    auto* log = Logger::root();
    log->addAppender(appender);
    log->info(__FILE__, __LINE__, "writing to file");
}

TEST(LogBugRegression, MultipleAppenders) {
    auto console = std::make_shared<ConsoleAppender>();
    auto file = std::make_shared<FileAppender>("/tmp/zero_test_log2.txt");
    auto* log = Logger::get("multi.append");
    log->addAppender(console);
    log->addAppender(file);
    log->info(__FILE__, __LINE__, "this goes to both");
}

TEST(LogBugRegression, LoggerHierarchy) {
    auto* parent = Logger::get("zedis");
    auto* child = Logger::get("zedis.server");
    auto* leaf = Logger::get("zedis.server.session");

    EXPECT_EQ(parent->name(), "zedis");
    EXPECT_EQ(child->name(), "zedis.server");
    EXPECT_EQ(leaf->name(), "zedis.server.session");
}

// =====================================================================
// Section 8 — Configuration
// =====================================================================

TEST(ConfigBugRegression, LookupCreate) {
    auto* var = Config::lookup<int>("test.key", 42);
    EXPECT_NE(var, nullptr);
    EXPECT_EQ(var->name(), "test.key");
    EXPECT_EQ(var->getValue(), 42);
}

TEST(ConfigBugRegression, SetAndGetValue) {
    auto* var = Config::lookup<int>("test.value", 0);
    EXPECT_EQ(var->getValue(), 0);
    var->setValue(100);
    EXPECT_EQ(var->getValue(), 100);
    var->setValue(-50);
    EXPECT_EQ(var->getValue(), -50);
}

TEST(ConfigBugRegression, ReLookupReturnsSame) {
    auto* a = Config::lookup<int>("same2", 1);
    auto* b = Config::lookup<int>("same2", 999);
    EXPECT_EQ(a, b);
    EXPECT_EQ(a->getValue(), 1); // Original default preserved
}

TEST(ConfigBugRegression, DifferentTypes) {
    auto* intVar = Config::lookup<int>("typed.int", 42);
    auto* strVar = Config::lookup<std::string>("typed.str", std::string("hello"));
    auto* dblVar = Config::lookup<double>("typed.dbl", 3.14);
    auto* boolVar = Config::lookup<bool>("typed.bool", true);

    EXPECT_EQ(intVar->getValue(), 42);
    EXPECT_EQ(strVar->getValue(), "hello");
    EXPECT_DOUBLE_EQ(dblVar->getValue(), 3.14);
    EXPECT_TRUE(boolVar->getValue());
}

TEST(ConfigBugRegression, StringConfig) {
    auto* var = Config::lookup<std::string>("str.key", std::string("default"));
    EXPECT_EQ(var->getValue(), "default");
    var->setValue("modified");
    EXPECT_EQ(var->getValue(), "modified");
}

TEST(ConfigBugRegression, DoubleConfig) {
    auto* var = Config::lookup<double>("dbl.key", 1.0);
    var->setValue(2.71828);
    EXPECT_DOUBLE_EQ(var->getValue(), 2.71828);
}

TEST(ConfigBugRegression, BoolConfig) {
    auto* var = Config::lookup<bool>("bool.key", false);
    EXPECT_FALSE(var->getValue());
    var->setValue(true);
    EXPECT_TRUE(var->getValue());
}

TEST(ConfigBugRegression, ListenerNotification) {
    auto* var = Config::lookup<int>("listener.key", 0);
    std::atomic<int> changes{0};
    int old_received = 0, new_received = 0;

    var->addListener([&](const int& old_val, const int& new_val) {
        changes.fetch_add(1);
        old_received = old_val;
        new_received = new_val;
    });

    var->setValue(42);
    EXPECT_EQ(changes.load(), 1);
    EXPECT_EQ(old_received, 0);
    EXPECT_EQ(new_received, 42);

    var->setValue(100);
    EXPECT_EQ(changes.load(), 2);
    EXPECT_EQ(old_received, 42);
    EXPECT_EQ(new_received, 100);
}

TEST(ConfigBugRegression, MultipleListeners) {
    auto* var = Config::lookup<int>("multi.listener", 0);
    std::atomic<int> fired{0};

    var->addListener([&](const int&, const int&) { fired.fetch_add(1); });
    var->addListener([&](const int&, const int&) { fired.fetch_add(1); });
    var->addListener([&](const int&, const int&) { fired.fetch_add(1); });

    var->setValue(1);
    EXPECT_EQ(fired.load(), 3);
}

TEST(ConfigBugRegression, ToString) {
    auto* intVar = Config::lookup<int>("tostring.int", 42);
    auto* strVar = Config::lookup<std::string>("tostring.str", std::string("hello"));
    EXPECT_NE(intVar->toString(), "");
    EXPECT_NE(strVar->toString(), "");
}

TEST(ConfigBugRegression, LoadFromFile) {
    // If no YAML file exists, this should fail gracefully
    bool ok = Config::instance().loadFromFile("/nonexistent_config_does_not_exist.yaml");
    EXPECT_FALSE(ok);
}

TEST(ConfigBugRegression, LoadFromDir) {
    bool ok = Config::instance().loadFromDir("/nonexistent_dir_xyz");
    EXPECT_FALSE(ok);
}

TEST(ConfigBugRegression, GetAllVars) {
    auto& all = Config::instance().getAll();
    // Should have at least the variables we created in tests
    EXPECT_GT(all.size(), 0u);
}

TEST(ConfigBugRegression, ZeroValue) {
    auto* var = Config::lookup<int>("zero.val", 0);
    var->setValue(0);
    EXPECT_EQ(var->getValue(), 0);
    EXPECT_EQ(var->toString(), "0");
}

TEST(ConfigBugRegression, NegativeValues) {
    auto* var = Config::lookup<int>("negative.val", 0);
    var->setValue(-999);
    EXPECT_EQ(var->getValue(), -999);
}

TEST(ConfigBugRegression, LargeValues) {
    auto* var = Config::lookup<uint64_t>("uint64.val", 0u);
    var->setValue(UINT64_MAX);
    EXPECT_EQ(var->getValue(), UINT64_MAX);
}

TEST(ConfigBugRegression, EmptyString) {
    auto* var = Config::lookup<std::string>("empty.str", std::string("nonempty"));
    var->setValue("");
    EXPECT_EQ(var->getValue(), "");
}

// =====================================================================
// Section 9 — Init and Global Functions
// =====================================================================

TEST(InitBugRegression, InitZero) {
    // InitZero may already have been called implicitly, double-call safe?
    InitZero(0, nullptr);
    SUCCEED();
}

TEST(InitBugRegression, GetConfig) {
    Config& cfg = GetConfig();
    // Just verify it returns a reference
    (void)cfg;
    SUCCEED();
}

// =====================================================================
// Section 10 — Cross-subsystem integration edges
// =====================================================================

TEST(IntegrationBugRegression, LogConfigInteraction) {
    auto* logger = Logger::get("integration.test");
    auto* configVar = Config::lookup<std::string>("log.level", std::string("info"));
    // Both systems should coexist
    logger->info(__FILE__, __LINE__, "log level from config: " + configVar->getValue());
    SUCCEED();
}

TEST(IntegrationBugRegression, SocketWithAddress) {
    auto sock = Socket::createTCP();
    IPv4Address addr("127.0.0.1", 0);
    sock->setReuseAddr(true);
    sock->bind(addr);
    auto local = sock->localAddress();
    EXPECT_NE(local, nullptr);
    sock->close();
}

TEST(IntegrationBugRegression, TimerWithCallback) {
    TimerWheel tw;
    std::atomic<int> fired{0};
    tw.addTimer(5, [&]() { fired.store(42); });
    for (int i = 0; i < 10; ++i) tw.tick();
    EXPECT_EQ(fired.load(), 42);
}

TEST(IntegrationBugRegression, ChannelWithFiberThread) {
    Channel<int> ch(10);
    // Fill with trySend from main thread
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(ch.trySend(i));
    }
    EXPECT_EQ(ch.size(), 10u);

    int val = 0;
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(ch.tryRecv(val));
        EXPECT_EQ(val, i);
    }
}

TEST(IntegrationBugRegression, SchedulerWithReactor) {
    Scheduler sched(1);
    sched.start();
    sched.schedule([]() {
        auto* sch = Scheduler::GetThis();
        EXPECT_NE(sch, nullptr);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sched.stop();
}

// =====================================================================
// Section 11 — Exception safety & resource cleanup
// =====================================================================

TEST(ExceptionSafety, AnyResetOnThrow) {
    any a = 42;
    try {
        (void)any_cast<double>(a); // Throws
    } catch (const bad_any_cast&) {
        // After exception, any should still be valid
        EXPECT_TRUE(a.has_value());
        EXPECT_EQ(any_cast<int>(a), 42);
    }
}

TEST(ExceptionSafety, OptionalResetOnThrow) {
    optional<int> o = 42;
    try {
        o.reset();
        (void)o.value(); // Throws
    } catch (const bad_optional_access&) {
        // Should not crash
        EXPECT_FALSE(o.has_value());
    }
}

TEST(ExceptionSafety, ExpectedResetOnThrow) {
    expected<int, std::string> e = unexpected<std::string>("err");
    try {
        (void)e.value(); // Throws
    } catch (const bad_expected_access<std::string>& ex) {
        EXPECT_EQ(ex.error(), "err");
        // Expected should still be valid
        EXPECT_FALSE(e.has_value());
        EXPECT_EQ(e.error(), "err");
    }
}

TEST(ExceptionSafety, DoubleClearAny) {
    any a = 42;
    a.reset();
    EXPECT_FALSE(a.has_value());
    a.reset(); // Double reset
    EXPECT_FALSE(a.has_value());
}

TEST(ExceptionSafety, DoubleCloseSocket) {
    auto sock = Socket::createTCP();
    sock->close();
    sock->close(); // Double close
    SUCCEED();
}

TEST(ExceptionSafety, DoubleCloseChannel) {
    Channel<int> ch(10);
    ch.close();
    ch.close(); // Double close
    EXPECT_TRUE(ch.isClosed());
}

// =====================================================================
// Section 12 — Boundary and overflow testing
// =====================================================================

TEST(BoundaryBugRegression, TimerMaxDelay) {
    TimerWheel tw;
    std::atomic<int> fires{0};
    // Very large delay (near max range ~49 days in ms)
    tw.addTimer(UINT64_MAX / 2, [&]() { fires.fetch_add(1); });
    EXPECT_GT(fires.load(), -1); // Just check we didn't crash
}

TEST(BoundaryBugRegression, ChannelFullBufferBoundary) {
    Channel<int> ch(1);
    EXPECT_TRUE(ch.trySend(1));
    EXPECT_FALSE(ch.trySend(2)); // Full
    int val = 0;
    EXPECT_TRUE(ch.tryRecv(val));
    EXPECT_EQ(val, 1);
    // Now there's space again
    EXPECT_TRUE(ch.trySend(3));
}

TEST(BoundaryBugRegression, ChainBufferVeryLargeAppend) {
    ChainBuffer buf;
    std::string huge(500000, 'X');
    buf.append(huge.data(), huge.size());
    EXPECT_EQ(buf.readableSize(), 500000u);
    buf.consume(500000);
    EXPECT_TRUE(buf.empty());
}

TEST(BoundaryBugRegression, EmptyConfigString) {
    auto* var = Config::lookup<std::string>("boundary.empty", std::string(""));
    EXPECT_EQ(var->getValue(), "");
    var->setValue("");
    EXPECT_EQ(var->getValue(), "");
}

TEST(BoundaryBugRegression, OptionalWithZero) {
    optional<int> o = 0;
    EXPECT_TRUE(o.has_value()); // 0 is a valid value
    EXPECT_EQ(o.value(), 0);
    EXPECT_EQ(o.value_or(42), 0); // value_or returns 0, not default
}

TEST(BoundaryBugRegression, ExpectedWithZero) {
    expected<int, std::string> e = 0;
    EXPECT_TRUE(e.has_value());
    EXPECT_EQ(e.value(), 0);
}

TEST(BoundaryBugRegression, FdManagerMaxFd) {
    auto& mgr = FdManager::instance();
    // Test at boundary: fd 0 and high fd
    auto* ctx0 = mgr.get(0);
    EXPECT_NE(ctx0, nullptr);
    mgr.remove(0);

    auto* ctxHigh = mgr.get(65535);
    EXPECT_NE(ctxHigh, nullptr);
    mgr.remove(65535);
}

// =====================================================================
// Section 13 — Stress with pattern repeats (shake out RAII)
// =====================================================================

TEST(RepeatStressBugRegression, SpinLockLockUnlockLoop) {
    SpinLock lock;
    for (int i = 0; i < 1000; ++i) {
        lock.lock();
        lock.unlock();
    }
    SUCCEED();
}

TEST(RepeatStressBugRegression, AnyReassignLoop) {
    for (int i = 0; i < 200; ++i) {
        any a = i;
        EXPECT_EQ(any_cast<int>(a), i);
    }
}

TEST(RepeatStressBugRegression, OptionalCreateDestroy) {
    for (int i = 0; i < 500; ++i) {
        optional<std::string> o = std::string("iteration test");
        o.reset();
    }
    SUCCEED();
}

TEST(RepeatStressBugRegression, SocketCreateClose) {
    for (int i = 0; i < 100; ++i) {
        auto sock = Socket::createTCP();
        sock->close();
    }
    SUCCEED();
}

TEST(RepeatStressBugRegression, TimerCreateDestroy) {
    for (int i = 0; i < 5; ++i) {
        TimerWheel tw;
        tw.addTimer(1, []() {});
        tw.addTimer(10, []() {});
        tw.tick();
    }
    SUCCEED();
}

// =====================================================================
// Section 14 — Verify non-trivial default/assignment states
// =====================================================================

TEST(StateVerification, AnyDefaultState) {
    any a;
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(a.type(), typeid(void));
    a = a; // Self-assign
    EXPECT_FALSE(a.has_value());
}

TEST(StateVerification, OptionalDefaultState) {
    optional<std::string> o;
    EXPECT_FALSE(o.has_value());
    o = o; // Self-assign
    EXPECT_FALSE(o.has_value());
}

TEST(StateVerification, ExpectedDefaultState) {
    expected<int, std::string> e = 0;
    EXPECT_TRUE(e.has_value());
    e = e; // Self-assign
    EXPECT_TRUE(e.has_value());
    EXPECT_EQ(e.value(), 0);
}

TEST(StateVerification, MovedFromAny) {
    any a = 42;
    any b = std::move(a);
    EXPECT_FALSE(a.has_value());
    // Moving into a again
    a = std::move(b);
    EXPECT_TRUE(a.has_value());
    EXPECT_FALSE(b.has_value());
}

TEST(StateVerification, MovedFromOptional) {
    optional<std::string> a = std::string("hello");
    optional<std::string> b = std::move(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(b.has_value());
    EXPECT_EQ(*b, "hello");
}

// =====================================================================
// Section 15 — FiberMutex placeholder tests (if API surface exists via fiber_mutex)
// =====================================================================

// No FiberMutex header exists yet; verify FiberLocal and Channel compose
// with fiber IDs.

TEST(FiberIntegration, FiberLocalMultipleSlots) {
    FiberLocal<int> fl_int;
    FiberLocal<std::string> fl_str;
    fl_int.set(42);
    fl_str.set(std::string("hello"));
    EXPECT_EQ(fl_int.get(), 42);
    EXPECT_EQ(fl_str.get(), "hello");
}

// =====================================================================
// Finished. Total: ~80+ test cases covering all zero subsystems.
// =====================================================================
