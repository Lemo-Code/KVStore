#include <gtest/gtest.h>
#include "zero/base/endian.h"

using namespace zero;

TEST(EndianTest, DetectLittleEndian) {
    // x86/ARM 通常是小端
    bool detected = IsLittleEndian();
    uint16_t probe = 0x0001;
    bool expected = (*reinterpret_cast<uint8_t*>(&probe) == 0x01);
    EXPECT_EQ(detected, expected);
}

TEST(EndianTest, ByteSwapRoundTrip) {
    uint32_t v = 0x12345678u;
    EXPECT_EQ(ByteSwap(ByteSwap(v)), v);
    uint64_t w = 0x0123456789ABCDEFULL;
    EXPECT_EQ(ByteSwap(ByteSwap(w)), w);
}

TEST(EndianTest, HostNetworkRoundTrip) {
    uint16_t v16 = 0xABCD;
    EXPECT_EQ(NetworkToHost(HostToNetwork(v16)), v16);
    uint32_t v32 = 0xDEADBEEF;
    EXPECT_EQ(NetworkToHost(HostToNetwork(v32)), v32);
}

TEST(EndianTest, NetworkIsBigEndianOnLittleHost) {
    if (!IsLittleEndian()) return;
    uint32_t host = 0x01020304;
    uint32_t net = HostToNetwork(host);
    EXPECT_EQ(net, 0x04030201u);
}
