#include "lemo/log/appender.h"

namespace lemo {
namespace log {

Appender::Appender()
    : has_layout_(false), threshold_(LogLevel::DEBUG) {}

void Appender::SetLayout(Layout::ptr layout) {
  std::lock_guard<std::mutex> lock(mutex_);
  layout_ = layout;
  has_layout_ = static_cast<bool>(layout_);
}

Layout::ptr Appender::GetLayout() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return layout_;
}

bool Appender::HasLayout() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return has_layout_;
}

void Appender::SetThreshold(LogLevel::Level level) { threshold_ = level; }

LogLevel::Level Appender::GetThreshold() const { return threshold_; }

bool Appender::PassesThreshold(LogLevel::Level level) const {
  return level >= threshold_;
}

std::string Appender::Format(const LogRecord& record) const {
  Layout::ptr layout;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    layout = layout_;
  }
  if (layout) {
    return layout->Format(record);
  }
  return record.message;
}

}  // namespace log
}  // namespace lemo
