#include <gtest/gtest.h>
#include "zero/config/config.h"

using namespace zero;

TEST(ConfigTest, LookupAndModifyScalar) {
    auto port = Config::Lookup("server.port", 6379, "listen port");
    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->getValue(), 6379);
    port->setValue(7000);
    EXPECT_EQ(port->getValue(), 7000);
    EXPECT_EQ(port->toString(), "7000");
}

TEST(ConfigTest, FromStringUpdatesValue) {
    auto threads = Config::Lookup("server.threads", 4, "worker threads");
    ASSERT_NE(threads, nullptr);
    EXPECT_TRUE(threads->fromString("8"));
    EXPECT_EQ(threads->getValue(), 8);
}

TEST(ConfigTest, BoolConfig) {
    auto enabled = Config::Lookup("cluster.enabled", false, "cluster on/off");
    ASSERT_NE(enabled, nullptr);
    EXPECT_FALSE(enabled->getValue());
    enabled->setValue(true);
    EXPECT_TRUE(enabled->getValue());
}

TEST(ConfigTest, DoubleConfig) {
    auto ratio = Config::Lookup("test.ratio", 0.5, "ratio");
    ASSERT_NE(ratio, nullptr);
    ratio->fromString("1.25");
    EXPECT_DOUBLE_EQ(ratio->getValue(), 1.25);
}

TEST(ConfigTest, VisitAllVars) {
    Config::Lookup("visit.a", 1, "a");
    Config::Lookup("visit.b", 2, "b");
    int count = 0;
    Config::Visit([&](ConfigVarBase::ptr) { count++; });
    EXPECT_GE(count, 2);
}
