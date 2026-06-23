// zero Log — async logging framework with hierarchical loggers
//
// Provides a high-performance logging system with:
//   - 6 log levels: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
//   - Hierarchical loggers (e.g., "app.module.component")
//   - Pluggable appenders (console, file, syslog, custom)
//   - Pattern-based format strings with placeholders:
//       %t - timestamp, %T - thread id, %F - fiber id,
//       %N - logger name, %L - level, %f - file, %l - line, %m - message
//   - Async logging with lock-free ring buffer (optional)
//   - Log rotation (by size or time) for file appender
//   - Thread-safe and fiber-safe
//
// Usage:
//   auto* log = Logger::get("myapp.server");
//   ZERO_LOG_INFO(log, "Server started on port " << 8080);
//   ZERO_LOG_DEBUG(log, "Accepted connection from " << addr);
//   ZERO_LOG_ERROR(log, "Failed to read: " << errno);
#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <sstream>
#include <functional>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <ctime>
#include <cstdint>

namespace zero {

// ============================================================
// Log levels
// ============================================================

// Log severity levels, ordered by increasing importance.
// Compatible with syslog severity levels for integration.
enum class LogLevel : int {
    TRACE = 0,   // Detailed tracing, not for production
    DEBUG = 10,  // Debugging information
    INFO  = 20,  // General informational messages
    WARN  = 30,  // Warning conditions
    ERROR = 40,  // Error conditions
    FATAL = 50,  // Fatal errors (program will abort)
};

// Convert log level to human-readable string
inline const char* log_level_name(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:              return "?????";
    }
}

// ============================================================
// LogEvent — all data for a single log entry
// ============================================================

struct LogEvent {
    const char* file = nullptr;        // Source filename (__FILE__)
    int32_t line = 0;                  // Source line number
    int64_t timestamp_us = 0;          // Unix timestamp in microseconds
    uint64_t thread_id = 0;            // OS thread ID (pthread_self)
    uint64_t fiber_id = 0;             // Fiber ID (0 if not in fiber)
    LogLevel level = LogLevel::INFO;   // Severity level
    std::string logger_name;           // Hierarchical logger name
    std::string message;               // The log message text

    // Whether this message passes the level filter
    bool should_log(LogLevel min_level) const noexcept {
        return static_cast<int>(level) >= static_cast<int>(min_level);
    }

    // Format a timestamp string
    std::string format_time(const char* fmt = "%Y-%m-%d %H:%M:%S") const;
};

// ============================================================
// LogFormatter — formats LogEvent to string
// ============================================================

class LogFormatter {
public:
    // Create a formatter with a pattern string.
    // Placeholders:
    //   %%  - literal %
    //   %t  - timestamp
    //   %T  - thread ID
    //   %F  - fiber ID
    //   %N  - logger name
    //   %L  - log level
    //   %f  - source file
    //   %l  - source line
    //   %m  - message body
    //   %n  - newline
    //
    // Default pattern: "%t [%L] [%N] (%T:%F) %f:%l - %m%n"
    explicit LogFormatter(
        const std::string& pattern = "%t [%L] [%N] %f:%l - %m%n");

    // Format a LogEvent into a string
    std::string format(const LogEvent& event) const;

    // Change the pattern at runtime
    void set_pattern(const std::string& pattern);

    const std::string& pattern() const noexcept { return pattern_; }

private:
    std::string pattern_;
    // Pre-parsed format items for faster formatting
    struct FormatItem;
    std::vector<std::unique_ptr<FormatItem>> items_;
    void parse_pattern();
};

// ============================================================
// LogAppender — pluggable output destination
// ============================================================

class LogAppender {
public:
    virtual ~LogAppender() = default;

    // Write a log event to this appender
    virtual void log(const LogEvent& event) = 0;

    // Flush buffered output
    virtual void flush() = 0;

    // Set the formatter for this appender
    virtual void set_formatter(std::shared_ptr<LogFormatter> fmt) = 0;

    // Get the current formatter
    virtual std::shared_ptr<LogFormatter> get_formatter() const = 0;
};

// ============================================================
// ConsoleAppender — writes to stdout/stderr
// ============================================================

