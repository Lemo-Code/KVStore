#pragma once

#include "lemo/log/record.h"

#include <memory>
#include <string>

namespace lemo {
namespace log {

class Layout {
 public:
  typedef std::shared_ptr<Layout> ptr;
  virtual ~Layout() {}
  virtual std::string Format(const LogRecord& record) const = 0;
};

}  // namespace log
}  // namespace lemo
