#include <gtest/gtest.h>
#include "zero/base/macro.h"

using namespace zero;

TEST(ZeroUtilTest, GetThreadId) {
    EXPECT_GT(GetThreadId(), 0u);
}

TEST(ZeroUtilTest, GetCurrentMS) {
    uint64_t t1 = GetCurrentMS();
    uint64_t t2 = GetCurrentMS();
    EXPECT_LE(t1, t2);
}

TEST(ZeroUtilTest, BacktraceToString) {
    std::string bt = BacktraceToString(0);
    EXPECT_FALSE(bt.empty());
}
