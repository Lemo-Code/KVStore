#include <gtest/gtest.h>
#include <fstream>
#include "fault_helpers.h"
#include "temp_file.h"
#include "zero/zero.h"
#include "zero/config/config.h"
#include "zero/net/buffer.h"
#include "zero/base/lexicalcast.h"

using namespace zero;
using namespace kvtest;

// ---- ByteBuffer 边界 / 故障注入 ----

TEST(ByteBufferBoundaryFaultTest, ReadFromMissingFile) {
    ByteBuffer buf;
    EXPECT_FALSE(buf.readFromFile("/tmp/kvstore_no_such_buffer_file_42"));
}

TEST(ByteBufferBoundaryFaultTest, ReadFromEmptyFile) {
    kvtest::TempFile tmp("/tmp/kvstore_empty_buf_XXXXXX");
    ByteBuffer buf;
    EXPECT_TRUE(buf.readFromFile(tmp.path()));
    EXPECT_EQ(buf.getSize(), 0u);
}

TEST(ByteBufferBoundaryFaultTest, WriteToUnwritablePath) {
    ByteBuffer buf;
    buf.writeFUInt32(1);
    EXPECT_FALSE(buf.writeToFile("/proc/kvstore_cannot_write"));
}

TEST(ByteBufferBoundaryFaultTest, PositionPastEnd) {
    ByteBuffer buf;
    buf.writeFUInt8(1);
    buf.setPosition(0);
    EXPECT_EQ(buf.readFUInt8(), 1);
    buf.setPosition(buf.getSize());
    EXPECT_EQ(buf.getReadSize(), 0u);
}

TEST(ByteBufferBoundaryFaultTest, ZeroLengthString) {
    ByteBuffer buf;
    buf.writeStringV64("");
    buf.setPosition(0);
    EXPECT_EQ(buf.readStringV64(), "");
}

TEST(ByteBufferBoundaryFaultTest, LargeVarintRoundTrip) {
    ByteBuffer buf;
    buf.writeUInt64(UINT64_MAX);
    buf.setPosition(0);
    EXPECT_EQ(buf.readUInt64(), UINT64_MAX);
}

// ---- Config 故障注入 ----

TEST(ConfigBoundaryFaultTest, LoadMissingYaml) {
    InitConfig();
    EXPECT_FALSE(Config::LoadFromYamlFile("/tmp/kvstore_missing_cfg.yaml"));
}

TEST(ConfigBoundaryFaultTest, LoadMalformedYaml) {
    InitConfig();
    Config::Lookup("fault.port", 6379, "port");
    kvtest::TempFile tmp("/tmp/kvstore_bad_yaml_XXXXXX");
    writeTextFile(tmp.path(), "fault:\n  port: [not, valid, scalar\n");
    EXPECT_FALSE(Config::LoadFromYamlFile(tmp.path()));
}

TEST(ConfigBoundaryFaultTest, FromStringInvalidScalar) {
    InitConfig();
    auto item = Config::Lookup("fault.scalar", 1, "int");
    ASSERT_NE(item, nullptr);
    EXPECT_FALSE(item->fromString("abc"));
}

TEST(ConfigBoundaryFaultTest, LookupMissingAfterInit) {
    InitConfig();
    EXPECT_FALSE(Config::Lookup<int>("fault.nonexistent.key"));
}

// ---- LexicalCast 边界 ----

TEST(LexicalCastBoundaryFaultTest, AtoiEdgeCases) {
    LexicalCast<int, std::string> cast;
    EXPECT_EQ(cast(""), 0);
    EXPECT_EQ(cast("12abc"), 12);
    EXPECT_EQ(cast("-99"), -99);
}

TEST(LexicalCastBoundaryFaultTest, DoubleEdgeCases) {
    LexicalCast<double, std::string> dcast;
    EXPECT_DOUBLE_EQ(dcast("0"), 0.0);
    EXPECT_DOUBLE_EQ(dcast("-3.14"), -3.14);
}
