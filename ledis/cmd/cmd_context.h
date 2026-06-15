#pragma once

#include <ctime>
#include <cstdint>
#include <string>
#include <string_view>
#include <lstl/container/vector.h>

#include "ledis/protocol/resp_writer.h"
#include "ledis/storage/value.h"

namespace ledis {

// ============================================================
// CmdContext — 命令执行上下文
// ============================================================
//
// 每个命令执行时创建，承载输入参数和输出缓冲区的引用。
// 提供便捷的 reply 方法，统一回复格式。
//

// 前向声明
struct ClientContext;
class StorageEngine;

struct CmdContext {
    // 输入
    ClientContext*                  client = nullptr;
    StorageEngine*                  engine = nullptr;
    class PubSubManager*            pubsub = nullptr;
    lstl::vector<std::string_view>   args;  // 命令参数 (零拷贝引用)
    std::string*                    response_buf = nullptr;

    // 当前数据库索引 (SELECT 命令)
    int db_index = 0;

    // 是否为写命令 (用于 AOF 过滤)
    bool is_write = false;

    // ---- 便捷回复方法 ----

    void replyOK() {
        RespWriter::writeOK(*response_buf);
    }

    void replyPong() {
        RespWriter::writePong(*response_buf);
    }

    void replyNull() {
        RespWriter::writeNull(*response_buf);
    }

    void replyInteger(int64_t v) {
        RespWriter::writeInteger(*response_buf, v);
    }

    void replyBulk(std::string_view s) {
        RespWriter::writeBulkString(*response_buf, s);
    }

    void replyBulk(const std::string& s) {
        RespWriter::writeBulkString(*response_buf, s);
    }

    void replySimpleString(const std::string& s) {
        RespWriter::writeSimpleString(*response_buf, s);
    }

    void replyError(const std::string& msg) {
        RespWriter::writeError(*response_buf, msg);
    }

    void replyStringArray(const lstl::vector<std::string>& arr) {
        RespWriter::writeStringArray(*response_buf, arr);
    }

    void replyEmptyArray() {
        RespWriter::writeEmptyArray(*response_buf);
    }

    void replyNullArray() {
        RespWriter::writeNullArray(*response_buf);
    }

    // ---- 常用错误 ----

    void replyUnknownCommand(std::string_view cmd) {
        std::string msg = "ERR unknown command '";
        msg.append(cmd);
        msg += "'";
        replyError(msg);
    }

    void replyWrongArgCount(std::string_view cmd) {
        std::string msg = "ERR wrong number of arguments for '";
        msg.append(cmd);
        msg += "' command";
        replyError(msg);
    }

    void replyWrongType() {
        constexpr std::string_view msg = "WRONGTYPE Operation against a key holding the wrong kind of value";
        response_buf->reserve(response_buf->size() + msg.size() + 4);
        *response_buf += '-';
        response_buf->append(msg);
        *response_buf += "\r\n";
    }

    void replyOutOfMemory() {
        response_buf->append(resp::ERR_OOM.data(), resp::ERR_OOM.size());
    }

    // ---- 获取当前时间 (ms) ----
    uint64_t nowMs() const {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME_COARSE, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000ULL
             + static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL;
    }
};

} // namespace ledis
