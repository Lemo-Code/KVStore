#pragma once

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>

// 分支预测优化
#if defined(__GNUC__) || defined(__clang__)
    #define ZERO_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define ZERO_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define ZERO_LIKELY(x)   (x)
    #define ZERO_UNLIKELY(x) (x)
#endif

// 断言
#define ZERO_ASSERT(cond) \
    assert(cond)

#define ZERO_ASSERT_MSG(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "ASSERTION FAILED: %s\n  at %s:%d\n  msg: %s\n", \
                    #cond, __FILE__, __LINE__, (std::string(msg).c_str())); \
            std::abort(); \
        } \
    } while (0)

namespace zero {

// 获取 backtrace 字符串 (用于 crash dump)
std::string BacktraceToString(int skip = 1);

// 获取当前线程 ID
uint32_t GetThreadId();

// 获取当前时间戳 (毫秒)
uint64_t GetCurrentMS();

} // namespace zero
