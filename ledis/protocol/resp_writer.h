#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <lstl/container/vector.h>

#include "ledis/protocol/resp_types.h"

namespace ledis {

// ============================================================
// RespWriter — RESP 回复序列化器
// ============================================================
//
// 所有静态方法，直接将 RESP 格式写入 std::string
// 线程安全: 调用者负责同步 (每客户端独立 buffer，无竞争)
//
class RespWriter {
public:
    // ---- 简单类型 ----

    static void writeOK(std::string& buf) {
        buf.append(resp::REPLY_OK.data(), resp::REPLY_OK.size());
    }

    static void writePong(std::string& buf) {
        buf.append(resp::REPLY_PONG.data(), resp::REPLY_PONG.size());
    }

    static void writeNull(std::string& buf) {
        buf.append(resp::REPLY_NULL.data(), resp::REPLY_NULL.size());
    }

    static void writeInteger(std::string& buf, int64_t v) {
        char tmp[32];
        int len = snprintf(tmp, sizeof(tmp), ":%ld\r\n", v);
        buf.append(tmp, static_cast<size_t>(len));
    }

    static void writeSimpleString(std::string& buf, const std::string& s) {
        buf += resp::TYPE_SIMPLE;
        buf += s;
        buf.append(resp::CRLF.data(), resp::CRLF.size());
    }

    static void writeError(std::string& buf, const std::string& msg) {
        buf += resp::TYPE_ERROR;
        buf += msg;
        buf.append(resp::CRLF.data(), resp::CRLF.size());
    }

    // ---- Bulk String ----

    static void writeBulkString(std::string& buf, const char* data, size_t len) {
        char header[32];
        int hlen = snprintf(header, sizeof(header), "$%zu\r\n", len);
        buf.append(header, static_cast<size_t>(hlen));
        if (len > 0) {
            buf.append(data, len);
        }
        buf.append(resp::CRLF.data(), resp::CRLF.size());
    }

    static void writeBulkString(std::string& buf, const std::string& s) {
        writeBulkString(buf, s.data(), s.size());
    }

    static void writeBulkString(std::string& buf, std::string_view sv) {
        writeBulkString(buf, sv.data(), sv.size());
    }

    // ---- Array ----

    // 写数组头 (调用后需依次写各元素)
    static void writeArrayHeader(std::string& buf, int64_t len) {
        char header[32];
        int hlen = snprintf(header, sizeof(header), "*%ld\r\n", len);
        buf.append(header, static_cast<size_t>(hlen));
    }

    // 便利方法: 写字符串数组 (常用于 KEYS, SMEMBERS 等)
    static void writeStringArray(std::string& buf,
                                  const lstl::vector<std::string>& arr) {
        writeArrayHeader(buf, static_cast<int64_t>(arr.size()));
        for (const auto& s : arr) {
            writeBulkString(buf, s);
        }
    }

    // 写入 "$<len>\r\n<data>\r\n" (内联版本，减少临时对象)
    static void writeBulkStringInline(std::string& buf, std::string_view s) {
        buf += '$';
        buf += std::to_string(s.size());
        buf += "\r\n";
        buf.append(s.data(), s.size());
        buf += "\r\n";
    }

    // 写空数组
    static void writeEmptyArray(std::string& buf) {
        writeArrayHeader(buf, 0);
    }

    // 写 Null Array
    static void writeNullArray(std::string& buf) {
        buf.append("*-1\r\n", 5);
    }
};

} // namespace ledis
