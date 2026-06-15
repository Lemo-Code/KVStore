#pragma once

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <lstl/container/vector.h>

#include "ledis/protocol/resp_types.h"

namespace ledis {

// ============================================================
// RespParser — 流式 RESP 协议解析器 (零拷贝)
// ============================================================
//
// 状态机驱动，边收数据边解析。
// feed() 返回后 args_ 中的 string_view 指向原始 buffer 数据，
// 调用者必须在下一个 feed() 之前处理完命令并写回复。
//
// 支持 pipeline: 一个 TCP 段中包含多个命令时，循环调用 feed()
// 直到返回 NEED_MORE 或 PROTO_ERR。
//
// 用法:
//   RespParser parser;
//   while (true) {
//       n = stream.read(buf);
//       for (;;) {
//           size_t consumed;
//           auto r = parser.feed(buf, len, consumed);
//           if (r == RespParser::NEED_MORE) break;
//           if (r == RespParser::ERROR)   handle_error();
//           handle_command(parser.args());
//       }
//   }
//
class RespParser {
public:
    enum class Result : uint8_t {
        OK,         // 解析完成一个完整命令, args() 可用
        NEED_MORE,  // 数据不足，需更多字节
        ERROR,      // 协议错误
    };

    RespParser() { reset(); }

    // 喂入数据
    // @param data     接收缓冲区起始地址
    // @param len      缓冲区有效数据长度
    // @param consumed 输出: 此次解析消耗的字节数 (用于移动读写指针)
    Result feed(const char* data, size_t len, size_t& consumed);

    // 获取解析完成的命令参数 (仅在 OK 后有效)
    const lstl::vector<std::string_view>& args() const { return args_; }

    // 重置解析器，复用对象
    void reset();

    // 查询解析器状态
    bool isIdle() const { return state_ == State::TOP_LEVEL; }

    // 错误信息 (仅在 ERROR 后有效)
    const std::string& errorMsg() const { return error_msg_; }

private:
    enum class State : uint8_t {
        TOP_LEVEL,          // 等待命令/类型的首字节
        READING_BULK_LEN,   // 读 $<len>\r\n
        READING_BULK_DATA,  // 读 <len> bytes 然后 \r\n
        READING_ARRAY_LEN,  // 读 *<len>\r\n
        READING_SIMPLE,     // 读简单字符串直到 \r\n (+ 或 - 或 :)
    };

    // 查找 CRLF 的位置
    // 返回 nullptr 表示在 [ptr, ptr+len) 范围内未找到
    static const char* findCRLF(const char* ptr, size_t len);

    // 解析有符号整数，返回是否成功
    static bool parseLen(const char* start, const char* end, int64_t& out);

    State  state_ = State::TOP_LEVEL;

    // 嵌套数组栈
    // array_counts_ 记录每层数组还有多少个元素待读取
    lstl::vector<int> array_counts_;

    // 当前正在读取的 bulk string 剩余字节数
    int64_t bulk_len_ = 0;

    // 解析结果
    lstl::vector<std::string_view> args_;

    // 指针追踪
    const char* cursor_ = nullptr;  // 当前消费位置

