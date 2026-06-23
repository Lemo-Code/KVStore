#include <gtest/gtest.h>
#include "ledis_helpers.h"
#include "ledis/protocol/resp_parser.h"
#include "ledis/protocol/resp_writer.h"

using namespace ledis;
using namespace kvtest;

static std::string enc(std::initializer_list<const char*> args) {
    std::string buf;
    RespWriter::writeArrayHeader(buf, static_cast<int64_t>(args.size()));
    for (auto* a : args) RespWriter::writeBulkString(buf, a);
    return buf;
}

TEST(IntegrationPipelineTest, MultipleCommandsInOneBuffer) {
    ensureCommandTable();
    StorageEngine engine;

    std::string pipeline =
        enc({"SET", "p1", "v1"}) +
        enc({"SET", "p2", "v2"}) +
        enc({"GET", "p1"}) +
        enc({"MGET", "p1", "p2"}) +
        enc({"DEL", "p1", "p2"});

    RespParser parser;
    size_t offset = 0;
    int commands = 0;

    while (offset < pipeline.size()) {
        size_t consumed = 0;
        auto r = parser.feed(pipeline.data() + offset,
                             pipeline.size() - offset, consumed);
        if (r == RespParser::Result::NEED_MORE) break;
        ASSERT_EQ(r, RespParser::Result::OK);

        std::string response;
        CmdContext ctx;
        ctx.engine = &engine;
        ctx.args = parser.args();
        ctx.response = &response;
        dispatchCommand(ctx);
        ++commands;
        offset += consumed;
        parser.reset();
    }
    EXPECT_EQ(commands, 5);
    EXPECT_EQ(engine.size(), 0u);
}
