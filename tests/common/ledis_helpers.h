#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include <lstl/container/vector.h>
#include "ledis/core/command.h"
#include "ledis/core/storage_engine.h"

namespace kvtest {

inline void ensureCommandTable() {
    static bool done = false;
    if (!done) {
        ledis::initCommandTable();
        done = true;
    }
}

inline lstl::vector<std::string_view> makeArgs(
        std::initializer_list<const char*> args) {
    lstl::vector<std::string_view> out;
    out.reserve(args.size());
    for (auto* a : args) out.push_back(a);
    return out;
}

inline std::string runCommand(ledis::StorageEngine& engine,
                              std::initializer_list<const char*> args) {
    ensureCommandTable();
    auto arg_views = makeArgs(args);
    std::string response;
    ledis::CmdContext ctx;
    ctx.engine = &engine;
    ctx.args = arg_views;
    ctx.response = &response;
    ledis::dispatchCommand(ctx);
    return response;
}

inline std::string extractBulk(const std::string& resp) {
    if (resp.size() < 4 || resp[0] != '$') return {};
    auto crlf = resp.find("\r\n");
    if (crlf == std::string::npos) return {};
    int64_t len = std::stoll(resp.substr(1, crlf - 1));
    if (len < 0) return {};
    size_t start = crlf + 2;
    if (start + static_cast<size_t>(len) + 2 > resp.size()) return {};
    return resp.substr(start, static_cast<size_t>(len));
}

inline int64_t extractInteger(const std::string& resp) {
    if (resp.empty() || resp[0] != ':') return 0;
    return std::stoll(resp.substr(1));
}

inline bool isOK(const std::string& resp) {
    return resp == "+OK\r\n";
}

inline bool isNull(const std::string& resp) {
    return resp == "$-1\r\n";
}

inline std::string extractSimple(const std::string& resp) {
    if (resp.size() < 3 || resp[0] != '+') return {};
    auto crlf = resp.find("\r\n");
    if (crlf == std::string::npos) return {};
    return resp.substr(1, crlf - 1);
}

} // namespace kvtest