    // 错误信息
    std::string error_msg_;
};

// ============================================================
// 实现
// ============================================================

inline void RespParser::reset() {
    state_ = State::TOP_LEVEL;
    array_counts_.clear();
    bulk_len_ = 0;
    args_.clear();
    cursor_ = nullptr;
    error_msg_.clear();
}

inline const char* RespParser::findCRLF(const char* ptr, size_t len) {
    for (size_t i = 0; i + 1 < len; ++i) {
        if (ptr[i] == '\r' && ptr[i + 1] == '\n') return ptr + i;
    }
    return nullptr;
}

inline bool RespParser::parseLen(const char* start, const char* end, int64_t& out) {
    if (start >= end) return false;

    bool neg = false;
    if (*start == '-') {
        neg = true;
        start++;
    }

    int64_t val = 0;
    for (const char* p = start; p < end; ++p) {
        if (*p < '0' || *p > '9') return false;
        val = val * 10 + (*p - '0');
        if (val < 0) return false;  // overflow
    }

    out = neg ? -val : val;
    return true;
}

inline RespParser::Result RespParser::feed(const char* data, size_t len, size_t& consumed) {
    consumed = 0;

    if (len == 0) return Result::NEED_MORE;

    const char* ptr = data;
    const char* end = data + len;

    while (ptr < end) {
        switch (state_) {

        // ======== TOP_LEVEL: 期待类型首字节 ========
        case State::TOP_LEVEL: {
            char type = *ptr;

            if (type == resp::TYPE_ARRAY) {
                // 客户端命令总是 Array of Bulk Strings
                // 开始解析: *<len>\r\n
                state_ = State::READING_ARRAY_LEN;
                ptr++;  // 消费 '*'
                cursor_ = ptr;  // 记录数字起始位置
                break;
            }

            if (type == resp::TYPE_BULK) {
                // 独立的 Bulk String (非标准但兼容)
                state_ = State::READING_BULK_LEN;
                ptr++;
                cursor_ = ptr;
                break;
            }

            if (type == resp::TYPE_SIMPLE || type == resp::TYPE_ERROR
                || type == resp::TYPE_INTEGER) {
                // 简单类型 (用于内联命令或回复)
                state_ = State::READING_SIMPLE;
                ptr++;
                cursor_ = ptr;
                break;
            }

            // 未知类型
            error_msg_ = "Protocol error: unexpected type byte '";
            error_msg_ += type;
            error_msg_ += "'";
            return Result::ERROR;
        }

        // ======== READING_ARRAY_LEN: 读 *<len>\r\n ========
        case State::READING_ARRAY_LEN: {
            const char* crlf = findCRLF(ptr, end - ptr);
            if (!crlf) {
                // 数据不足，需要等待更多数据
                // cursor_ 保持为数字起始位置
                consumed = ptr - data;
                return Result::NEED_MORE;
            }

            int64_t array_len;
            if (!parseLen(cursor_, crlf, array_len) || array_len < -1) {
                error_msg_ = "Protocol error: invalid array length";
                return Result::ERROR;
            }

            if (array_len == -1) {
                // Null Array — 不应该出现在客户端命令中
                error_msg_ = "Protocol error: null array in command";
                return Result::ERROR;
            }

            if (array_len == 0) {
                // 空数组 → 命令完成 (无参数)
                state_ = State::TOP_LEVEL;
                args_.clear();
            } else if (array_counts_.empty()) {
                // 顶层数组
                array_counts_.push_back(static_cast<int>(array_len));
                args_.clear();
                args_.reserve(static_cast<size_t>(array_len));
                state_ = State::TOP_LEVEL;  // 回到顶层读取第一个元素
            } else {
                // 嵌套数组
                array_counts_.push_back(static_cast<int>(array_len));
                state_ = State::TOP_LEVEL;
            }

            ptr = crlf + 2;  // 消费 \r\n
            cursor_ = ptr;
            break;
        }

        // ======== READING_BULK_LEN: 读 $<len>\r\n ========
        case State::READING_BULK_LEN: {
            const char* crlf = findCRLF(ptr, end - ptr);
            if (!crlf) {
                consumed = ptr - data;
                return Result::NEED_MORE;
            }

            if (!parseLen(cursor_, crlf, bulk_len_) || bulk_len_ < -1) {
                error_msg_ = "Protocol error: invalid bulk length";
                return Result::ERROR;
            }

            ptr = crlf + 2;

            if (bulk_len_ == -1) {
                // Null Bulk String → 空 string_view
                args_.push_back(std::string_view{});
                bulk_len_ = 0;
                cursor_ = ptr;

                // 更新数组计数
                if (!array_counts_.empty()) {
                    array_counts_.back()--;
                    if (array_counts_.back() == 0) {
                        array_counts_.pop_back();
                        if (array_counts_.empty()) {
                            state_ = State::TOP_LEVEL;
                            consumed = ptr - data;
                            return Result::OK;  // 命令完整
                        }
                        state_ = State::TOP_LEVEL;
                    }
                }
                break;
            }

            if (bulk_len_ >= 0) {
                state_ = State::READING_BULK_DATA;
                cursor_ = ptr;
                // Fall through to READING_BULK_DATA check below
            }
            break;
        }

        // ======== READING_BULK_DATA: 读 <len> bytes + \r\n ========
        case State::READING_BULK_DATA: {
            size_t remaining = end - ptr;
            size_t needed = static_cast<size_t>(bulk_len_) + 2;  // data + CRLF

            if (remaining < needed) {
                consumed = ptr - data;
                return Result::NEED_MORE;
            }

            // 验证尾部 CRLF
            if (ptr[bulk_len_] != '\r' || ptr[bulk_len_ + 1] != '\n') {
                error_msg_ = "Protocol error: expected CRLF after bulk data";
                return Result::ERROR;
            }

            // 零拷贝引用
            if (bulk_len_ > 0) {
                args_.push_back(std::string_view(ptr, static_cast<size_t>(bulk_len_)));
            } else {
                args_.push_back(std::string_view{});  // 空 bulk string
            }

            ptr += needed;  // 消费 data + CRLF
            cursor_ = ptr;
            bulk_len_ = 0;

            // 更新数组计数
            if (!array_counts_.empty()) {
                array_counts_.back()--;
                if (array_counts_.back() == 0) {
                    // 当前数组完成
                    array_counts_.pop_back();
                    if (array_counts_.empty()) {
                        // 顶层数组完成 → 命令完整!
                        state_ = State::TOP_LEVEL;
                        consumed = ptr - data;
                        return Result::OK;
                    }
                }
            }

            // 下一个元素
            state_ = State::TOP_LEVEL;
            break;
        }

        // ======== READING_SIMPLE: 读到 \r\n ========
        case State::READING_SIMPLE: {
            const char* crlf = findCRLF(ptr, end - ptr);
            if (!crlf) {
                consumed = ptr - data;
                return Result::NEED_MORE;
            }

            // 存储简单字符串 (不包含 \r\n)
            args_.push_back(std::string_view(cursor_, crlf - cursor_));

            ptr = crlf + 2;
            cursor_ = ptr;
            state_ = State::TOP_LEVEL;

            // 简单命令不在数组中，直接返回
            consumed = ptr - data;
            return Result::OK;
        }

        } // switch
    }

    // 循环结束但没完成 — 需要更多数据
    consumed = ptr - data;
    return Result::NEED_MORE;
}

} // namespace ledis
