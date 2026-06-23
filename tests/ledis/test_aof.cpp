#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "temp_file.h"
#include "ledis/replication/aof_writer.h"
#include "ledis/protocol/resp_parser.h"

using namespace ledis;
using namespace kvtest;

TEST(AofTest, AppendAndReadBack) {
    TempFile tmp("/tmp/kvstore_aof_XXXXXX");
    AofWriter writer(tmp.path(), AofWriter::NO);
    ASSERT_TRUE(writer.start());

    lstl::vector<std::string_view> args;
    args.push_back("SET");
    args.push_back("k");
    args.push_back("v");
    writer.appendArgs(args);
    writer.stop();

    // 读取文件验证 RESP 格式
    FILE* f = fopen(tmp.path().c_str(), "r");
    ASSERT_NE(f, nullptr);
    char buf[256] = {};
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    ASSERT_GT(n, 0u);
    std::string content(buf, n);
    EXPECT_NE(content.find("*3"), std::string::npos);
    EXPECT_NE(content.find("SET"), std::string::npos);
}

TEST(AofTest, EmptyPathNoOp) {
    AofWriter writer;
    EXPECT_TRUE(writer.start());
    lstl::vector<std::string_view> args{"PING"};
    writer.appendArgs(args);
    writer.stop();
}

TEST(AofTest, FsyncModeEverysec) {
    TempFile tmp("/tmp/kvstore_aof_es_XXXXXX");
    AofWriter writer(tmp.path(), AofWriter::EVERYSEC);
    ASSERT_TRUE(writer.start());
    lstl::vector<std::string_view> args{"SET", "a", "b"};
    writer.appendArgs(args);
    writer.stop();
}

TEST(AofTest, FsyncModeAlways) {
    TempFile tmp("/tmp/kvstore_aof_al_XXXXXX");
    AofWriter writer(tmp.path(), AofWriter::ALWAYS);
    ASSERT_TRUE(writer.start());
    EXPECT_FALSE(writer.path().empty());
    EXPECT_GE(writer.fd(), 0);
    lstl::vector<std::string_view> args{"SET", "x", "y"};
    writer.appendArgs(args);
    writer.setMode(AofWriter::NO);
    writer.stop();
}

TEST(AofTest, FsyncLoopEverysec) {
    TempFile tmp("/tmp/kvstore_aof_loop_XXXXXX");
    AofWriter writer(tmp.path(), AofWriter::EVERYSEC);
    ASSERT_TRUE(writer.start());
    EXPECT_GE(writer.fd(), 0);
    lstl::vector<std::string_view> args{"PING"};
    writer.appendArgs(args);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    writer.stop();
}
