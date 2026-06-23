#include <gtest/gtest.h>
#include <fstream>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "temp_file.h"
#include "zero/config/config.h"
#include "zero/net/buffer.h"
#include "zero/base/singleton.h"
#include "zero/zero.h"

using namespace zero;

namespace {
struct CounterData { int value = 0; };
using Counter = Singleton<CounterData>;
}

TEST(ZeroExtendedTest, YamlLoad) {
    InitConfig();
    kvtest::TempFile tmp("/tmp/kvstore_cfg_XXXXXX");
    {
        std::ofstream ofs(tmp.path());
        ofs << "fiber:\n  stack_size: 262144\n";
    }
    EXPECT_TRUE(Config::LoadFromYamlFile(tmp.path()));
    EXPECT_EQ(config::FiberStackSize(), 262144u);
}

TEST(ZeroExtendedTest, BufferFileRoundTrip) {
    ByteBuffer buf;
    buf.writeFUInt32(0xABCDEF01u);
    buf.writeStringV64("buffer-test");
    kvtest::TempFile tmp("/tmp/kvstore_buf_XXXXXX");
    EXPECT_TRUE(buf.writeToFile(tmp.path()));
    ByteBuffer buf2;
    EXPECT_TRUE(buf2.readFromFile(tmp.path()));
    buf2.setPosition(0);
    EXPECT_EQ(buf2.readFUInt32(), 0xABCDEF01u);
    EXPECT_EQ(buf2.readStringV64(), "buffer-test");
}

TEST(ZeroExtendedTest, BufferHexString) {
    ByteBuffer buf;
    buf.writeFUInt8(0xAB);
    buf.writeFUInt8(0xCD);
    auto hex = buf.toHexString();
    EXPECT_FALSE(hex.empty());
}

TEST(ZeroExtendedTest, Singleton) {
    Counter::GetInstance()->value = 42;
    EXPECT_EQ(Counter::GetInstance()->value, 42);
}

TEST(ZeroExtendedTest, InitZeroNoConfig) {
    InitZero(0, nullptr);
    EXPECT_GE(config::SchedulerThreads(), 1);
}

TEST(ZeroExtendedTest, LogLevelStrings) {
    EXPECT_STREQ(LogLevel::ToString(LogLevel::INFO), "INFO");
    EXPECT_EQ(LogLevel::FromString("DEBUG"), LogLevel::DEBUG);
}

TEST(ZeroExtendedTest, YamlContainerTypes) {
    InitConfig();
    auto vec = Config::Lookup("test.vec", std::vector<int>{1, 2}, "int vec");
    auto lst = Config::Lookup("test.lst", std::list<std::string>{"a"}, "str list");
    auto st = Config::Lookup("test.st", std::set<int>{42}, "int set");
    auto mp = Config::Lookup("test.mp", std::map<std::string, int>{{"k", 1}}, "str-int map");
    ASSERT_NE(vec, nullptr);
    ASSERT_NE(lst, nullptr);
    ASSERT_NE(st, nullptr);
    ASSERT_NE(mp, nullptr);

    kvtest::TempFile tmp("/tmp/kvstore_cfg2_XXXXXX");
    {
        std::ofstream ofs(tmp.path());
        ofs << "test:\n"
            << "  vec: [10, 20, 30]\n"
            << "  lst: [x, y]\n"
            << "  st: [5, 6]\n"
            << "  mp:\n"
            << "    foo: 99\n";
    }
    EXPECT_TRUE(Config::LoadFromYamlFile(tmp.path()));
    EXPECT_EQ(vec->getValue().size(), 3u);
    EXPECT_EQ(vec->getValue()[1], 20);
    EXPECT_EQ(lst->getValue().size(), 2u);
    EXPECT_EQ(st->getValue().count(5), 1u);
    EXPECT_EQ(mp->getValue().at("foo"), 99);

    EXPECT_FALSE(vec->fromString("not-a-container"));
    EXPECT_FALSE(Config::Lookup<std::vector<int>>("test.vec.missing"));

    int cb_count = 0;
    auto id = vec->addListener([&](const auto&, const auto&) { cb_count++; });
    vec->setValue({1});
    vec->delListener(id);
    vec->clearListeners();
    EXPECT_GE(cb_count, 1);
}

TEST(ZeroExtendedTest, LoadFromConfDir) {
    InitConfig();
    Config::Lookup("confdir.port", 6379, "port");
    kvtest::TempFile dir_marker("/tmp/kvstore_confdir_XXXXXX");
    std::string dir = dir_marker.path() + ".d";
    std::string cmd = "mkdir -p " + dir;
    (void)std::system(cmd.c_str());
    {
        std::ofstream ofs(dir + "/server.yaml");
        ofs << "confdir:\n  port: 7001\n";
    }
    Config::LoadFromConfDir(dir, false);
    EXPECT_EQ(Config::Lookup<int>("confdir.port")->getValue(), 7001);
    std::string rm_cmd = "rm -rf " + dir;
    (void)std::system(rm_cmd.c_str());
}

TEST(ZeroExtendedTest, BufferRemainingOps) {
    ByteBuffer buf;
    buf.writeFUInt16(0xABCD);
    buf.writeFInt16(-100);
    buf.setPosition(0);
    EXPECT_EQ(buf.readFUInt16(), 0xABCDu);
    EXPECT_EQ(buf.readFInt16(), -100);

    buf.clear();
    buf.writeStringF32("short");
    buf.setPosition(0);
    EXPECT_EQ(buf.readStringF32(), "short");

    buf.clear();
    buf.writeInt32(-7);
    buf.writeUInt64(12345);
    buf.setPosition(0);
    EXPECT_EQ(buf.readInt32(), -7);
    EXPECT_EQ(buf.readUInt64(), 12345u);
}
