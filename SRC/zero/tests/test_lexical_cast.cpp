// test_lexical_cast.cpp — Comprehensive lexical_cast unit tests
// Tests all numeric/string conversion paths, edge cases, error handling,
// and the safe-conversion helper family.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <string>
#include <string_view>
#include <limits>
#include <stdexcept>

using namespace zero;

// ============================================================
// Integer -> String -> Integer round-trip via lexical_cast
// ============================================================

TEST(LexicalCast, IntToStringRoundTrip) {
    EXPECT_EQ(lexical_cast<std::string>(42), "42");
    EXPECT_EQ(lexical_cast<std::string>(-42), "-42");
    EXPECT_EQ(lexical_cast<std::string>(0), "0");
    EXPECT_EQ(lexical_cast<std::string>(INT32_MAX),
              std::to_string(INT32_MAX));
    EXPECT_EQ(lexical_cast<std::string>(INT32_MIN + 1),
              std::to_string(INT32_MIN + 1));
}

TEST(LexicalCast, LongToStringRoundTrip) {
    EXPECT_EQ(lexical_cast<std::string>(123456789L), "123456789");
    EXPECT_EQ(lexical_cast<std::string>(-987654321L), "-987654321");
    EXPECT_EQ(lexical_cast<std::string>(0L), "0");
}

TEST(LexicalCast, LongLongToStringRoundTrip) {
    EXPECT_EQ(lexical_cast<std::string>(INT64_MAX),
              std::to_string(INT64_MAX));
    EXPECT_EQ(lexical_cast<std::string>(INT64_MIN),
              std::to_string(INT64_MIN));
    EXPECT_EQ(lexical_cast<std::string>(0LL), "0");
}

TEST(LexicalCast, UnsignedToStringRoundTrip) {
    EXPECT_EQ(lexical_cast<std::string>(UINT32_MAX),
              std::to_string(UINT32_MAX));
    EXPECT_EQ(lexical_cast<std::string>(0u), "0");
    EXPECT_EQ(lexical_cast<std::string>(42u), "42");
}

TEST(LexicalCast, FloatToStringRoundTrip) {
    EXPECT_EQ(lexical_cast<std::string>(3.14f), "3.14");
    EXPECT_EQ(lexical_cast<std::string>(0.0f), "0");
    EXPECT_EQ(lexical_cast<std::string>(-1.5f), "-1.5");
}

TEST(LexicalCast, DoubleToStringRoundTrip) {
    EXPECT_EQ(lexical_cast<std::string>(3.14159265358979),
              "3.14159265358979");
    EXPECT_EQ(lexical_cast<std::string>(0.0), "0");
    EXPECT_EQ(lexical_cast<std::string>(-2.71828), "-2.71828");
    EXPECT_EQ(lexical_cast<std::string>(1e10), "10000000000");
}

// ============================================================
// String -> Integer via lexical_cast
// ============================================================

TEST(LexicalCast, StringToInt) {
    EXPECT_EQ(lexical_cast<int>("42"), 42);
    EXPECT_EQ(lexical_cast<int>("0"), 0);
    EXPECT_EQ(lexical_cast<int>("-1"), -1);
    EXPECT_EQ(lexical_cast<int>("+100"), 100);
    EXPECT_EQ(lexical_cast<int>("2147483647"), 2147483647);
    EXPECT_EQ(lexical_cast<int>("-2147483648"), -2147483647 - 1);
}

TEST(LexicalCast, StringToLong) {
    EXPECT_EQ(lexical_cast<long>("9223372036854775807"),
              static_cast<long>(INT64_MAX));
    EXPECT_EQ(lexical_cast<long>("0"), 0L);
    EXPECT_EQ(lexical_cast<long>("-1"), -1L);
}

TEST(LexicalCast, StringToUnsigned) {
    EXPECT_EQ(lexical_cast<unsigned int>("42"), 42u);
    EXPECT_EQ(lexical_cast<unsigned int>("0"), 0u);
    EXPECT_EQ(lexical_cast<uint32_t>("4294967295"), 4294967295u);
}

TEST(LexicalCast, StringToFloat) {
    EXPECT_FLOAT_EQ(lexical_cast<float>("3.14"), 3.14f);
    EXPECT_FLOAT_EQ(lexical_cast<float>("0"), 0.0f);
    EXPECT_FLOAT_EQ(lexical_cast<float>("-1.5"), -1.5f);
    EXPECT_FLOAT_EQ(lexical_cast<float>("1e3"), 1000.0f);
}

TEST(LexicalCast, StringToDouble) {
    EXPECT_DOUBLE_EQ(lexical_cast<double>("3.14159"), 3.14159);
    EXPECT_DOUBLE_EQ(lexical_cast<double>("0.0"), 0.0);
    EXPECT_DOUBLE_EQ(lexical_cast<double>("-1e5"), -1e5);
}

