#pragma once

#include "lemo/log/layout.h"

#include <memory>
#include <string>

namespace lemo {
namespace log {

class PatternLayout : public Layout {
 public:
  explicit PatternLayout(const std::string& pattern);
  ~PatternLayout();
  std::string Format(const LogRecord& record) const override;
  const std::string& GetPattern() const;
  bool IsError() const;

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
  void InitPattern();
};

typedef PatternLayout LogFormatter;

}  // namespace log
}  // namespace lemo
