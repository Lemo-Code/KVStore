/**
 * @file util.cc
 * @brief net 模块通用工具函数实现。
 */
#include "util.h"

#include <chrono>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

namespace net {

namespace {

const std::chrono::steady_clock::time_point kStartTime =
    std::chrono::steady_clock::now();

}  // namespace

uint32_t GetThreadId() {
  return static_cast<uint32_t>(syscall(SYS_gettid));
}

uint32_t GetFiberId() {
  return 0;
}

std::string GetThreadName() {
  char name[16] = {0};
  if (pthread_getname_np(pthread_self(), name, sizeof(name)) == 0) {
    return std::string(name);
  }
  return "unknown";
}

uint32_t GetElapseMs() {
  const auto now = std::chrono::steady_clock::now();
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now - kStartTime)
          .count());
}

uint64_t GetCurrentMS() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<uint64_t>(tv.tv_sec) * 1000ull +
         static_cast<uint64_t>(tv.tv_usec) / 1000ull;
}

uint64_t GetCurrentUS() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<uint64_t>(tv.tv_sec) * 1000000ull +
         static_cast<uint64_t>(tv.tv_usec);
}

std::string Time2Str(time_t ts, const std::string& format) {
  struct tm tm_val;
  localtime_r(&ts, &tm_val);
  char buf[64] = {0};
  strftime(buf, sizeof(buf), format.c_str(), &tm_val);
  return std::string(buf);
}

}  // namespace net
