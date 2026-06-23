#include <gtest/gtest.h>
#include "lrpc/protocol.h"
#include <lstl/container/vector.h>

using namespace lrpc;

TEST(LrpcProtocolTest, FrameHeaderRoundTrip) {
    FrameHeader hdr{};
    hdr.magic = RPC_MAGIC;
    hdr.frame_len = 40;
    hdr.call_id = 42;
    hdr.msg_type = static_cast<uint16_t>(MsgType::PING);
    hdr.flags = FLAG_ONEWAY;
    hdr.body_len = 20;

    uint8_t buf[FRAME_HDR_SIZE];
    encodeFrameHeader(buf, hdr);

    FrameHeader decoded{};
    EXPECT_TRUE(decodeFrameHeader(buf, FRAME_HDR_SIZE, decoded));
    EXPECT_EQ(decoded.magic, RPC_MAGIC);
    EXPECT_EQ(decoded.call_id, 42u);
    EXPECT_EQ(decoded.body_len, 20u);
}

TEST(LrpcProtocolTest, RejectBadMagic) {
    uint8_t buf[FRAME_HDR_SIZE] = {};
    FrameHeader decoded{};
    EXPECT_FALSE(decodeFrameHeader(buf, FRAME_HDR_SIZE, decoded));
}

TEST(LrpcProtocolTest, HeadersEncodeDecode) {
    lstl::vector<std::pair<std::string, std::string>> headers;
    headers.push_back({"node-id", "abc"});
    headers.push_back({"epoch", "7"});

    uint8_t buf[256];
    size_t n = encodeHeaders(buf, sizeof(buf), headers);
    EXPECT_GT(n, 0u);
    EXPECT_EQ(n, headerBlockSize(headers));

    lstl::vector<std::pair<std::string, std::string>> out;
    size_t consumed = decodeHeaders(buf, n, out);
    EXPECT_EQ(consumed, n);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].first, "node-id");
    EXPECT_EQ(out[1].second, "7");
}

TEST(LrpcProtocolTest, U32U16Endian) {
    uint8_t buf[4];
    encodeU32(buf, 0x01020304);
    EXPECT_EQ(decodeU32(buf), 0x01020304u);
    encodeU16(buf, 0xABCD);
    EXPECT_EQ(decodeU16(buf), 0xABCDu);
}

TEST(LrpcProtocolTest, FrameSizeCalc) {
    lstl::vector<std::pair<std::string, std::string>> h{{"k", "v"}};
    size_t hb = headerBlockSize(h);
    EXPECT_EQ(frameSize(100, hb), FRAME_HDR_SIZE + hb + 100);
}
