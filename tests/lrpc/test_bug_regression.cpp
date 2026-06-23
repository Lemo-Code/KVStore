#include <gtest/gtest.h>
#include "lrpc/protocol.h"
#include <lstl/container/vector.h>
#include <cstring>

using namespace lrpc;

TEST(LrpcBugRegressionTest, DecodeHeaderTruncatedAtEachField) {
    uint8_t buf[32] = {};
    FrameHeader hdr{};
    EXPECT_FALSE(decodeFrameHeader(buf, FRAME_HDR_SIZE - 1, hdr));

    encodeU32(buf, RPC_MAGIC);
    encodeU32(buf + 4, 8);
    EXPECT_FALSE(decodeFrameHeader(buf, FRAME_HDR_SIZE, hdr));
}

TEST(LrpcBugRegressionTest, DecodeHeadersEmptyBlock) {
    uint8_t buf[8] = {};
    encodeU16(buf, 0);
    lstl::vector<std::pair<std::string, std::string>> out;
    EXPECT_EQ(decodeHeaders(buf, 2, out), 2u);
    EXPECT_TRUE(out.empty());
}

TEST(LrpcBugRegressionTest, EncodeHeadersZeroCapacity) {
    lstl::vector<std::pair<std::string, std::string>> h{{"k", "v"}};
    uint8_t buf[1] = {};
    EXPECT_EQ(encodeHeaders(buf, 0, h), 0u);
}

TEST(LrpcBugRegressionTest, DecodeHeadersKeyLengthOverflow) {
    uint8_t buf[16] = {};
    encodeU16(buf, 1);
    encodeU16(buf + 2, 100);
    lstl::vector<std::pair<std::string, std::string>> out;
    EXPECT_EQ(decodeHeaders(buf, 4, out), 0u);
}

TEST(LrpcBugRegressionTest, U32RoundTripBoundary) {
    uint8_t buf[4];
    encodeU32(buf, 0);
    EXPECT_EQ(decodeU32(buf), 0u);
    encodeU32(buf, UINT32_MAX);
    EXPECT_EQ(decodeU32(buf), UINT32_MAX);
}

TEST(LrpcBugRegressionTest, FrameSizeZeroBody) {
    lstl::vector<std::pair<std::string, std::string>> empty;
    size_t hb = headerBlockSize(empty);
    EXPECT_EQ(hb, 2u);
    EXPECT_EQ(frameSize(0, hb), FRAME_HDR_SIZE + hb);
}

TEST(LrpcBugRegressionTest, HeaderRoundTripMaxKeyValue) {
    std::string key(200, 'k');
    std::string val(300, 'v');
    lstl::vector<std::pair<std::string, std::string>> headers{{key, val}};
    uint8_t buf[1024] = {};
    size_t n = encodeHeaders(buf, sizeof(buf), headers);
    EXPECT_GT(n, 0u);
    lstl::vector<std::pair<std::string, std::string>> out;
    EXPECT_EQ(decodeHeaders(buf, n, out), n);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].first.size(), 200u);
    EXPECT_EQ(out[0].second.size(), 300u);
}
