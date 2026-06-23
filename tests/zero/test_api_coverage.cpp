#include <gtest/gtest.h>
#include <sys/uio.h>
#include <vector>
#include "temp_file.h"
#include "zero/zero.h"
#include "zero/config/config.h"
#include "zero/net/buffer.h"
#include "zero/base/endian.h"
#include "zero/base/lexicalcast.h"
#include "zero/base/singleton.h"
#include "zero/scheduler/timer_wheel.h"
#include "zero/base/macro.h"

using namespace zero;
using namespace kvtest;

namespace {
struct DummySingleton { int v = 0; };
using Dummy = Singleton<DummySingleton>;
}

// zero 库公开 API 冒烟

TEST(ZeroApiCoverageTest, ByteBufferFullSurface) {
    ByteBuffer buf(64);
    buf.writeFInt8(-1);
    buf.writeFUInt8(255);
    buf.writeFInt16(-100);
    buf.writeFUInt16(0xABCD);
    buf.writeFInt32(-7);
    buf.writeFUInt32(42);
    buf.writeFInt64(-99);
    buf.writeFUInt64(12345);
    buf.writeInt32(-7);
    buf.writeUInt32(8);
    buf.writeInt64(-99);
    buf.writeUInt64(12345);
    buf.writeFloat(1.5f);
    buf.writeDouble(2.5);
    buf.writeStringF16("f16");
    buf.writeStringF32("f32");
    buf.writeStringF64("f64");
    buf.writeStringV64("v64");
    buf.write("raw", 3);

    EXPECT_GT(buf.getBaseSize(), 0u);
    EXPECT_GT(buf.getCapacity(), 0u);
    EXPECT_GT(buf.getSize(), 0u);

    buf.setPosition(0);
    EXPECT_GT(buf.getReadSize(), 0u);
    EXPECT_EQ(buf.readFInt8(), -1);
    EXPECT_EQ(buf.readFUInt8(), 255);
    EXPECT_EQ(buf.readFInt16(), -100);
    EXPECT_EQ(buf.readFUInt16(), 0xABCDu);
    EXPECT_EQ(buf.readFInt32(), -7);
    EXPECT_EQ(buf.readFUInt32(), 42u);
    EXPECT_EQ(buf.readFInt64(), -99);
    EXPECT_EQ(buf.readFUInt64(), 12345u);
    EXPECT_EQ(buf.readInt32(), -7);
    EXPECT_EQ(buf.readUInt32(), 8u);
    EXPECT_EQ(buf.readInt64(), -99);
    EXPECT_EQ(buf.readUInt64(), 12345u);
    EXPECT_FLOAT_EQ(buf.readFloat(), 1.5f);
    EXPECT_DOUBLE_EQ(buf.readDouble(), 2.5);
    EXPECT_EQ(buf.readStringF16(), "f16");
    EXPECT_EQ(buf.readStringF32(), "f32");
    EXPECT_EQ(buf.readStringF64(), "f64");
    EXPECT_EQ(buf.readStringV64(), "v64");
    char raw[3];
    buf.read(raw, 3);
    EXPECT_EQ(std::string(raw, 3), "raw");

    (void)buf.isLittleEndian();
    buf.setIsLittleEndian(true);

    std::vector<iovec> iov;
    (void)buf.getReadBuffers(iov, 16);
    (void)buf.getWriteBuffers(iov, 16);
    (void)buf.toHexString();

    TempFile tmp("/tmp/kvstore_buf_api_XXXXXX");
    EXPECT_TRUE(buf.writeToFile(tmp.path()));
    ByteBuffer loaded;
    EXPECT_TRUE(loaded.readFromFile(tmp.path()));
    buf.clear();
}

TEST(ZeroApiCoverageTest, ConfigEndianLexicalTimerUtil) {
    InitConfig();
    auto item = Config::Lookup("api.test", 1, "test int");
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->toString(), "1");
    item->setValue(9);
    EXPECT_EQ(item->getValue(), 9);

    int visited = 0;
    Config::Visit([&](ConfigVarBase::ptr) { visited++; });
    EXPECT_GE(visited, 1);

    EXPECT_EQ(ByteSwap(ByteSwap(uint32_t{0x01020304})), 0x01020304u);
    uint16_t v16 = 0x1234;
    EXPECT_EQ(NetworkToHost(HostToNetwork(v16)), v16);

    LexicalCast<int, std::string> toInt;
    LexicalCast<std::string, int> toStr;
    EXPECT_EQ(toInt("42"), 42);
    EXPECT_EQ(toStr(7), "7");

    Dummy::GetInstance()->v = 1;
    EXPECT_EQ(Dummy::GetInstance()->v, 1);

    TimerWheel wheel;
    EXPECT_TRUE(wheel.empty());
    EXPECT_EQ(wheel.count(), 0u);
    uint64_t id = wheel.addTimer(50, []() {});
    EXPECT_FALSE(wheel.empty());
    EXPECT_NE(wheel.nextExpireMs(), ~0ull);
    EXPECT_TRUE(wheel.cancelTimer(id));

    EXPECT_GT(GetThreadId(), 0u);
    EXPECT_GT(GetCurrentMS(), 0u);
    EXPECT_FALSE(BacktraceToString(2).empty());
}
