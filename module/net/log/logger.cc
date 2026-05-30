/**
 * @file logger.cc
 * @brief Logger 日志器核心逻辑实现。
 */
#include "log/logger.h"

#include "log/config/log_config.h"

#include <iostream>

namespace net {

namespace detail {

void AttachRoot(const std::shared_ptr<Logger>& child,
                const std::shared_ptr<Logger>& root) {
  child->root_ = root;
}

}  // namespace detail

/** 初始化名称、级别、默认格式器；Appender 由 Manager 或用户添加 */
/** 初始化名称、级别、默认格式器；Appender 由 Manager 或用户添加 */
Logger::Logger(const std::string& name, bool async_mode)
    : async_mode_(async_mode),
      name_(name),
      level_(static_cast<LogLevel::Level>(LogConfig::instance().defaultLevel())),
      formatter_(new LogFormatter(LogConfig::instance().defaultPattern())),
      root_(nullptr) {}

/**
 * 添加输出地；若 Appender 未设格式器则继承 Logger 的格式器。
 * 避免 formatter 为 nullptr 导致输出时段错误。
 */
void Logger::addAppender(LogAppender::ptr appender) {
  MutexType::Lock lock(mutex_);
  if (!appender->getFormatter()) {
    LogAppender::MutexType::Lock app_lock(appender->mutex_);
    appender->formatter_ = formatter_;
  }
  appenders_.push_back(appender);
}

/** 按指针相等删除第一个匹配的 Appender */
void Logger::delAppender(LogAppender::ptr appender) {
  MutexType::Lock lock(mutex_);
  for (auto it = appenders_.begin(); it != appenders_.end(); ++it) {
    if (*it == appender) {
      appenders_.erase(it);
      break;
    }
  }
}

/** 移除全部 Appender，此后输出将回退到 root_ */
void Logger::clearAppenders() {
  MutexType::Lock lock(mutex_);
  appenders_.clear();
}

/** 判断指定 Appender 是否已在列表中 */
bool Logger::isAppenderExists(LogAppender::ptr appender) const {
  for (const auto& it : appenders_) {
    if (it == appender) {
      return true;
    }
  }
  return false;
}

/**
 * 设置 Logger 级格式器，并同步到已显式配置过格式器的 Appender。
 * 未显式配置的 Appender 在输出时仍动态继承 Logger 格式器。
 */
void Logger::setFormatter(LogFormatter::ptr formatter) {
  MutexType::Lock lock(mutex_);
  formatter_ = formatter;
  for (auto& app : appenders_) {
    LogAppender::MutexType::Lock app_lock(app->mutex_);
    if (app->hasFormatter()) {
      app->formatter_ = formatter_;
    }
  }
}

/** 以模式字符串设置格式器，解析失败时打印错误并保留原格式器 */
void Logger::setFormatter(const std::string& pattern) {
  LogFormatter::ptr fmt(new LogFormatter(pattern));
  if (fmt->hasError()) {
    std::cerr << "net log: invalid formatter for logger '" << name_
              << "': " << pattern << '\n';
    return;
  }
  setFormatter(fmt);
}

/** 返回当前格式模式字符串 */
std::string Logger::getFormatterStr() const {
  return formatter_->getPattern();
}

/** 返回格式器智能指针 */
LogFormatter::ptr Logger::getFormatter() const {
  return formatter_;
}

/**
 * 核心输出入口：级别过滤后分发给所有 Appender。
 * 若本 Logger 无 Appender，则委托 root_ 输出（Manager 注入的回退链）。
 */
void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
  if (level < level_) {
    return;
  }

  const auto self = shared_from_this();
  MutexType::Lock lock(mutex_);
  if (!appenders_.empty()) {
    for (auto& app : appenders_) {
      app->log(self, level, event, async_mode_);
    }
  } else if (root_) {
    root_->log(level, event);
  }
}

void Logger::debug(LogEvent::ptr event) { log(LogLevel::DEBUG, event); }
void Logger::info(LogEvent::ptr event) { log(LogLevel::INFO, event); }
void Logger::warn(LogEvent::ptr event) { log(LogLevel::WARN, event); }
void Logger::error(LogEvent::ptr event) { log(LogLevel::ERROR, event); }
void Logger::fatal(LogEvent::ptr event) { log(LogLevel::FATAL, event); }

}  // namespace net
