#pragma once

#include <string>

namespace lemo {
namespace log {

// 日志运行时参数（由 config 模块在加载后写入，log 模块读取）
class LogRuntime {
 public:
  static const std::string& AsyncWorkerName();
  static void SetAsyncWorkerName(const std::string& name);

 private:
  LogRuntime() = delete;
};

}  // namespace log
}  // namespace lemo
