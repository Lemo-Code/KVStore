#include "lemo/log/logger.h"
#include "lemo/log/pattern_layout.h"

#include <iostream>
#include <vector>

namespace lemo {
namespace log {

static const char* kDefaultPattern =
    "%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n";

Logger::Logger(const std::string& name)
    : name_(name),
      level_(LogLevel::DEBUG),
      level_set_(false),
      additive_(true),
      parent_(),
      root_() {
  layout_.reset(new PatternLayout(kDefaultPattern));
}

LogLevel::Level Logger::GetEffectiveLevel() const {
  const Logger* cur = this;
  while (cur) {
    if (cur->level_set_) return cur->level_;
    cur = cur->parent_.get();
  }
  return LogLevel::DEBUG;
}

void Logger::SetLevel(LogLevel::Level level) {
  level_ = level;
  level_set_ = true;
}

void Logger::InheritLayoutTo(Appender::ptr appender) {
  if (appender && !appender->HasLayout()) {
    appender->SetLayout(layout_);
  }
}

void Logger::AddAppender(Appender::ptr appender) {
  std::lock_guard<std::mutex> lock(mutex_);
  InheritLayoutTo(appender);
  appenders_.push_back(appender);
}

void Logger::ClearAppenders() {
  std::lock_guard<std::mutex> lock(mutex_);
  appenders_.clear();
}

void Logger::Flush() {
  std::vector<Appender::ptr> appenders;
  appenders.reserve(appenders_.size());
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::list<Appender::ptr>::const_iterator it = appenders_.begin();
         it != appenders_.end(); ++it) {
      if (*it) appenders.push_back(*it);
    }
  }
  for (size_t i = 0; i < appenders.size(); ++i) {
    appenders[i]->Flush();
  }
}

void Logger::SetLayout(Layout::ptr layout) {
  std::lock_guard<std::mutex> lock(mutex_);
  layout_ = layout;
  for (std::list<Appender::ptr>::iterator it = appenders_.begin();
       it != appenders_.end(); ++it) {
    if (*it && !(*it)->HasLayout()) {
      (*it)->SetLayout(layout_);
    }
  }
}

void Logger::SetLayout(const std::string& pattern) {
  Layout::ptr layout(new PatternLayout(pattern));
  PatternLayout* pl = dynamic_cast<PatternLayout*>(layout.get());
  if (pl && pl->IsError()) {
    std::cerr << "Logger setLayout name=" << name_ << " pattern=" << pattern
              << " invalid" << std::endl;
    return;
  }
  SetLayout(layout);
}

void Logger::Log(LogLevel::Level level, LogEvent::ptr event) {
  if (level < GetEffectiveLevel() || !event) return;
  const LogRecord record = event->ToRecord();
  AppendToSelf(record);
  if (additive_ && parent_) {
    parent_->AppendToAncestors(record);
  } else if (appenders_.empty() && root_ && this != root_.get()) {
    root_->Log(level, event);
  }
}

void Logger::AppendToSelf(const LogRecord& record) {
  std::vector<Appender::ptr> appenders;
  appenders.reserve(appenders_.size());
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::list<Appender::ptr>::const_iterator it = appenders_.begin();
         it != appenders_.end(); ++it) {
      if (*it) appenders.push_back(*it);
    }
  }
  for (size_t i = 0; i < appenders.size(); ++i) {
    appenders[i]->Append(record);
  }
}

void Logger::AppendToAncestors(const LogRecord& record) {
  AppendToSelf(record);
  if (additive_ && parent_) {
    parent_->AppendToAncestors(record);
  }
}

}  // namespace log
}  // namespace lemo
