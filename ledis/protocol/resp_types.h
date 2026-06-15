#pragma once

#include <cstdint>
#include <string_view>

namespace ledis {

// ============================================================
// RESP 协议常量
// ============================================================
// RESP (REdis Serialization Protocol) 定义 5 种数据类型:
//   +  Simple String   → +OK\r\n
//   -  Error           → -ERR message\r\n
//   :  Integer         → :1000\r\n
//   $  Bulk String     → $5\r\nhello\r\n   ($-1\r\n = null)
//   *  Array           → *2\r\n...\r\n     (*-1\r\n = null)
//
// 其他:
//   客户端命令总是以 Array of Bulk Strings 形式发送
//   内联命令 (空格分隔) 也可选支持，但对标 redis-cli 使用 RESP

namespace resp {

// 类型标识字节
inline constexpr char TYPE_SIMPLE  = '+';
inline constexpr char TYPE_ERROR   = '-';
inline constexpr char TYPE_INTEGER = ':';
inline constexpr char TYPE_BULK    = '$';
inline constexpr char TYPE_ARRAY   = '*';

// 行结束符
inline constexpr std::string_view CRLF = "\r\n";

// Null 标记
inline constexpr int64_t NULL_BULK_LEN  = -1;
inline constexpr int64_t NULL_ARRAY_LEN = -1;

// 常见回复
inline constexpr std::string_view REPLY_OK   = "+OK\r\n";
inline constexpr std::string_view REPLY_PONG = "+PONG\r\n";
inline constexpr std::string_view REPLY_NULL = "$-1\r\n";
inline constexpr std::string_view REPLY_ZERO = ":0\r\n";
inline constexpr std::string_view REPLY_ONE  = ":1\r\n";

// 常见错误回复前缀
inline constexpr std::string_view ERR_UNKNOWN_CMD  = "-ERR unknown command '";
inline constexpr std::string_view ERR_WRONG_ARGS   = "-ERR wrong number of arguments for '";
inline constexpr std::string_view ERR_WRONG_TYPE   = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
inline constexpr std::string_view ERR_NO_SUCH_KEY  = "-ERR no such key\r\n";
inline constexpr std::string_view ERR_VALUE_NOT_INT= "-ERR value is not an integer or out of range\r\n";
inline constexpr std::string_view ERR_OOM          = "-ERR out of memory\r\n";

} // namespace resp
} // namespace ledis
