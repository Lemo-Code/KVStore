#pragma once

#include <cstdint>
#include <string_view>

namespace ledis {
namespace resp {

// ============================================================
// RESP2 协议类型字节
// ============================================================
constexpr char TYPE_SIMPLE  = '+';
constexpr char TYPE_ERROR   = '-';
constexpr char TYPE_INTEGER = ':';
constexpr char TYPE_BULK    = '$';
constexpr char TYPE_ARRAY   = '*';

constexpr std::string_view CRLF = "\r\n";

constexpr int64_t NULL_BULK_LEN  = -1;
constexpr int64_t NULL_ARRAY_LEN = -1;

// 常用回复
constexpr std::string_view REPLY_OK     = "+OK\r\n";
constexpr std::string_view REPLY_PONG   = "+PONG\r\n";
constexpr std::string_view REPLY_NULL   = "$-1\r\n";
constexpr std::string_view REPLY_ZERO   = ":0\r\n";
constexpr std::string_view REPLY_ONE    = ":1\r\n";
constexpr std::string_view REPLY_QUEUED = "+QUEUED\r\n";

// 错误消息
constexpr std::string_view ERR_WRONG_TYPE   = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
constexpr std::string_view ERR_NOT_INTEGER  = "-ERR value is not an integer or out of range\r\n";
constexpr std::string_view ERR_NOT_FLOAT    = "-ERR value is not a valid float\r\n";
constexpr std::string_view ERR_SYNTAX       = "-ERR syntax error\r\n";
constexpr std::string_view ERR_NO_SUCH_KEY  = "-ERR no such key\r\n";
constexpr std::string_view ERR_OUT_OF_RANGE = "-ERR index out of range\r\n";
constexpr std::string_view ERR_OFFSET_RANGE = "-ERR offset is out of range\r\n";
constexpr std::string_view ERR_NOAUTH       = "-NOAUTH Authentication required.\r\n";
constexpr std::string_view ERR_INVALID_PWD  = "-ERR invalid password\r\n";
constexpr std::string_view ERR_EXEC_NO_MULTI = "-ERR EXEC without MULTI\r\n";

} // namespace resp
} // namespace ledis