// ============================================================
// String -> Integer: Failure cases (empty, invalid)
// ============================================================

TEST(LexicalCast, StringToIntEmptyThrows) {
    EXPECT_THROW(lexical_cast<int>(""), std::runtime_error);
    EXPECT_THROW(lexical_cast<int>(std::string_view("")), std::runtime_error);
}

TEST(LexicalCast, StringToIntInvalidThrows) {
    EXPECT_THROW(lexical_cast<int>("abc"), std::runtime_error);
    EXPECT_THROW(lexical_cast<int>("12.34"), std::runtime_error);
    EXPECT_THROW(lexical_cast<int>("12abc"), std::runtime_error);
    EXPECT_THROW(lexical_cast<int>("0x10"), std::runtime_error);
    EXPECT_THROW(lexical_cast<int>(" 42"), std::runtime_error);
}

TEST(LexicalCast, StringToDoubleInvalidThrows) {
    EXPECT_THROW(lexical_cast<double>(""), std::runtime_error);
    EXPECT_THROW(lexical_cast<double>("abc"), std::runtime_error);
}

TEST(LexicalCast, NullPointerGuard) {
    EXPECT_THROW(lexical_cast<int>((const char*)nullptr), std::runtime_error);
    EXPECT_THROW(lexical_cast<bool>((const char*)nullptr), std::runtime_error);
}

TEST(LexicalCast, ConstCharReturnGuard) {
    // Numeric -> const char* is disallowed (dangling)
    EXPECT_THROW(lexical_cast<const char*>(42), std::runtime_error);
    EXPECT_THROW(lexical_cast<const char*>(true), std::runtime_error);
}

// ============================================================
// Bool conversions
// ============================================================

TEST(LexicalCast, BoolToString) {
    EXPECT_EQ(lexical_cast<std::string>(true), "true");
    EXPECT_EQ(lexical_cast<std::string>(false), "false");
}

TEST(LexicalCast, StringToBool) {
    EXPECT_TRUE(lexical_cast<bool>("true"));
    EXPECT_TRUE(lexical_cast<bool>("1"));
    EXPECT_TRUE(lexical_cast<bool>("yes"));
    EXPECT_TRUE(lexical_cast<bool>("on"));
    EXPECT_FALSE(lexical_cast<bool>("false"));
    EXPECT_FALSE(lexical_cast<bool>("0"));
    EXPECT_FALSE(lexical_cast<bool>("no"));
    EXPECT_FALSE(lexical_cast<bool>("off"));
    EXPECT_THROW(lexical_cast<bool>("maybe"), std::runtime_error);
    EXPECT_THROW(lexical_cast<bool>(""), std::runtime_error);
}

// ============================================================
// String-to-T-to-String full round-trip
// ============================================================

TEST(LexicalCast, FullRoundTripInt) {
    for (int v : {0, 1, -1, 42, -100, INT32_MAX, INT32_MIN + 1}) {
        std::string s = lexical_cast<std::string>(v);
        int back = lexical_cast<int>(s);
        EXPECT_EQ(back, v);
    }
}

TEST(LexicalCast, FullRoundTripDouble) {
    double values[] = {0.0, 1.0, -1.0, 3.14, 1e10, -2.5e-3};
    for (double v : values) {
        std::string s = lexical_cast<std::string>(v);
        double back = lexical_cast<double>(s);
        EXPECT_DOUBLE_EQ(back, v);
    }
}

TEST(LexicalCast, FullRoundTripUnsigned) {
    unsigned values[] = {0u, 1u, 42u, UINT32_MAX / 2, UINT32_MAX};
    for (unsigned v : values) {
        std::string s = lexical_cast<std::string>(v);
        unsigned back = lexical_cast<unsigned>(s);
        EXPECT_EQ(back, v);
    }
}

// ============================================================
// Numeric -> Numeric (static_cast)
// ============================================================

TEST(LexicalCast, NumericToNumeric) {
    EXPECT_EQ(lexical_cast<int>(42.9), 42);
    EXPECT_EQ(lexical_cast<double>(42), 42.0);
    EXPECT_EQ(lexical_cast<int64_t>(42), 42LL);
    EXPECT_EQ(lexical_cast<short>(100), 100);
    EXPECT_EQ(lexical_cast<float>(3), 3.0f);
    EXPECT_EQ(lexical_cast<long>(-1), -1L);
}

// ============================================================
// String-to-String (identity / static_cast)
// ============================================================

TEST(LexicalCast, StringToString) {
    std::string src = "hello";
    std::string result = lexical_cast<std::string>(src);
    EXPECT_EQ(result, "hello");
}

TEST(LexicalCast, StringViewToString) {
    std::string_view sv = "world";
    std::string result = lexical_cast<std::string>(sv);
    EXPECT_EQ(result, "world");
}

