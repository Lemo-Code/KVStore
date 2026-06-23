#include <gtest/gtest.h>
#include "zero/zero.h"

using namespace zero;

TEST(ZeroInitTest, InitConfigRegistersVars) {
    InitConfig();
    EXPECT_NE(config::FiberStackSize(), 0u);
    EXPECT_GT(config::SchedulerThreads(), 0);
    EXPECT_TRUE(config::SocketTcpNoDelay());
    EXPECT_GT(config::SocketConnectTimeoutMs(), 0);
    EXPECT_GT(config::ReactorMaxEvents(), 0u);
}

TEST(ZeroInitTest, ConfigAccessors) {
    InitConfig();
    EXPECT_FALSE(config::SchedulerName().empty());
    EXPECT_GE(config::TcpServerTimeoutMs(), 0);
    EXPECT_GE(config::FiberStackSize(), 1024u);
}
