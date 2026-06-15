#include "zero/log/log.h"
#include "zero/thread/thread.h"
#include "zero/fiber/fiber.h"
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <iomanip>
#include <iostream>
#include <chrono>

namespace zero {

// ====================================================================
// LogLevel
// ====================================================================
const char* LogLevel::ToString(Level level) {
    switch (level) {
        case DEBUG: return "DEBUG";
        case INFO:  return "INFO";
        case WARN:  return "WARN";
        case ERROR: return "ERROR";
        case FATAL: return "FATAL";
        default:    return "UNKNOWN";
    }
}

LogLevel::Level LogLevel::FromString(const std::string& str) {
    if (str == "DEBUG") return DEBUG;
    if (str == "INFO")  return INFO;
    if (str == "WARN")  return WARN;
    if (str == "ERROR") return ERROR;
    if (str == "FATAL") return FATAL;
    return UNKNOWN;
}

// ====================================================================
// LogEvent
// ====================================================================
LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
                   const char* file, int32_t line, uint32_t thread_id,
                   uint64_t fiber_id, uint64_t time, const std::string& thread_name)
    : file_(file), line_(line), thread_id_(thread_id), fiber_id_(fiber_id),
      time_(time), thread_name_(thread_name), logger_(std::move(logger)), level_(level) {}

void LogEvent::format(const char* fmt, ...) {
    va_list al;
    va_start(al, fmt);
    format(fmt, al);
    va_end(al);
}

void LogEvent::format(const char* fmt, va_list al) {
    char* buf = nullptr;
    int len = vasprintf(&buf, fmt, al);
    if (len >= 0 && buf) {
        ss_ << buf;
        free(buf);
    }
}

// ====================================================================
// LogFormatter
// ====================================================================
LogFormatter::LogFormatter(const std::string& pattern) : pattern_(pattern) {
    init();
}

void LogFormatter::init() {
    // 支持 %d{...} %p %t %F %f %l %m %n
    items_.clear();

    struct SimpleItem : FormatItem {
        std::string fmt;
        SimpleItem(const std::string& f) : fmt(f) {}
        void format(std::ostream& os, std::shared_ptr<Logger>, LogLevel::Level level, LogEvent::ptr event) override {
            for (size_t i = 0; i < fmt.size(); ++i) {
                if (fmt[i] == '%' && i + 1 < fmt.size()) {
                    switch (fmt[++i]) {
                        case 'p': // level
                            os << LogLevel::ToString(level);
                            break;
                        case 't': { // thread id
                            os << event->getThreadId();
                            break;
                        }
                        case 'F': { // fiber id
                            os << event->getFiberId();
                            break;
                        }
                        case 'f': { // file
                            os << event->getFile();
                            break;
                        }
                        case 'l': { // line
                            os << event->getLine();
                            break;
                        }
                        case 'm': { // message
                            os << event->getSS().str();
                            break;
                        }
                        case 'n': // newline
                            os << "\n";
                            break;
                        case 'd': { // date/time
                            if (fmt[i+1] == '{') {
                                size_t end = fmt.find('}', i+1);
                                std::string df = end != std::string::npos
                                    ? fmt.substr(i+2, end - i - 2)
                                    : "%Y-%m-%d %H:%M:%S";
                                i = (end != std::string::npos) ? end : i;
                                time_t t = event->getTime();
                                struct tm tm_buf;
                                localtime_r(&t, &tm_buf);
                                char buf[128];
                                strftime(buf, sizeof(buf), df.c_str(), &tm_buf);
                                os << buf;
                            }
                            break;
                        }
                        case '%':
                            os << '%';
                            break;
                        default:
                            os << '%' << fmt[i];
                    }
                } else {
                    os << fmt[i];
                }
            }
        }
    };
    items_.push_back(std::make_shared<SimpleItem>(pattern_));
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger,
                                  LogLevel::Level level, LogEvent::ptr event) {
    std::stringstream ss;
    for (auto& item : items_) {
        item->format(ss, logger, level, event);
    }
    return ss.str();
}