// ============================================================
// Safe conversion: lexical_cast_safe
// ============================================================

TEST(LexicalCast, SafeConversionSuccess) {
    int out = -1;
    EXPECT_TRUE(lexical_cast_safe<int>("999", out));
    EXPECT_EQ(out, 999);

    double d = 0.0;
    EXPECT_TRUE(lexical_cast_safe<double>("3.14", d));
    EXPECT_DOUBLE_EQ(d, 3.14);
}

TEST(LexicalCast, SafeConversionFailure) {
    int out = 42;
    EXPECT_FALSE(lexical_cast_safe<int>("not-a-number", out));
    // out should be unchanged on failure (no exception, just false)
    EXPECT_EQ(out, 42);

    EXPECT_FALSE(lexical_cast_safe<int>("", out));
    EXPECT_EQ(out, 42);
}

// ============================================================
// to_chars_string / from_chars_string helpers
// ============================================================

TEST(LexicalCast, ToCharsString) {
    std::string s;
    EXPECT_TRUE(to_chars_string(42, s));
    EXPECT_EQ(s, "42");
    EXPECT_TRUE(to_chars_string(-1, s));
    EXPECT_EQ(s, "-1");
    EXPECT_TRUE(to_chars_string(0, s));
    EXPECT_EQ(s, "0");
    EXPECT_TRUE(to_chars_string(INT64_MAX, s));
    EXPECT_EQ(s, std::to_string(INT64_MAX));
}

TEST(LexicalCast, FromCharsString) {
    int v = 0;
    EXPECT_TRUE(from_chars_string("123", v));
    EXPECT_EQ(v, 123);
    EXPECT_TRUE(from_chars_string("-99", v));
    EXPECT_EQ(v, -99);
    EXPECT_FALSE(from_chars_string("abc", v));
    EXPECT_FALSE(from_chars_string("", v));
}

// ============================================================
// Float/Double helper functions
// ============================================================

TEST(LexicalCast, FloatToStringHelper) {
    EXPECT_EQ(float_to_string(1.5f), "1.5");
    EXPECT_EQ(float_to_string(0.0f), "0");
    EXPECT_EQ(float_to_string(-0.5f), "-0.5");
    EXPECT_NE(float_to_string(3.14159265f).find("3.14"), std::string::npos);
}

TEST(LexicalCast, DoubleToStringHelper) {
    EXPECT_EQ(double_to_string(3.14159), "3.14159");
    EXPECT_EQ(double_to_string(0.0), "0");
    EXPECT_EQ(double_to_string(-1.0), "-1");
    EXPECT_NE(double_to_string(2.718281828).find("2.718"), std::string::npos);
}

TEST(LexicalCast, StringToFloatHelper) {
    float f = 0;
    EXPECT_TRUE(string_to_float("2.5", f));
    EXPECT_FLOAT_EQ(f, 2.5f);
    EXPECT_TRUE(string_to_float("-1.25", f));
    EXPECT_FLOAT_EQ(f, -1.25f);
    EXPECT_FALSE(string_to_float("nope", f));
    EXPECT_FALSE(string_to_float("", f));
}

TEST(LexicalCast, StringToDoubleHelper) {
    double d = 0;
    EXPECT_TRUE(string_to_double("6.28", d));
    EXPECT_DOUBLE_EQ(d, 6.28);
    EXPECT_TRUE(string_to_double("-3.0", d));
    EXPECT_DOUBLE_EQ(d, -3.0);
    EXPECT_FALSE(string_to_double("nope", d));
    EXPECT_FALSE(string_to_double("", d));
}

// ============================================================
// Edge cases
// ============================================================

TEST(LexicalCast, LeadingPlusSign) {
    EXPECT_EQ(lexical_cast<int>("+42"), 42);
    EXPECT_EQ(lexical_cast<unsigned int>("+0"), 0u);
}

TEST(LexicalCast, TrailingDotInInt) {
    // "42." is not a valid integer
    EXPECT_THROW(lexical_cast<int>("42."), std::runtime_error);
}

TEST(LexicalCast, NegativeUnsigned) {
    // Negative unsigned should throw
    EXPECT_THROW(lexical_cast<unsigned int>("-1"), std::runtime_error);
}

TEST(LexicalCast, Overflow) {
    // Out-of-range integer should throw
    std::string huge(40, '9');
    EXPECT_THROW(lexical_cast<int>(huge), std::runtime_error);
    EXPECT_THROW(lexical_cast<int64_t>(huge), std::runtime_error);
}

TEST(LexicalCast, LexicalCastToStringEdgeCase) {
    // Strings with special characters
    std::string special = "hello\nworld\ttest";
    std::string result = lexical_cast<std::string>(special);
    EXPECT_EQ(result, special);
}
