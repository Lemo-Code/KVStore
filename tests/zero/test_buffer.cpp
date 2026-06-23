#include <gtest/gtest.h>
#include "zero/net/buffer.h"

using namespace zero;

TEST(ByteBufferTest, FixedIntegersRoundTrip) {
    ByteBuffer buf;
    buf.writeFInt32(-12345);
    buf.writeFUInt32(0xDEADBEEFu);
    buf.writeFInt64(-9876543210LL);
    buf.setPosition(0);
    EXPECT_EQ(buf.readFInt32(), -12345);
    EXPECT_EQ(buf.readFUInt32(), 0xDEADBEEFu);
    EXPECT_EQ(buf.readFInt64(), -9876543210LL);
}

TEST(ByteBufferTest, VarintRoundTrip) {
    ByteBuffer buf;
    buf.writeUInt32(42);
    buf.writeUInt64(1ULL << 40);
    buf.setPosition(0);
    EXPECT_EQ(buf.readUInt32(), 42u);
    EXPECT_EQ(buf.readUInt64(), 1ULL << 40);
}

TEST(ByteBufferTest, StringAndRawData) {
    ByteBuffer buf;
    std::string hello = "hello";
    buf.writeStringV64(hello);
    buf.write("world", 5);
    buf.setPosition(0);
    EXPECT_EQ(buf.readStringV64(), hello);
    char raw[5];
    buf.read(raw, 5);
    EXPECT_EQ(std::string(raw, 5), "world");
}

TEST(ByteBufferTest, FloatDoubleRoundTrip) {
    ByteBuffer buf;
    buf.writeFloat(3.14f);
    buf.writeDouble(2.718281828);
    buf.setPosition(0);
    EXPECT_FLOAT_EQ(buf.readFloat(), 3.14f);
    EXPECT_DOUBLE_EQ(buf.readDouble(), 2.718281828);
}

TEST(ByteBufferTest, ClearResetsState) {
    ByteBuffer buf;
    buf.writeFUInt32(100);
    buf.clear();
    EXPECT_EQ(buf.getSize(), 0u);
    EXPECT_EQ(buf.getPosition(), 0u);
}

TEST(ByteBufferTest, MultiBlockGrowth) {
    ByteBuffer buf(64);
    for (int i = 0; i < 200; ++i)
        buf.writeFUInt8(static_cast<uint8_t>(i & 0xFF));
    buf.setPosition(0);
    for (int i = 0; i < 200; ++i)
        EXPECT_EQ(buf.readFUInt8(), static_cast<uint8_t>(i & 0xFF));
}
