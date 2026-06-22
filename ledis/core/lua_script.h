#pragma once

// ============================================================
// LuaScriptEngine — LuaJIT 脚本执行引擎
// ============================================================
// 支持 EVAL / EVALSHA / SCRIPT LOAD / SCRIPT FLUSH / SCRIPT EXISTS
//
// 用法:
//   LuaScriptEngine lua(engine);
//   lua.eval(script, keys, argv, response);
//

#include <string>
#include <lstl/container/vector.h>
#include <lstl/container/unordered_map.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include "ledis/core/storage_engine.h"
#include "ledis/protocol/resp_writer.h"

namespace ledis {

// SHA1 简化版 (用于 EVALSHA)
static std::string sha1Simple(const std::string& s) {
    // 简化: 使用 std::hash 作为脚本标识
    return std::to_string(std::hash<std::string>{}(s));
}

class LuaScriptEngine {
public:
    LuaScriptEngine(StorageEngine* engine) : engine_(engine) {
        lua_ = luaL_newstate();
        luaL_openlibs(lua_);

        // 注册 redis.call / redis.pcall
        lua_newtable(lua_);
        lua_pushcfunction(lua_, luaRedisCall);
        lua_setfield(lua_, -2, "call");
        lua_pushcfunction(lua_, luaRedisPCall);
        lua_setfield(lua_, -2, "pcall");
        lua_setglobal(lua_, "redis");

        // 存储 engine 指针到 Lua registry
        lua_pushlightuserdata(lua_, engine_);
        lua_setfield(lua_, LUA_REGISTRYINDEX, "ledis_engine");

        // 存储当前 response buffer
        lua_pushlightuserdata(lua_, &response_buf_);
        lua_setfield(lua_, LUA_REGISTRYINDEX, "ledis_response");

        // 存储当前 ctx
        lua_pushlightuserdata(lua_, &current_ctx_);
        lua_setfield(lua_, LUA_REGISTRYINDEX, "ledis_ctx");
    }

    ~LuaScriptEngine() { if (lua_) lua_close(lua_); }

    // EVAL script numkeys key [key...] arg [arg...]
    void eval(const lstl::vector<std::string_view>& args, std::string& out) {
        std::string script(args[1]);
        int64_t numkeys = 0;
        try { numkeys = std::stoll(std::string(args[2])); } catch (...) {}

        lstl::vector<std::string> keys, argv;
        size_t i = 3;
        for (int64_t k = 0; k < numkeys && i < args.size(); ++k, ++i)
            keys.push_back(std::string(args[i]));
        for (; i < args.size(); ++i)
            argv.push_back(std::string(args[i]));

        response_buf_ = &out;
        execScript(script, keys, argv, out);
    }

    // SCRIPT LOAD
    void scriptLoad(const lstl::vector<std::string_view>& args, std::string& out) {
        std::string script(args[2]);
        std::string sha = sha1Simple(script);
        scripts_[sha] = script;
        RespWriter::writeBulkString(out, sha);
    }

    // SCRIPT EXISTS
    void scriptExists(const lstl::vector<std::string_view>& args, std::string& out) {
        RespWriter::writeArrayHeader(out, static_cast<int64_t>(args.size() - 2));
        for (size_t i = 2; i < args.size(); ++i) {
            auto it = scripts_.find(std::string(args[i]));
            RespWriter::writeInteger(out, it != scripts_.end() ? 1 : 0);
        }
    }

    // SCRIPT FLUSH
    void scriptFlush(std::string& out) {
        scripts_.clear();
        out += "+OK\r\n";
    }

    // EVALSHA
    void evalsha(const lstl::vector<std::string_view>& args, std::string& out) {
        auto it = scripts_.find(std::string(args[1]));
        if (it == scripts_.end()) {
            out += "-NOSCRIPT No matching script. Please use EVAL.\r\n";
            return;
        }
        response_buf_ = &out;
        int64_t numkeys = 0;
        try { numkeys = std::stoll(std::string(args[2])); } catch (...) {}

        lstl::vector<std::string> keys, argv;
        size_t i = 3;
        for (int64_t k = 0; k < numkeys && i < args.size(); ++k, ++i)
            keys.push_back(std::string(args[i]));
        for (; i < args.size(); ++i)
            argv.push_back(std::string(args[i]));

        execScript(it->second, keys, argv, out);
    }

private:
    void execScript(const std::string& script,
                    const lstl::vector<std::string>& keys,
                    const lstl::vector<std::string>& argv,
                    std::string& out) {
        // 加载脚本
        if (luaL_loadstring(lua_, script.c_str()) != LUA_OK) {
            out += "-ERR ";
            out += lua_tostring(lua_, -1);
            out += "\r\n";
            lua_pop(lua_, 1);
            return;
        }

        // 设置 KEYS 和 ARGV
        lua_newtable(lua_);
        for (size_t i = 0; i < keys.size(); ++i) {
            lua_pushstring(lua_, keys[i].c_str());
            lua_rawseti(lua_, -2, static_cast<int>(i + 1));
        }
        lua_setglobal(lua_, "KEYS");

        lua_newtable(lua_);
        for (size_t i = 0; i < argv.size(); ++i) {
            lua_pushstring(lua_, argv[i].c_str());
            lua_rawseti(lua_, -2, static_cast<int>(i + 1));
        }
        lua_setglobal(lua_, "ARGV");

        // 执行
        current_ctx_.response = &out;
        current_ctx_.engine = engine_;

        if (lua_pcall(lua_, 0, 1, 0) != LUA_OK) {
            out += "-ERR ";
            out += lua_tostring(lua_, -1);
            out += "\r\n";
            lua_pop(lua_, 1);
            return;
        }

        // 转换返回值
        luaToResp(lua_, -1, out);
        lua_pop(lua_, 1);
    }

