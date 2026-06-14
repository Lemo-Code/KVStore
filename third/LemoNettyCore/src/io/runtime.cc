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
  if (iom == nullptr) {
    return nullptr;
  }
  (void)iom;
  return nullptr;
}

}  // namespace io
}  // namespace lemo
