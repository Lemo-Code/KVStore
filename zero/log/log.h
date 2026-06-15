#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <sstream>
#include <fstream>
#include <functional>
#include <ctime>

#include "zero/base/singleton.h"
#include "zero/thread/mutex.h"
#include "zero/base/macro.h"

namespace zero {

// ============ LogLevel ============
class LogLevel {
public:
    enum Level { UNKNOWN = 0, TRACE = 1, DEBUG = 2, INFO = 3, WARN = 4, ERROR = 5, FATAL = 6, OFF = 99 };
    static const char* ToString(Level level);
    static Level FromString(const std::string& str);
};

// ============ LogEvent ============
class Logger;  // fwd

class LogEvent {
public:
    using ptr = std::shared_ptr<LogEvent>;

    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
             const char* file, int32_t line, uint32_t thread_id,
             uint64_t fiber_id, uint64_t time, const std::string& thread_name);

    const char*    getFile()       const { return file_; }
    int32_t        getLine()       const { return line_; }
    uint32_t       getThreadId()   const { return thread_id_; }
    uint64_t       getFiberId()    const { return fiber_id_; }
    uint64_t       getTime()       const { return time_; }
    LogLevel::Level getLevel()     const { return level_; }
    std::string    getThreadName() const { return thread_name_; }
    std::stringstream& getSS()          { return ss_; }
    std::shared_ptr<Logger> getLogger() const { return logger_; }

    void format(const char* fmt, ...);
    void format(const char* fmt, va_list al);

private:
    const char* file_ = nullptr;
    int32_t line_ = 0;
    uint32_t thread_id_ = 0;
    uint64_t fiber_id_ = 0;
    uint64_t time_ = 0;
    std::string thread_name_;
    std::stringstream ss_;
    std::shared_ptr<Logger> logger_;
    LogLevel::Level level_ = LogLevel::UNKNOWN;
};

// ============ LogFormatter ============
class LogFormatter {
public:
    using ptr = std::shared_ptr<LogFormatter>;

    explicit LogFormatter(const std::string& pattern = "%d{%Y-%m-%d %H:%M:%S} [%p] [%t] [%F:%l] %m%n");
    void init();
    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
    const std::string& getPattern() const { return pattern_; }
    bool isError() const { return error_; }

private:
    class FormatItem {
    public:
        using ptr = std::shared_ptr<FormatItem>;
        virtual ~FormatItem() = default;
        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger,
                           LogLevel::Level level, LogEvent::ptr event) = 0;
    };

    std::string pattern_;
    std::vector<FormatItem::ptr> items_;
    bool error_ = false;
};

// ============ MDC (Mapped Diagnostic Context) ============
// Fiber-local key-value store for structured logging.
// Usage: MDC::put("request_id", "abc123");  // appears as %X{request_id}
class MDC {
public:
    static void put(const std::string& key, const std::string& val);
    static std::string get(const std::string& key);
    static void remove(const std::string& key);
    static void clear();
    static const std::map<std::string, std::string>& getAll();
};

// ============ RateLimiter (Token Bucket) ============
class RateLimiter {
public:
    RateLimiter(double rate_per_sec = 100.0, int burst = 10)
        : rate_(rate_per_sec), burst_(burst), tokens_(burst),
          last_update_(std::chrono::steady_clock::now()) {}
    bool allow();

private:
    double rate_;
    int burst_;
    double tokens_;
    std::chrono::steady_clock::time_point last_update_;
    Mutex mutex_;
};

// ============ LogAppender ============
class LogAppender {
    friend class Logger;
public:
    using ptr = std::shared_ptr<LogAppender>;
    virtual ~LogAppender() = default;

    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    virtual void flush() {}

    void setFormatter(LogFormatter::ptr fmt);
    LogFormatter::ptr getFormatter() const;
    LogLevel::Level getLevel() const { return level_; }
    void setLevel(LogLevel::Level l) { level_ = l; }

protected:
    LogLevel::Level level_ = LogLevel::DEBUG;
    LogFormatter::ptr formatter_;
    bool has_formatter_ = false;
    Mutex mutex_;
};

// Console with ANSI color support
class ConsoleLogAppender : public LogAppender {
public:
    using ptr = std::shared_ptr<ConsoleLogAppender>;
    explicit ConsoleLogAppender(bool use_color = true) : use_color_(use_color) {}
    void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
    static const char* levelColor(LogLevel::Level level);
private:
    bool use_color_;
};

// Stdout (backward compat)
using StdoutLogAppender = ConsoleLogAppender;

