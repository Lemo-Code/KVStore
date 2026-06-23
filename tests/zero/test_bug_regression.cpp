#include <gtest/gtest.h>
#include "fault_helpers.h"
#include "temp_file.h"
#include "zero/zero.h"
#include "zero/config/config.h"
#include "zero/net/buffer.h"
#include "zero/scheduler/timer_wheel.h"
#include "zero/base/macro.h"

using namespace zero;
using namespace kvtest;

TEST(ZeroBugRegressionTest, BufferReadFromMissingFile) {
    ByteBuffer buf;
    EXPECT_FALSE(buf.readFromFile("/tmp/kvstore_zero_no_such_file"));
}

TEST(ZeroBugRegressionTest, BufferWriteToInvalidPath) {
    ByteBuffer buf;
    buf.writeFUInt32(1);
    EXPECT_FALSE(buf.writeToFile("/proc/kvstore_cannot_write_here"));
}

TEST(ZeroBugRegressionTest, ConfigLoadMalformedYaml) {
    InitConfig();
    Config::Lookup("bug.port", 6379, "port");
    TempFile tmp("/tmp/kvstore_bug_yaml_XXXXXX");
    writeTextFile(tmp.path(), "bug:\n  port: [unclosed\n");
    EXPECT_FALSE(Config::LoadFromYamlFile(tmp.path()));
}

TEST(ZeroBugRegressionTest, ConfigFromStringRejectsBadScalar) {
    InitConfig();
    auto item = Config::Lookup("bug.scalar", 1, "int");
    ASSERT_NE(item, nullptr);
    EXPECT_FALSE(item->fromString("not-int"));
}

TEST(ZeroBugRegressionTest, TimerCancelUnknownIdLazyAccept) {
    TimerWheel wheel;
    // 当前实现: 惰性取消队列，未知 ID 也返回 true
    EXPECT_TRUE(wheel.cancelTimer(999999));
    EXPECT_TRUE(wheel.empty());
}

TEST(ZeroBugRegressionTest, TimerDoubleCancelLazyAccept) {
    TimerWheel wheel;
    uint64_t id = wheel.addTimer(100, []() {});
    EXPECT_TRUE(wheel.cancelTimer(id));
    EXPECT_TRUE(wheel.cancelTimer(id));
}

TEST(ZeroBugRegressionTest, BufferPositionAtEnd) {
    ByteBuffer buf;
    buf.writeFUInt8(1);
    buf.setPosition(buf.getSize());
    EXPECT_EQ(buf.getReadSize(), 0u);
}

TEST(ZeroBugRegressionTest, TimerTickWithNoTimers) {
    TimerWheel wheel;
    std::vector<std::function<void()>> cbs;
    wheel.tick(GetCurrentMS(), cbs);
    EXPECT_TRUE(cbs.empty());
    EXPECT_EQ(wheel.nextExpireMs(), ~0ull);
}
