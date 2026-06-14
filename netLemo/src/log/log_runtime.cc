#include "lemo/log/log_runtime.h"

namespace lemo {
namespace log {
namespace {

std::string g_async_worker_name("lemo-log-async");

}  // namespace

const std::string& LogRuntime::AsyncWorkerName() { return g_async_worker_name; }

void LogRuntime::SetAsyncWorkerName(const std::string& name) {
  g_async_worker_name = name.empty() ? "lemo-log-async" : name;
}

}  // namespace log
}  // namespace lemo