// File with rolling support
class FileLogAppender : public LogAppender {
public:
    using ptr = std::shared_ptr<FileLogAppender>;
    explicit FileLogAppender(const std::string& filename,
                             uint64_t max_size = 0,       // 0=no rolling
                             uint32_t max_files = 10);
    void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
    void flush() override;
    bool reopen();
private:
    void rotate();
    std::string filename_;
    std::ofstream stream_;
    uint64_t max_size_;
    uint32_t max_files_;
    uint64_t current_size_ = 0;
};

// ============ Logger ============
class Logger : public std::enable_shared_from_this<Logger> {
public:
    using ptr = std::shared_ptr<Logger>;
    using MutexType = Mutex;

    explicit Logger(const std::string& name = "root");

    void log(LogLevel::Level level, LogEvent::ptr event);
    void trace(LogEvent::ptr e) { log(LogLevel::TRACE, e); }
    void debug(LogEvent::ptr e) { log(LogLevel::DEBUG, e); }
    void info(LogEvent::ptr e)  { log(LogLevel::INFO, e); }
    void warn(LogEvent::ptr e)  { log(LogLevel::WARN, e); }
    void error(LogEvent::ptr e) { log(LogLevel::ERROR, e); }
    void fatal(LogEvent::ptr e) { log(LogLevel::FATAL, e); }

    // Hierarchical logger support
    void setParent(Logger::ptr parent) { parent_ = parent; }
    Logger::ptr getParent() const { return parent_; }

    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    void clearAppenders();

    // Get all effective appenders (own + inherited from parent chain)
    void forEachAppender(std::function<void(LogAppender::ptr)> cb);

    void setFormatter(LogFormatter::ptr fmt);
    void setFormatter(const std::string& pattern);
    LogFormatter::ptr getFormatter() const;

    LogLevel::Level getLevel() const { return level_; }
    void setLevel(LogLevel::Level l) { level_ = l; }
    const std::string& getName() const { return name_; }

    // Rate limiting
    void setRateLimiter(std::shared_ptr<RateLimiter> limiter) { limiter_ = limiter; }

    void flush();

private:
    std::string name_;
    LogLevel::Level level_ = LogLevel::DEBUG;
    bool level_set_ = false; ///< true if user explicitly set level (vs inherited)
    LogFormatter::ptr formatter_;
    std::list<LogAppender::ptr> appenders_;
    Logger::ptr parent_;
    std::shared_ptr<RateLimiter> limiter_;
    MutexType mutex_;
};

// ============ LogEventWrap (RAII 自动输出) ============
class LogEventWrap {
public:
    LogEventWrap(Logger::ptr logger, LogLevel::Level level,
                 const char* file, int32_t line);
    ~LogEventWrap();
    std::stringstream& getSS() { return event_->getSS(); }
    LogEvent::ptr getEvent() const { return event_; }
private:
    LogEvent::ptr event_;
    Logger::ptr logger_;
};

// ============ LoggerManager ============
class LoggerManager {
public:
    LoggerManager();
    /// Get or create a logger. Hierarchical: "a.b.c" auto-creates parents "a" and "a.b".
    Logger::ptr getLogger(const std::string& name);
    Logger::ptr getRoot() const { return root_; }
    void setRootLevel(LogLevel::Level l) { root_->setLevel(l); }
private:
    Logger::ptr getOrCreateParent(const std::string& name);
    std::map<std::string, Logger::ptr> loggers_;
    Logger::ptr root_;
    Mutex mutex_;
};

using LoggerMgr = Singleton<LoggerManager>;

// ============ 日志宏 ============
#define ZERO_LOG_LEVEL(logger, level) \
    if (logger->getLevel() <= level) \
        zero::LogEventWrap(logger, level, __FILE__, __LINE__).getSS()

#define ZERO_LOG_TRACE(logger) ZERO_LOG_LEVEL(logger, zero::LogLevel::TRACE)
#define ZERO_LOG_DEBUG(logger) ZERO_LOG_LEVEL(logger, zero::LogLevel::DEBUG)
#define ZERO_LOG_INFO(logger)  ZERO_LOG_LEVEL(logger, zero::LogLevel::INFO)
#define ZERO_LOG_WARN(logger)  ZERO_LOG_LEVEL(logger, zero::LogLevel::WARN)
#define ZERO_LOG_ERROR(logger) ZERO_LOG_LEVEL(logger, zero::LogLevel::ERROR)
#define ZERO_LOG_FATAL(logger) ZERO_LOG_LEVEL(logger, zero::LogLevel::FATAL)

#define ZERO_LOG_ROOT()  zero::LoggerMgr::GetInstance()->getRoot()
#define ZERO_LOG_NAME(n) zero::LoggerMgr::GetInstance()->getLogger(n)

} // namespace zero
