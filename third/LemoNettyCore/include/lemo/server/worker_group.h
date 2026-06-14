#pragma once

#include "lemo/io/runtime.h"
#include "lemo/nettycore_export.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace lemo {
namespace server {

/**
 * @brief 多 Runtime 工作组，按 round-robin 分发新连接。
 */
class LEMO_NETTYCORE_API WorkerGroup {
 public:
  explicit WorkerGroup(size_t workers, bool use_caller = false,
                       const std::string& name = "worker");
  ~WorkerGroup();

  WorkerGroup(const WorkerGroup&) = delete;
  WorkerGroup& operator=(const WorkerGroup&) = delete;

  void stop();
  io::Runtime& pick();
  io::Runtime& worker(size_t index);
  const io::Runtime& worker(size_t index) const;
  size_t size() const { return workers_.size(); }

 private:
  std::vector<io::Runtime::ptr> workers_;
  std::atomic<size_t> round_robin_{0};
};

}  // namespace server
}  // namespace lemo
