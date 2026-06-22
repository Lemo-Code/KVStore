#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <lstl/container/vector.h>

#include "ledis/protocol/resp_writer.h"
#include "ledis/core/value.h"

namespace ledis {

class StorageEngine;

// ============================================================
// CmdContext — 命令执行上下文
// ============================================================
struct CmdContext {
    StorageEngine*                  engine = nullptr;
    lstl::vector<std::string_view>  args;
    std::string*                    response = nullptr;

    uint64_t client_id = 0;
    int      db_index  = 0;
    bool     is_write  = false;
    bool     authenticated = true;
    bool     block_for_stream = false;
    int64_t  block_ms = 0;
    lstl::vector<std::string> block_keys;

    // 便捷回复
    void replyOK()               { RespWriter::writeOK(*response); }
    void replyPong()             { RespWriter::writePong(*response); }
    void replyNull()             { RespWriter::writeNull(*response); }
    void replyInteger(int64_t v) { RespWriter::writeInteger(*response, v); }
    void replyBulk(std::string_view s) { RespWriter::writeBulkString(*response, s); }
    void replySimpleString(std::string_view s) { RespWriter::writeSimpleString(*response, s); }
    void replyError(std::string_view msg) { RespWriter::writeError(*response, msg); }
    void replyStringArray(const lstl::vector<std::string>& arr) { RespWriter::writeStringArray(*response, arr); }
    void replyEmptyArray() { RespWriter::writeEmptyArray(*response); }
    void replyNullArray()  { RespWriter::writeNullArray(*response); }
    void replyWrongType()  { RespWriter::writeErrorWrongType(*response); }
    void replyNotInteger() { RespWriter::writeErrorNotInteger(*response); }
    void replyNoSuchKey()  { RespWriter::writeErrorNoSuchKey(*response); }
    void replyQueued()     { RespWriter::writeQueued(*response); }

    void replyUnknownCmd(std::string_view cmd) {
        RespWriter::writeErrorUnknownCmd(*response, cmd);
    }
    void replyWrongArgs(std::string_view cmd) {
        RespWriter::writeErrorWrongArgs(*response, cmd);
    }

    static uint64_t nowMs() {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME_COARSE, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000ULL
             + static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL;
    }
};

// ============================================================
// 命令标志
// ============================================================
enum CmdFlags : uint32_t {
    CMD_READONLY = 1u << 0,
    CMD_WRITE    = 1u << 1,
    CMD_FAST     = 1u << 2,
    CMD_DENYOOM  = 1u << 3,
    CMD_NOSCRIPT = 1u << 4,
    CMD_PUBSUB   = 1u << 5,
};

using CmdHandler = void (*)(CmdContext& ctx);

// ============================================================
// CmdInfo — 命令注册信息
// ============================================================
struct CmdInfo {
    const char* name;
    CmdHandler  handler;
    int         arity;        // >0=精确个数, -n=至少n个
    uint32_t    flags;
    const char* sflags;

    bool checkArity(int argc) const {
        return (arity > 0) ? (argc == arity) : (argc >= -arity);
    }
};

// ============================================================
// 命令注册表接口 (实现在 command.cc)
// ============================================================

void registerCommand(const char* name, CmdHandler handler,
                     int arity, uint32_t flags, const char* sflags);

const CmdInfo* lookupCommand(std::string_view name);
void dispatchCommand(CmdContext& ctx);
void initCommandTable();

} // namespace ledis
