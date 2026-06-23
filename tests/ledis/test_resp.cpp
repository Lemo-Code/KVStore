#include <gtest/gtest.h>
#include "ledis/protocol/resp_parser.h"
#include "ledis/protocol/resp_writer.h"

using namespace ledis;

static std::string buildArrayCmd(std::initializer_list<const char*> args) {
    std::string buf;
    RespWriter::writeArrayHeader(buf, static_cast<int64_t>(args.size()));
    for (auto* a : args)
        RespWriter::writeBulkString(buf, a);
    return buf;
}

TEST(RespTest, WriterPrimitives) {
    std::string buf;
    RespWriter::writeOK(buf);
    EXPECT_EQ(buf, "+OK\r\n");
    buf.clear();
    RespWriter::writeInteger(buf, 42);
    EXPECT_EQ(buf, ":42\r\n");
    buf.clear();
    RespWriter::writeBulkString(buf, "hello");
    EXPECT_EQ(buf, "$5\r\nhello\r\n");
    buf.clear();
    RespWriter::writeError(buf, "ERR fail");
    EXPECT_EQ(buf, "-ERR fail\r\n");
    buf.clear();
    RespWriter::writeNull(buf);
    EXPECT_EQ(buf, "$-1\r\n");
    buf.clear();
    RespWriter::writeEmptyArray(buf);
    EXPECT_EQ(buf, "*0\r\n");
}

TEST(RespTest, ParserSingleCommand) {
    auto raw = buildArrayCmd({"SET", "k", "v"});
    RespParser parser;
    size_t consumed = 0;
    auto r = parser.feed(raw.data(), raw.size(), consumed);
    ASSERT_EQ(r, RespParser::Result::OK);
    ASSERT_EQ(parser.args().size(), 3u);
    EXPECT_EQ(parser.args()[0], "SET");
    EXPECT_EQ(parser.args()[2], "v");
}

TEST(RespTest, ParserIncremental) {
    auto raw = buildArrayCmd({"GET", "key"});
    RespParser parser;
    // 分两段喂入，确保第一段不足时返回 NEED_MORE
    size_t mid = raw.size() / 2;
    size_t consumed = 0;
    auto r = parser.feed(raw.data(), mid, consumed);
    EXPECT_EQ(r, RespParser::Result::NEED_MORE);
    r = parser.feed(raw.data() + consumed, raw.size() - consumed, consumed);
    EXPECT_EQ(r, RespParser::Result::OK);
    EXPECT_EQ(parser.args()[0], "GET");
}

TEST(RespTest, ParserPipeline) {
    auto cmd1 = buildArrayCmd({"PING"});
    auto cmd2 = buildArrayCmd({"ECHO", "hi"});
    std::string pipeline = cmd1 + cmd2;

    RespParser parser;
    size_t off = 0;
    size_t consumed = 0;
    auto r = parser.feed(pipeline.data() + off, cmd1.size(), consumed);
    EXPECT_EQ(r, RespParser::Result::OK);
    EXPECT_EQ(parser.args()[0], "PING");
    parser.reset();

    off += consumed;
    r = parser.feed(pipeline.data() + off, pipeline.size() - off, consumed);
    EXPECT_EQ(r, RespParser::Result::OK);
    EXPECT_EQ(parser.args()[0], "ECHO");
}

TEST(RespTest, ParserNullBulk) {
    std::string buf;
    RespWriter::writeArrayHeader(buf, 2);
    RespWriter::writeBulkString(buf, "GET");
    buf += "$-1\r\n";
    RespParser parser;
    size_t consumed = 0;
    auto r = parser.feed(buf.data(), buf.size(), consumed);
    EXPECT_EQ(r, RespParser::Result::OK);
    EXPECT_TRUE(parser.args()[1].empty());
}

TEST(RespTest, ParserErrorOnBadType) {
    const char* bad = "?\r\n";
    RespParser parser;
    size_t consumed = 0;
    auto r = parser.feed(bad, 3, consumed);
    EXPECT_EQ(r, RespParser::Result::ERROR);
}

TEST(RespTest, ParserReset) {
    auto raw = buildArrayCmd({"DEL", "a"});
    RespParser parser;
    size_t consumed = 0;
    parser.feed(raw.data(), raw.size(), consumed);
    parser.reset();
    EXPECT_TRUE(parser.isIdle());
    EXPECT_TRUE(parser.args().empty());
}
