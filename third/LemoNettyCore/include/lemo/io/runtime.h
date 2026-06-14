#pragma once

#include "lemo/io/iomanager.h"
#include "lemo/nettycore_export.h"

#include <memory>
#include <string>

namespace lemo {
namespace io {

/**
 * @brief 运行时门面：Scheduler + Reactor + Hook。
 *
 * 应用层推荐持有 Runtime，而非直接持有 IOManager。
 */
class LEMO_NETTYCORE_API Runtime {
 public:
  typedef std::shared_ptr<Runtime> ptr;

  Runtime(size_t threads = 1, bool use_caller = true,
          const std::string& name = "");
  ~Runtime();

  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  void stop();

  fiber::Scheduler& scheduler() { return *iom_; }
  const fiber::Scheduler& scheduler() const { return *iom_; }
  IOManager& iom() { return *iom_; }
  const IOManager& iom() const { return *iom_; }
  Reactor& reactor() { return iom_->reactor(); }
  const Reactor& reactor() const { return iom_->reactor(); }

  static Runtime* GetThis();

 private:
  IOManager::ptr iom_;
};

}  // namespace io
}  // namespace lemo
