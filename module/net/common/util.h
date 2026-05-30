#ifndef NET_COMMON_UTIL_H
#define NET_COMMON_UTIL_H

#include <cstdint>
#include <ctime>
#include <string>

namespace net {

/** net 模块通用运行时工具（线程 ID、时间戳等）。 */

uint32_t GetThreadId();
uint32_t GetFiberId();
std::string GetThreadName();
uint32_t GetElapseMs();
uint64_t GetCurrentMS();
uint64_t GetCurrentUS();
std::string Time2Str(time_t ts, const std::string& format = "%Y-%m-%d %H:%M:%S");

}  // namespace net

#endif  // NET_COMMON_UTIL_H