    static void luaToResp(lua_State* L, int idx, std::string& out) {
        int t = lua_type(L, idx);
        switch (t) {
        case LUA_TNIL:
            RespWriter::writeNull(out);
            break;
        case LUA_TBOOLEAN:
            RespWriter::writeInteger(out, lua_toboolean(L, idx));
            break;
        case LUA_TNUMBER:
            RespWriter::writeInteger(out, static_cast<int64_t>(lua_tonumber(L, idx)));
            break;
        case LUA_TSTRING:
            RespWriter::writeBulkString(out, lua_tostring(L, idx));
            break;
        case LUA_TTABLE: {
            // 数组 → RESP array
            int count = 0;
            lua_pushnil(L);
            while (lua_next(L, idx - 1) != 0) {
                count++;
                lua_pop(L, 1);
            }
            RespWriter::writeArrayHeader(out, count);
            lua_pushnil(L);
            while (lua_next(L, idx - 1) != 0) {
                luaToResp(L, -1, out);
                lua_pop(L, 1);
            }
            break;
        }
        default:
            RespWriter::writeOK(out);
            break;
        }
    }

    static int luaRedisCall(lua_State* L) {
        return luaRedisCommand(L, false);
    }

    static int luaRedisPCall(lua_State* L) {
        return luaRedisCommand(L, true);
    }

    static int luaRedisCommand(lua_State* L, bool pcall) {
        lua_getfield(L, LUA_REGISTRYINDEX, "ledis_engine");
        auto* engine = static_cast<StorageEngine*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, LUA_REGISTRYINDEX, "ledis_response");
        auto* out = static_cast<std::string*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        int nargs = lua_gettop(L);
        if (nargs < 1) { lua_pushnil(L); return 1; }

        // 收集所有参数到持久化存储
        lua_args_.clear();
        for (int i = 1; i <= nargs; ++i) {
            size_t len;
            const char* s = lua_tolstring(L, i, &len);
            lua_args_.push_back(std::string(s, len));
        }

        // 构建 CmdContext
        lstl::vector<std::string_view> sv;
        for (auto& a : lua_args_) sv.push_back(a);

        std::string rsp;
        CmdContext ctx;
        ctx.engine = engine;
        ctx.args = sv;
        ctx.response = &rsp;
        dispatchCommand(ctx);

        // 解析 RESP 响应转为 Lua 值
        respToLua(L, rsp);
        return 1;
    }

    static void respToLua(lua_State* L, const std::string& resp) {
        if (resp.empty()) { lua_pushnil(L); return; }
        char type = resp[0];
        switch (type) {
        case '+': case '-': {
            // Simple string / Error
            size_t end = resp.find("\r\n");
            lua_pushstring(L, resp.substr(1, end - 1).c_str());
            break;
        }
        case ':': {
            // Integer
            size_t end = resp.find("\r\n");
            lua_pushinteger(L, std::stoll(resp.substr(1, end - 1)));
            break;
        }
        case '$': {
            // Bulk string
            if (resp == "$-1\r\n") { lua_pushboolean(L, 0); break; }
            size_t nend = resp.find("\r\n");
            int64_t len = std::stoll(resp.substr(1, nend - 1));
            if (len < 0) { lua_pushboolean(L, 0); break; }
            lua_pushstring(L, resp.substr(nend + 2, static_cast<size_t>(len)).c_str());
            break;
        }
        case '*': {
            // Array
            if (resp == "*-1\r\n") { lua_pushboolean(L, 0); break; }
            size_t nend = resp.find("\r\n");
            int64_t count = std::stoll(resp.substr(1, nend - 1));
            lua_newtable(L);
            size_t pos = nend + 2;
            for (int64_t i = 0; i < count; ++i) {
                // 递归解析子元素
                std::string sub;
                char st = resp[pos];
                if (st == '$') {
                    size_t sl = resp.find("\r\n", pos);
                    int64_t slen = std::stoll(resp.substr(pos + 1, sl - pos - 1));
                    sub = resp.substr(pos, sl + 2 + static_cast<size_t>(slen) + 2);
                } else if (st == ':' || st == '+' || st == '-') {
                    size_t sl = resp.find("\r\n", pos);
                    sub = resp.substr(pos, sl - pos + 2);
                } else if (st == '*') {
                    // 嵌套数组: 简化跳过
                    sub = "$-1\r\n";
                }
                respToLua(L, sub);
                lua_rawseti(L, -2, i + 1);
            }
            break;
        }
        default:
            lua_pushstring(L, resp.c_str());
        }
    }

    static thread_local lstl::vector<std::string> lua_args_;

    lua_State* lua_ = nullptr;
    StorageEngine* engine_;
    std::string* response_buf_ = nullptr;
    CmdContext current_ctx_;
    lstl::unordered_map<std::string, std::string> scripts_;
};

thread_local lstl::vector<std::string> LuaScriptEngine::lua_args_;

} // namespace ledis
