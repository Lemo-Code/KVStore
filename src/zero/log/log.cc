#include "zero/log/log.h"
#include "zero/thread/thread.h"
#include "zero/fiber/fiber.h"
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <functional>

namespace zero {

// ====================================================================
// LogLevel
// ====================================================================
const char* LogLevel::ToString(Level level) {
    switch (level) {
        case TRACE: return "TRACE";
        case DEBUG: return "DEBUG";
        case INFO:  return "INFO";
        case WARN:  return "WARN";
        case ERROR: return "ERROR";
        case FATAL: return "FATAL";
        default:    return "UNKNOWN";
    }
}

LogLevel::Level LogLevel::FromString(const std::string& str) {
    if (str == "TRACE") return TRACE;
    if (str == "DEBUG") return DEBUG;
    if (str == "INFO")  return INFO;
    if (str == "WARN")  return WARN;
    if (str == "ERROR") return ERROR;
    if (str == "FATAL") return FATAL;
    if (str == "OFF")   return OFF;
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
                        case 'N': { // logger name
                            os << event->getLogger()->getName();
                            break;
                        }
                        case 'C': { // color start/end
                            os << ConsoleLogAppender::levelColor(level);
                            break;
                        }
                        case 'X': { // MDC value
                            if (fmt[i+1] == '{') {
                                size_t end = fmt.find('}', i+1);
                                std::string key = (end != std::string::npos)
                                    ? fmt.substr(i+2, end - i - 2) : "";
                                i = (end != std::string::npos) ? end : i;
                                os << MDC::get(key);
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

// ====================================================================
// MDC
// ====================================================================
static thread_local std::map<std::string, std::string> t_mdc;

void MDC::put(const std::string& key, const std::string& val) { t_mdc[key] = val; }
std::string MDC::get(const std::string& key) { auto it = t_mdc.find(key); return it != t_mdc.end() ? it->second : ""; }
void MDC::remove(const std::string& key) { t_mdc.erase(key); }
void MDC::clear() { t_mdc.clear(); }
const std::map<std::string, std::string>& MDC::getAll() { return t_mdc; }

// ====================================================================
// RateLimiter
// ====================================================================
bool RateLimiter::allow() {
    Mutex::Lock lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - last_update_).count();
    last_update_ = now;
    tokens_ += elapsed * rate_;
    if (tokens_ > burst_) tokens_ = burst_;
    if (tokens_ >= 1.0) { tokens_ -= 1.0; return true; }
    return false;
}

// ====================================================================
// ConsoleLogAppender (ANSI color)
// ====================================================================
const char* ConsoleLogAppender::levelColor(LogLevel::Level level) {
    switch (level) {
        case LogLevel::TRACE: return "\033[37m";    // white
        case LogLevel::DEBUG: return "\033[36m";    // cyan
        case LogLevel::INFO:  return "\033[32m";    // green
        case LogLevel::WARN:  return "\033[33m";    // yellow
        case LogLevel::ERROR: return "\033[31m";    // red
        case LogLevel::FATAL: return "\033[35m";    // magenta
        default:              return "\033[0m";
    }
}

void ConsoleLogAppender::log(std::shared_ptr<Logger> logger,
                              LogLevel::Level level, LogEvent::ptr event) {
    Mutex::Lock lock(mutex_);
    std::string output;
    if (formatter_) {
        output = formatter_->format(logger, level, event);
    } else {
        output = event->getSS().str() + "\n";
    }
    if (use_color_) {
        std::cout << levelColor(level) << output << "\033[0m" << std::flush;
    } else {
        std::cout << output << std::flush;
    }
}

// ====================================================================
// FileLogAppender (with rolling)
// ====================================================================
FileLogAppender::FileLogAppender(const std::string& filename,
                                   uint64_t max_size, uint32_t max_files)
    : filename_(filename), max_size_(max_size), max_files_(max_files) {
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
        stream_ << output;
        current_size_ += output.size();
        if (!output.empty() && output.back() != '\n') {
            stream_ << std::endl;
            current_size_++;
        } else {
            stream_ << std::flush;
        }
    }
    if (max_size_ > 0 && current_size_ >= max_size_) rotate();
}

void FileLogAppender::rotate() {
    if (stream_.is_open()) stream_.close();
    for (uint32_t i = max_files_; i > 0; --i) {
        std::string old_name = filename_ + "." + std::to_string(i - 1);
        std::string new_name = filename_ + "." + std::to_string(i);
        if (i == 1) old_name = filename_;
        if (i == max_files_) std::remove(new_name.c_str());
        std::rename(old_name.c_str(), new_name.c_str());
    }
    reopen();
}

void FileLogAppender::flush() {
    if (stream_.is_open()) stream_.flush();
}

bool FileLogAppender::reopen() {
    if (stream_.is_open()) stream_.close();
    stream_.open(filename_, std::ios::app);
    if (stream_.is_open()) {
        stream_.seekp(0, std::ios::end);
        current_size_ = stream_.tellp();
    }
    return stream_.is_open();
}

// ====================================================================
// Logger
// ====================================================================
Logger::Logger(const std::string& name) : name_(name) {
    formatter_.reset(new LogFormatter());
    level_set_ = (name == "root"); // root level is explicit
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
    // Check effective level (this logger or inherited from parent chain)
    Logger::ptr cur = shared_from_this();
    LogLevel::Level effective_level = level_;
    while (!level_set_ && cur->parent_) {
        cur = cur->parent_;
        effective_level = cur->level_;
    }
    if (level < effective_level) return;

    // Rate limiting (if configured)
    if (limiter_ && !limiter_->allow()) return;

    // Log to own appenders
    {
        MutexType::Lock lock(mutex_);
        for (auto& ap : appenders_) {
            if (level >= ap->getLevel()) {
                ap->log(shared_from_this(), level, event);
            }
        }
    }
    // Also log to parent's appenders (inheritance)
    if (parent_) {
        parent_->log(level, event);
    }
}

void Logger::forEachAppender(std::function<void(LogAppender::ptr)> cb) {
    MutexType::Lock lock(mutex_);
    for (auto& ap : appenders_) cb(ap);
    if (parent_) parent_->forEachAppender(cb);
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
    root_->addAppender(std::make_shared<ConsoleLogAppender>(true));
    loggers_["root"] = root_;
}

Logger::ptr LoggerManager::getOrCreateParent(const std::string& name) {
    // "a.b.c" → parent is "a.b"
    size_t pos = name.rfind('.');
    if (pos == std::string::npos) return root_;

    std::string parent_name = name.substr(0, pos);
    auto it = loggers_.find(parent_name);
    if (it != loggers_.end()) return it->second;

    // Create parent recursively
    auto parent = std::make_shared<Logger>(parent_name);
    parent->setParent(getOrCreateParent(parent_name));
    loggers_[parent_name] = parent;
    return parent;
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
    if (name == "root" || name.empty()) return root_;

    Mutex::Lock lock(mutex_);
    auto it = loggers_.find(name);
    if (it != loggers_.end()) return it->second;

    auto logger = std::make_shared<Logger>(name);
    logger->setParent(getOrCreateParent(name));
    logger->setFormatter(root_->getFormatter());
    loggers_[name] = logger;
    return logger;
}

} // namespace zero