class ConsoleAppender : public LogAppender {
public:
    // Create with optional use of stderr for WARN and above
    explicit ConsoleAppender(bool use_stderr = true);

    void log(const LogEvent& event) override;
    void flush() override;
    void set_formatter(std::shared_ptr<LogFormatter> fmt) override;
    std::shared_ptr<LogFormatter> get_formatter() const override;

private:
    bool use_stderr_;
    std::shared_ptr<LogFormatter> formatter_;
    std::mutex mutex_;  // Protect against concurrent writes
};

// ============================================================
// FileAppender — writes to a file with rotation
// ============================================================

class FileAppender : public LogAppender {
public:
    // Open a file for logging. Creates the file if it doesn't exist.
    explicit FileAppender(const std::string& filename);
    ~FileAppender();

    void log(const LogEvent& event) override;
    void flush() override;
    void set_formatter(std::shared_ptr<LogFormatter> fmt) override;
    std::shared_ptr<LogFormatter> get_formatter() const override;

    // Enable log rotation by file size (bytes). When the file exceeds
    // this size, it is renamed to filename.N and a new file is opened.
    void enable_rotation(size_t max_size_bytes);

    // Enable time-based rotation (daily at midnight).
    void enable_daily_rotation();

    // Set maximum number of rotated log files to keep.
    void set_max_files(size_t max) noexcept { max_files_ = max; }

    // Reopen the log file (after external rotation, e.g., logrotate)
    bool reopen();

private:
    void rotate_if_needed();

    std::string filename_;
    int fd_ = -1;
    std::shared_ptr<LogFormatter> formatter_;
    std::mutex mutex_;

    // Rotation settings
    size_t max_size_ = 0;          // 0 = disabled
    size_t current_size_ = 0;
    bool daily_rotation_ = false;
    int last_rotation_day_ = -1;
    size_t max_files_ = 10;
};

// ============================================================
// SyslogAppender — writes to syslog
// ============================================================

class SyslogAppender : public LogAppender {
public:
    // Create a syslog appender with given ident string.
    explicit SyslogAppender(const std::string& ident);
    ~SyslogAppender();

    void log(const LogEvent& event) override;
    void flush() override;
    void set_formatter(std::shared_ptr<LogFormatter> fmt) override;
    std::shared_ptr<LogFormatter> get_formatter() const override;

private:
    std::string ident_;
    std::shared_ptr<LogFormatter> formatter_;
};

// ============================================================
// AsyncLogAppender — wraps another appender with async dispatch
// ============================================================

class AsyncLogAppender : public LogAppender {
public:
    // Wrap an existing appender with an async dispatch queue.
    // Log events are appended to a lock-free ring buffer and written
    // by a background thread.
    explicit AsyncLogAppender(std::shared_ptr<LogAppender> appender,
                                size_t ring_buffer_size = 65536);
    ~AsyncLogAppender();

    void log(const LogEvent& event) override;
    void flush() override;
    void set_formatter(std::shared_ptr<LogFormatter> fmt) override;
    std::shared_ptr<LogFormatter> get_formatter() const override;

    // Stop the background thread and drain remaining events
    void stop();

    // Number of events dropped due to full buffer
    size_t dropped() const noexcept { return dropped_; }

private:
    void worker_loop();

    std::shared_ptr<LogAppender> appender_;
    std::atomic<bool> running_{true};
    std::atomic<size_t> dropped_{0};
    std::thread worker_thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<LogEvent> buffer_;
    std::vector<LogEvent> swap_buffer_;
    size_t max_size_;
};

// ============================================================
// Logger — hierarchical named logger
// ============================================================

class Logger {
public:
    // Get or create a logger with the given hierarchical name.
    // Name can be dotted: "app.module.submodule"
    static Logger* get(const std::string& name);

    // Get the root logger ("")
    static Logger* root();

    // Get all registered loggers
    static const std::vector<Logger*>& all_loggers();

    // Log a message at the given level
    void log(LogLevel level, const char* file, int line,
              const std::string& msg);

    // Level filtering
    void set_level(LogLevel level) noexcept { level_ = level; }
    LogLevel level() const noexcept { return level_; }

    // The logger's name
    const std::string& name() const noexcept { return name_; }

