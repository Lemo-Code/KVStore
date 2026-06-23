#include <gtest/gtest.h>
#include "ledis_helpers.h"
#include "ledis/core/lua_script.h"

using namespace ledis;
using namespace kvtest;

class LuaScriptTest : public ::testing::Test {
protected:
    void SetUp() override { ensureCommandTable(); }
    StorageEngine engine;
    LuaScriptEngine lua{&engine};
    std::string out;

    void evalScript(const char* script, int numkeys = 0,
                    std::initializer_list<const char*> extra = {}) {
        lstl::vector<const char*> parts = {"EVAL", script};
        std::string numkeys_str = std::to_string(numkeys);
        parts.push_back(numkeys_str.c_str());
        for (auto* e : extra) parts.push_back(e);
        lstl::vector<std::string_view> args;
        for (auto* p : parts) args.push_back(p);
        out.clear();
        lua.eval(args, out);
    }
};

TEST_F(LuaScriptTest, EvalReturnTypes) {
    evalScript("return 42");
    EXPECT_EQ(extractInteger(out), 42);

    evalScript("return 'hello'");
    EXPECT_EQ(extractBulk(out), "hello");

    evalScript("return true");
    EXPECT_EQ(extractInteger(out), 1);

    evalScript("return nil");
    EXPECT_TRUE(isNull(out));

    evalScript("return {1, 'two', 3}");
    EXPECT_EQ(out[0], '*');
}

TEST_F(LuaScriptTest, EvalRedisCall) {
    evalScript("return redis.call('set', 'lk', 'lv')");
    EXPECT_TRUE(isOK(out) || extractBulk(out) == "OK" || out[0] == '+');

    evalScript("return redis.call('get', 'lk')");
    EXPECT_EQ(extractBulk(out), "lv");

    evalScript("return redis.pcall('get', 'lk')");
    EXPECT_EQ(extractBulk(out), "lv");
}

TEST_F(LuaScriptTest, ScriptLoadEvalshaExistsFlush) {
    auto load_args = makeArgs({"SCRIPT", "LOAD", "return 99"});
    out.clear();
    lua.scriptLoad(load_args, out);
    std::string sha = extractBulk(out);
    EXPECT_FALSE(sha.empty());

    auto exists_args = makeArgs({"SCRIPT", "EXISTS", sha.c_str()});
    out.clear();
    lua.scriptExists(exists_args, out);
    EXPECT_EQ(out[0], '*');

    auto evalsha_args = makeArgs({"EVALSHA", sha.c_str(), "0"});
    out.clear();
    lua.evalsha(evalsha_args, out);
    EXPECT_EQ(extractInteger(out), 99);

    out.clear();
    lua.scriptFlush(out);
    EXPECT_TRUE(isOK(out));

    out.clear();
    lua.evalsha(evalsha_args, out);
    EXPECT_NE(out.find("NOSCRIPT"), std::string::npos);
}

TEST_F(LuaScriptTest, EvalWithKeysAndArgv) {
    runCommand(engine, {"SET", "mykey", "myval"});
    evalScript("return redis.call('get', KEYS[1])", 1, {"mykey"});
    EXPECT_EQ(extractBulk(out), "myval");

    evalScript("return ARGV[1]", 0, {"argval"});
    EXPECT_EQ(extractBulk(out), "argval");
}

TEST_F(LuaScriptTest, EvalSyntaxError) {
    evalScript("this is not valid lua");
    EXPECT_NE(out.find("ERR"), std::string::npos);
}
