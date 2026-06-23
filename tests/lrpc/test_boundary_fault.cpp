#include <gtest/gtest.h>
#include "lrpc/protocol.h"
#include <lstl/container/vector.h>

using namespace lrpc;

TEST(LrpcBoundaryFaultTest, TruncatedFrameHeader) {
    uint8_t buf[FRAME_HDR_SIZE] = {};
    FrameHeader decoded{};
    EXPECT_FALSE(decodeFrameHeader(buf, FRAME_HDR_SIZE - 1, decoded));
}

TEST(LrpcBoundaryFaultTest, FrameLenTooSmall) {
    uint8_t buf[FRAME_HDR_SIZE] = {};
    encodeU32(buf, RPC_MAGIC);
    encodeU32(buf + 4, 8);
    FrameHeader decoded{};
    EXPECT_FALSE(decodeFrameHeader(buf, FRAME_HDR_SIZE, decoded));
}

TEST(LrpcBoundaryFaultTest, DecodeHeadersTruncatedKey) {
    uint8_t buf[16] = {};
    encodeU16(buf, 1);
    encodeU16(buf + 2, 10);
    lstl::vector<std::pair<std::string, std::string>> out;
    EXPECT_EQ(decodeHeaders(buf, 4, out), 0u);
}

TEST(LrpcBoundaryFaultTest, DecodeHeadersTruncatedValue) {
    uint8_t buf[32] = {};
    encodeU16(buf, 1);
    encodeU16(buf + 2, 2);
    buf[4] = 'k';
    buf[5] = 'v';
    encodeU32(buf + 6, 100);
    lstl::vector<std::pair<std::string, std::string>> out;
    EXPECT_EQ(decodeHeaders(buf, 10, out), 0u);
}

TEST(LrpcBoundaryFaultTest, EncodeHeadersBufferTooSmall) {
    lstl::vector<std::pair<std::string, std::string>> headers{
        {"long-key-name", "long-value-name"}};
    uint8_t tiny[4] = {};
    EXPECT_EQ(encodeHeaders(tiny, sizeof(tiny), headers), 0u);
}

TEST(LrpcBoundaryFaultTest, EmptyHeadersRoundTrip) {
    lstl::vector<std::pair<std::string, std::string>> headers;
    uint8_t buf[8] = {};
    size_t n = encodeHeaders(buf, sizeof(buf), headers);
    EXPECT_EQ(n, 2u);
    lstl::vector<std::pair<std::string, std::string>> out;
    EXPECT_EQ(decodeHeaders(buf, n, out), n);
    EXPECT_TRUE(out.empty());
}

TEST(LrpcBoundaryFaultTest, MaxU16KeyLength) {
    std::string key(300, 'k');
    lstl::vector<std::pair<std::string, std::string>> headers{{key, "v"}};
    uint8_t buf[512] = {};
    size_t n = encodeHeaders(buf, sizeof(buf), headers);
    EXPECT_GT(n, 0u);
    lstl::vector<std::pair<std::string, std::string>> out;
    EXPECT_EQ(decodeHeaders(buf, n, out), n);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].first.size(), 300u);
}
