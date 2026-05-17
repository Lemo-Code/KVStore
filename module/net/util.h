#ifndef NET_UTIL_H
#define NET_UTIL_H

#include <cstdint>
#include <string>
#include <ctime>

namespace net {

/**
 * @brief net 模块通用运行时工具（从 Sylar util 抽取，无协程/YAML 依赖）。
 */

// 获取当前内核线程 ID（Linux gettid）
uint32_t GetThreadId();

// 获取当前协程 ID（未集成协程时固定为 0）
uint32_t GetFiberId();

// 获取当前线程名
std::string GetThreadName();

// 进程启动至今的累计毫秒数
uint32_t GetElapseMs();

// 当前时间戳（毫秒 / 微秒，Unix 纪元）
uint64_t GetCurrentMS();
uint64_t GetCurrentUS();

// 将 time_t 按 strftime 格式转为字符串
std::string Time2Str(time_t ts, const std::string& format = "%Y-%m-%d %H:%M:%S");

}  // namespace net

#endif  // NET_UTIL_H
