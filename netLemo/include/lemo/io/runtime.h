#pragma once

#include "lemo/io/iomanager.h"

#include <memory>
#include <string>

namespace lemo {
namespace io {

/**
 * @brief 运行时门面：Scheduler + Reactor + Hook，替代直接使用 IOManager。
 */
class Runtime {
 public:
  typedef std::shared_ptr<Runtime> ptr;

  Runtime(size_t threads = 1, bool use_caller = true,
          const std::string& name = "");
  ~Runtime();

  void stop();

  fiber::Scheduler& scheduler() { return *iom_; }
  IOManager& iom() { return *iom_; }
  Reactor& reactor() { return iom_->reactor(); }

  static Runtime* GetThis();

 private:
  IOManager::ptr iom_;
};

}  // namespace io
}  // namespace lemo
