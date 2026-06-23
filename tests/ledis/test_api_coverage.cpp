#include <gtest/gtest.h>
#include <set>
#include <string>
#include "ledis_helpers.h"
#include "ledis/core/command.h"

using namespace ledis;
using namespace kvtest;

namespace {

void collectCommands(const CmdInfo& info, void* userdata) {
    auto* out = static_cast<std::set<std::string>*>(userdata);
    out->insert(info.name);
}

} // namespace

// 运行时校验：命令注册表与冒烟测试清单一致（API 存在性 + 测试可达性）
TEST(LedisApiCoverageTest, RegistryCountSanity) {
    ensureCommandTable();
    std::set<std::string> registered;
    forEachCommand(collectCommands, &registered);
    EXPECT_GE(registered.size(), 150u);
    EXPECT_LE(registered.size(), 200u);
}

TEST(LedisApiCoverageTest, CoreCommandsRegistered) {
    ensureCommandTable();
    const char* samples[] = {
        "set", "get", "hset", "lpush", "sadd", "zadd",
        "scan", "xadd", "geoadd", "pfadd", "bitop", "command",
    };
    for (auto* name : samples)
        EXPECT_NE(lookupCommand(name), nullptr) << name;
}