    // Add an appender to this logger
    void add_appender(std::shared_ptr<LogAppender> appender);

    // Remove all appenders
    void clear_appenders();

    // Whether this logger inherits appenders from parent loggers
    void set_inherit_appenders(bool inherit) noexcept {
        inherit_appenders_ = inherit;
    }

    // Convenience methods
    bool is_trace() const noexcept { return level_ <= LogLevel::TRACE; }
    bool is_debug() const noexcept { return level_ <= LogLevel::DEBUG; }
    void trace(const char* file, int line, const std::string& msg) {
        log(LogLevel::TRACE, file, line, msg);
    }
    void debug(const char* file, int line, const std::string& msg) {
        log(LogLevel::DEBUG, file, line, msg);
    }
    void info(const char* file, int line, const std::string& msg) {
        log(LogLevel::INFO, file, line, msg);
    }
    void warn(const char* file, int line, const std::string& msg) {
        log(LogLevel::WARN, file, line, msg);
    }
    void error(const char* file, int line, const std::string& msg) {
        log(LogLevel::ERROR, file, line, msg);
    }
    void fatal(const char* file, int line, const std::string& msg) {
        log(LogLevel::FATAL, file, line, msg);
    }

    // Collect appenders from this logger and all parent loggers
    void collect_appenders(
        std::vector<std::shared_ptr<LogAppender>>& out) const;

    explicit Logger(const std::string& name);
    ~Logger();

private:
    std::string name_;
    LogLevel level_ = LogLevel::DEBUG;
    std::vector<std::shared_ptr<LogAppender>> appenders_;
    bool inherit_appenders_ = true;
    std::mutex mutex_;

    // Parent logger (e.g., "app.module" for "app.module.sub")
    Logger* parent_ = nullptr;
};

} // namespace zero

// ============================================================
// Streaming log macros
// ============================================================
// Defined outside namespace for safe expansion in user code.
// The macros use an ostringstream for flexible message formatting.

#define ZERO_LOG(logger, level, stream_expr)                                  \
    do {                                                                       \
        auto* _zlog = (logger);                                               \
        if (_zlog &&                                                          \
            static_cast<int>(level) >= static_cast<int>(_zlog->level())) {    \
            std::ostringstream _zoss;                                          \
            _zoss << stream_expr;                                              \
            _zlog->log(level, __FILE__, __LINE__, _zoss.str());               \
        }                                                                      \
    } while (0)

#define ZERO_LOG_TRACE(logger, msg)                                            \
    ZERO_LOG(logger, ::zero::LogLevel::TRACE, msg)
#define ZERO_LOG_DEBUG(logger, msg)                                            \
    ZERO_LOG(logger, ::zero::LogLevel::DEBUG, msg)
#define ZERO_LOG_INFO(logger, msg)                                             \
    ZERO_LOG(logger, ::zero::LogLevel::INFO, msg)
#define ZERO_LOG_WARN(logger, msg)                                             \
    ZERO_LOG(logger, ::zero::LogLevel::WARN, msg)
#define ZERO_LOG_ERROR(logger, msg)                                            \
    ZERO_LOG(logger, ::zero::LogLevel::ERROR, msg)
#define ZERO_LOG_FATAL(logger, msg)                                            \
    ZERO_LOG(logger, ::zero::LogLevel::FATAL, msg)

// Convenience macros that use the root logger
#define ZERO_ROOT_LOG_TRACE(msg) ZERO_LOG_TRACE(::zero::Logger::root(), msg)
#define ZERO_ROOT_LOG_DEBUG(msg) ZERO_LOG_DEBUG(::zero::Logger::root(), msg)
#define ZERO_ROOT_LOG_INFO(msg)  ZERO_LOG_INFO(::zero::Logger::root(), msg)
#define ZERO_ROOT_LOG_WARN(msg)  ZERO_LOG_WARN(::zero::Logger::root(), msg)
#define ZERO_ROOT_LOG_ERROR(msg) ZERO_LOG_ERROR(::zero::Logger::root(), msg)
#define ZERO_ROOT_LOG_FATAL(msg) ZERO_LOG_FATAL(::zero::Logger::root(), msg)
