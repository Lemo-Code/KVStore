#include <gtest/gtest.h>
#include "zero/base/lexicalcast.h"

using namespace zero;

TEST(LexicalCastTest, StringToInt) {
    LexicalCast<int, std::string> cast;
    EXPECT_EQ(cast("42"), 42);
    EXPECT_EQ(cast("-7"), -7);
    EXPECT_EQ(cast("0"), 0);
}

TEST(LexicalCastTest, StringToInt64) {
    LexicalCast<int64_t, std::string> cast;
    EXPECT_EQ(cast("9223372036854775807"), 9223372036854775807LL);
}

TEST(LexicalCastTest, StringToDouble) {
    LexicalCast<double, std::string> cast;
    EXPECT_DOUBLE_EQ(cast("3.14"), 3.14);
}

TEST(LexicalCastTest, IntToString) {
    LexicalCast<std::string, int> cast;
    EXPECT_EQ(cast(123), "123");
    EXPECT_EQ(cast(-1), "-1");
}

TEST(LexicalCastTest, Int64ToString) {
    LexicalCast<std::string, int64_t> cast;
    EXPECT_EQ(cast(10000000000LL), "10000000000");
}

TEST(LexicalCastTest, StringPassthrough) {
    LexicalCast<std::string, std::string> cast;
    std::string s = "unchanged";
    EXPECT_EQ(&cast(s), &s);
}

TEST(LexicalCastTest, GenericStreamCast) {
    LexicalCast<std::string, int> toStr;
    LexicalCast<int, std::string> toInt;
    EXPECT_EQ(toInt(toStr(999)), 999);
}
