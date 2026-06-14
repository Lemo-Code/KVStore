#include "lemo/io/runtime.h"

namespace lemo {
namespace io {

Runtime::Runtime(size_t threads, bool use_caller, const std::string& name)
    : iom_(new IOManager(threads, use_caller, name)) {}

Runtime::~Runtime() { stop(); }

void Runtime::stop() {
  if (iom_) {
    iom_->stop();
  }
}

Runtime* Runtime::GetThis() {
  IOManager* iom = IOManager::GetThis();
  if (!iom) return nullptr;
  // Runtime 不强制 TLS；GetThis 返回 nullptr，业务直接用 IOManager::GetThis()
  return nullptr;
}

}  // namespace io
}  // namespace lemo
