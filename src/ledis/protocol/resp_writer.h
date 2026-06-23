#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include <lstl/container/vector.h>

#include "ledis/protocol/resp_types.h"

namespace ledis {

// ============================================================
// RespWriter — RESP 回复序列化器 (零临时分配)
// ============================================================
class RespWriter {
public:
    // 栈上整数转字符串, 追加到 buf (避免 std::to_string 的堆分配)
    static void appendInt(std::string& buf, int64_t v) {
        if (v == 0) { buf += '0'; return; }
        char tmp[24];
        int pos = 23;
        tmp[pos] = '\0';
        bool neg = v < 0;
        uint64_t u = neg ? -static_cast<uint64_t>(v) : static_cast<uint64_t>(v);
        while (u > 0) { tmp[--pos] = static_cast<char>('0' + (u % 10)); u /= 10; }
        if (neg) tmp[--pos] = '-';
        buf.append(tmp + pos, 23 - pos);
    }

    static void appendInt(uint64_t v) {
        // unused for now but might be helpful
    }

    static void writeOK(std::string& buf)     { buf += "+OK\r\n"; }
    static void writePong(std::string& buf)   { buf += "+PONG\r\n"; }
    static void writeNull(std::string& buf)   { buf += "$-1\r\n"; }
    static void writeQueued(std::string& buf) { buf += "+QUEUED\r\n"; }

    static void writeInteger(std::string& buf, int64_t v) {
        buf += ':';
        appendInt(buf, v);
        buf += "\r\n";
    }

    static void writeSimpleString(std::string& buf, std::string_view s) {
        buf += '+';
        buf.append(s.data(), s.size());
        buf += "\r\n";
    }

    static void writeError(std::string& buf, std::string_view msg) {
        buf += '-';
        buf.append(msg.data(), msg.size());
        buf += "\r\n";
    }

    static void writeErrorWrongType(std::string& buf) {
        buf += "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
    }

    static void writeErrorNotInteger(std::string& buf) {
        buf += "-ERR value is not an integer or out of range\r\n";
    }

    static void writeErrorUnknownCmd(std::string& buf, std::string_view cmd) {
        buf += "-ERR unknown command '";
        buf.append(cmd.data(), cmd.size());
        buf += "'\r\n";
    }

    static void writeErrorWrongArgs(std::string& buf, std::string_view cmd) {
        buf += "-ERR wrong number of arguments for '";
        buf.append(cmd.data(), cmd.size());
        buf += "' command\r\n";
    }

    static void writeErrorNoSuchKey(std::string& buf) {
        buf += "-ERR no such key\r\n";
    }

    // ---- Bulk String ----
    static void writeBulkString(std::string& buf, std::string_view s) {
        buf += '$';
        appendInt(buf, static_cast<int64_t>(s.size()));
        buf += "\r\n";
        buf.append(s.data(), s.size());
        buf += "\r\n";
    }

    // ---- Array ----
    static void writeArrayHeader(std::string& buf, int64_t len) {
        buf += '*';
        appendInt(buf, len);
        buf += "\r\n";
    }

    static void writeStringArray(std::string& buf,
                                  const lstl::vector<std::string>& arr) {
        writeArrayHeader(buf, static_cast<int64_t>(arr.size()));
        for (const auto& s : arr) writeBulkString(buf, s);
    }

    static void writeEmptyArray(std::string& buf) { buf += "*0\r\n"; }
    static void writeNullArray(std::string& buf)  { buf += "*-1\r\n"; }
};

} // namespace ledis