// ====================================================================
// LogAppender
// ====================================================================
void LogAppender::setFormatter(LogFormatter::ptr fmt) {
    Mutex::Lock lock(mutex_);
    formatter_ = std::move(fmt);
    has_formatter_ = true;
}

LogFormatter::ptr LogAppender::getFormatter() const {
    return formatter_;
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger,
                             LogLevel::Level level, LogEvent::ptr event) {
    Mutex::Lock lock(mutex_);
    std::string output;
    if (formatter_) {
        output = formatter_->format(logger, level, event);
    } else {
        output = event->getSS().str() + "\n";
    }
    std::cout << output << std::flush;
}

FileLogAppender::FileLogAppender(const std::string& filename)
    : filename_(filename) {
    reopen();
}

void FileLogAppender::log(std::shared_ptr<Logger> logger,
                           LogLevel::Level level, LogEvent::ptr event) {
    Mutex::Lock lock(mutex_);
    std::string output;
    if (formatter_) {
        output = formatter_->format(logger, level, event);
    } else {
        output = event->getSS().str() + "\n";
    }
    if (stream_.is_open()) {
        stream_ << output << std::flush;
    }
}

void FileLogAppender::flush() {
    if (stream_.is_open()) stream_.flush();
}

bool FileLogAppender::reopen() {
    if (stream_.is_open()) stream_.close();
    stream_.open(filename_, std::ios::app);
    return stream_.is_open();
}

// ====================================================================
// Logger
// ====================================================================
Logger::Logger(const std::string& name) : name_(name) {
    formatter_.reset(new LogFormatter());
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
    if (level < level_) return;
    MutexType::Lock lock(mutex_);
    for (auto& ap : appenders_) {
        if (level >= ap->getLevel()) {
            ap->log(shared_from_this(), level, event);
        }
    }
}

void Logger::addAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(mutex_);
    if (!appender->has_formatter_) {
        appender->setFormatter(formatter_);
    }
    appenders_.push_back(std::move(appender));
}

void Logger::delAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(mutex_);
    appenders_.remove(appender);
}

void Logger::clearAppenders() {
    MutexType::Lock lock(mutex_);
    appenders_.clear();
}

void Logger::setFormatter(LogFormatter::ptr fmt) {
    MutexType::Lock lock(mutex_);
    formatter_ = std::move(fmt);
    for (auto& ap : appenders_) {
        if (!ap->has_formatter_) {
            ap->setFormatter(formatter_);
        }
    }
}

void Logger::setFormatter(const std::string& pattern) {
    setFormatter(std::make_shared<LogFormatter>(pattern));
}

LogFormatter::ptr Logger::getFormatter() const {
    return formatter_;
}

void Logger::flush() {
    MutexType::Lock lock(mutex_);
    for (auto& ap : appenders_) {
        ap->flush();
    }
}

// ====================================================================
// LogEventWrap
// ====================================================================
LogEventWrap::LogEventWrap(Logger::ptr logger, LogLevel::Level level,
                           const char* file, int32_t line)
    : logger_(std::move(logger)) {
    event_ = std::make_shared<LogEvent>(
        logger_, level, file, line,
        GetThreadId(), GetFiberId(), GetCurrentMS() / 1000,
        Thread::GetName());
}

LogEventWrap::~LogEventWrap() {
    logger_->log(event_->getLevel(), event_);
}

// ====================================================================
// LoggerManager
// ====================================================================
LoggerManager::LoggerManager() {
    root_ = std::make_shared<Logger>("root");
    root_->addAppender(std::make_shared<StdoutLogAppender>());
    loggers_["root"] = root_;
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
    Mutex::Lock lock(mutex_);
    auto it = loggers_.find(name);
    if (it != loggers_.end()) return it->second;

    auto logger = std::make_shared<Logger>(name);
    logger->setFormatter(root_->getFormatter());
    loggers_[name] = logger;
    return logger;
}

} // namespace zero
