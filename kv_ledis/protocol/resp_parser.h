#pragma once

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <lstl/container/vector.h>

#include "kv_ledis/protocol/resp_types.h"

namespace ledis {

// ============================================================
// RespParser — 流式 RESP 协议解析器 (零拷贝)
// ============================================================
//
// 状态机驱动，支持 pipeline。
// args() 中的 string_view 指向原始 buffer。
//
class RespParser {
public:
    enum class Result : uint8_t { OK, NEED_MORE, ERROR };

    RespParser() { reset(); }

    Result feed(const char* data, size_t len, size_t& consumed);

    const lstl::vector<std::string_view>& args() const { return args_; }
    const std::string& errorMsg() const    { return error_msg_; }

    void reset();
    bool isIdle() const { return state_ == State::TOP_LEVEL && array_counts_.empty(); }

private:
    enum class State : uint8_t {
        TOP_LEVEL,
        READING_BULK_LEN,
        READING_BULK_DATA,
        READING_ARRAY_LEN,
        READING_SIMPLE,
    };

    static const char* findCRLF(const char* ptr, size_t len);
    static bool parseLen(const char* start, const char* end, int64_t& out);

    State  state_ = State::TOP_LEVEL;
    lstl::vector<int>    array_counts_;
    int64_t              bulk_len_ = 0;
    lstl::vector<std::string_view> args_;
    const char*          cursor_ = nullptr;
    std::string          error_msg_;
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
    for (size_t i = 0; i + 1 < len; ++i)
        if (ptr[i] == '\r' && ptr[i + 1] == '\n') return ptr + i;
    return nullptr;
}

inline bool RespParser::parseLen(const char* start, const char* end, int64_t& out) {
    if (start >= end) return false;
    bool neg = (*start == '-');
    if (neg) ++start;
    int64_t val = 0;
    for (const char* p = start; p < end; ++p) {
        if (*p < '0' || *p > '9') return false;
        val = val * 10 + (*p - '0');
        if (val < 0) return false;
    }
    out = neg ? -val : val;
    return true;
}

inline RespParser::Result RespParser::feed(const char* data, size_t len,
                                             size_t& consumed) {
    consumed = 0;
    if (len == 0) return Result::NEED_MORE;

    const char* ptr = data;
    const char* end = data + len;

    while (ptr < end) {
        switch (state_) {

        case State::TOP_LEVEL: {
            char type = *ptr;
            switch (type) {
            case resp::TYPE_ARRAY:
                state_ = State::READING_ARRAY_LEN;
                ptr++; cursor_ = ptr;
                break;
            case resp::TYPE_BULK:
                state_ = State::READING_BULK_LEN;
                ptr++; cursor_ = ptr;
                break;
            case resp::TYPE_SIMPLE:
            case resp::TYPE_ERROR:
            case resp::TYPE_INTEGER:
                state_ = State::READING_SIMPLE;
                ptr++; cursor_ = ptr;
                break;
            default:
                error_msg_ = "Protocol error: unexpected type byte";
                return Result::ERROR;
            }
            break;
        }

        case State::READING_ARRAY_LEN: {
            const char* crlf = findCRLF(ptr, end - ptr);
            if (!crlf) { consumed = ptr - data; return Result::NEED_MORE; }

            int64_t n;
            if (!parseLen(cursor_, crlf, n) || n < -1) {
                error_msg_ = "Protocol error: invalid array length";
                return Result::ERROR;
            }
            if (n == -1) {
                error_msg_ = "Protocol error: null array in command";
                return Result::ERROR;
            }
            if (n == 0) {
                state_ = State::TOP_LEVEL;
                args_.clear();
            } else {
                array_counts_.push_back(static_cast<int>(n));
                args_.clear();
                args_.reserve(static_cast<size_t>(n));
                state_ = State::TOP_LEVEL;
            }
            ptr = crlf + 2; cursor_ = ptr;
            break;
        }

        case State::READING_BULK_LEN: {
            const char* crlf = findCRLF(ptr, end - ptr);
            if (!crlf) { consumed = ptr - data; return Result::NEED_MORE; }

            if (!parseLen(cursor_, crlf, bulk_len_) || bulk_len_ < -1) {
                error_msg_ = "Protocol error: invalid bulk length";
                return Result::ERROR;
            }

            ptr = crlf + 2;

            if (bulk_len_ == -1) {
                args_.push_back(std::string_view{});
                bulk_len_ = 0; cursor_ = ptr;
                if (!array_counts_.empty()) {
                    array_counts_.back()--;
                    if (array_counts_.back() == 0) {
                        array_counts_.pop_back();
                        if (array_counts_.empty()) {
                            state_ = State::TOP_LEVEL;
                            consumed = ptr - data;
                            return Result::OK;
                        }
                    }
                }
                break;
            }
            state_ = State::READING_BULK_DATA;
            cursor_ = ptr;
            break;
        }

        case State::READING_BULK_DATA: {
            size_t remaining = end - ptr;
            size_t needed = static_cast<size_t>(bulk_len_) + 2;
            if (remaining < needed) {
                consumed = ptr - data;
                return Result::NEED_MORE;
            }

            if (ptr[bulk_len_] != '\r' || ptr[bulk_len_ + 1] != '\n') {
                error_msg_ = "Protocol error: expected CRLF after bulk data";
                return Result::ERROR;
            }

            if (bulk_len_ > 0)
                args_.push_back(std::string_view(ptr, static_cast<size_t>(bulk_len_)));
            else
                args_.push_back(std::string_view{});

            ptr += needed; cursor_ = ptr; bulk_len_ = 0;

            if (!array_counts_.empty()) {
                array_counts_.back()--;
                if (array_counts_.back() == 0) {
                    array_counts_.pop_back();
                    if (array_counts_.empty()) {
                        state_ = State::TOP_LEVEL;
                        consumed = ptr - data;
                        return Result::OK;
                    }
                }
            }
            state_ = State::TOP_LEVEL;
            break;
        }

        case State::READING_SIMPLE: {
            const char* crlf = findCRLF(ptr, end - ptr);
            if (!crlf) { consumed = ptr - data; return Result::NEED_MORE; }
            args_.push_back(std::string_view(cursor_, crlf - cursor_));
            ptr = crlf + 2; cursor_ = ptr;
            state_ = State::TOP_LEVEL;
            consumed = ptr - data;
            return Result::OK;
        }

        } // switch
    }

    consumed = ptr - data;
    return Result::NEED_MORE;
}

} // namespace ledis
