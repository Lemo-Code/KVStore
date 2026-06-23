#include <gtest/gtest.h>
#include "lrpc/protocol.h"
#include <lstl/container/vector.h>
#include <cstring>

using namespace lrpc;

// lrpc 协议层全部公开 API

TEST(LrpcApiCoverageTest, AllProtocolFunctions) {
    uint8_t buf[512] = {};

    encodeU32(buf, 0xAABBCCDD);
    EXPECT_EQ(decodeU32(buf), 0xAABBCCDDu);
    encodeU16(buf, 0xBEEF);
    EXPECT_EQ(decodeU16(buf), 0xBEEFu);

    FrameHeader hdr{};
    hdr.magic = RPC_MAGIC;
    hdr.frame_len = FRAME_HDR_SIZE + 20;
    hdr.call_id = 7;
    hdr.msg_type = static_cast<uint16_t>(MsgType::PING);
    hdr.flags = FLAG_ONEWAY;
    hdr.body_len = 20;
    encodeFrameHeader(buf, hdr);

    FrameHeader out{};
    EXPECT_TRUE(decodeFrameHeader(buf, FRAME_HDR_SIZE, out));
    EXPECT_EQ(out.magic, RPC_MAGIC);
    EXPECT_EQ(out.call_id, 7u);

    lstl::vector<std::pair<std::string, std::string>> headers{
        {"node-id", "n1"}, {"epoch", "3"}};
    size_t hb = headerBlockSize(headers);
    EXPECT_GT(hb, 0u);
    size_t n = encodeHeaders(buf, sizeof(buf), headers);
    EXPECT_EQ(n, hb);

    lstl::vector<std::pair<std::string, std::string>> decoded;
    EXPECT_EQ(decodeHeaders(buf, n, decoded), n);
    ASSERT_EQ(decoded.size(), 2u);

    EXPECT_EQ(frameSize(100, hb), FRAME_HDR_SIZE + hb + 100);

    EXPECT_EQ(static_cast<uint16_t>(MsgType::FORWARD), 10);
    EXPECT_EQ(static_cast<uint16_t>(MsgType::MIGRATE_COMMIT), 43);
    EXPECT_EQ(FLAG_RESPONSE, 0x01);
    EXPECT_EQ(FLAG_ERROR, 0x02);
    EXPECT_EQ(FLAG_ONEWAY, 0x04);
}

TEST(LrpcApiCoverageTest, FrameHeaderAllMsgTypes) {
    const MsgType types[] = {
        MsgType::PING, MsgType::PONG, MsgType::MEET, MsgType::FAIL,
        MsgType::FORWARD, MsgType::FORWARD_REPLY,
        MsgType::UPDATE_CONFIG,
        MsgType::REPL_CONF, MsgType::REPL_ACK,
        MsgType::MIGRATE_START, MsgType::MIGRATE_DATA,
        MsgType::MIGRATE_ACK, MsgType::MIGRATE_COMMIT,
        MsgType::PUBLISH,
    };
    uint8_t buf[FRAME_HDR_SIZE];
    for (auto mt : types) {
        FrameHeader hdr{};
        hdr.magic = RPC_MAGIC;
        hdr.frame_len = FRAME_HDR_SIZE;
        hdr.msg_type = static_cast<uint16_t>(mt);
        encodeFrameHeader(buf, hdr);
        FrameHeader decoded{};
        EXPECT_TRUE(decodeFrameHeader(buf, FRAME_HDR_SIZE, decoded));
        EXPECT_EQ(decoded.msg_type, static_cast<uint16_t>(mt));
    }
}
