#include <gtest/gtest.h>
#include "ledis/protocol/resp_writer.h"

using namespace ledis;

TEST(RespWriterApiCoverageTest, AllErrorAndSpecialWriters) {
    std::string buf;
    RespWriter::writePong(buf);
    RespWriter::writeQueued(buf);
    RespWriter::writeNullArray(buf);
    RespWriter::writeSimpleString(buf, "ok");
    RespWriter::writeStringArray(buf, lstl::vector<std::string>{"a"});
    RespWriter::writeErrorWrongType(buf);
    RespWriter::writeErrorNotInteger(buf);
    RespWriter::writeErrorUnknownCmd(buf, "foo");
    RespWriter::writeErrorWrongArgs(buf, "bar");
    RespWriter::writeErrorNoSuchKey(buf);
    RespWriter::appendInt(buf, 42);
    EXPECT_FALSE(buf.empty());
}
