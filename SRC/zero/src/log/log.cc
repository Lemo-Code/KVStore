// zero log.cc — production async logging framework
//
// Implements:
//   LogFormatter   - pre-parsed pattern items for high-throughput formatting
//   ConsoleAppender - ANSI color output, stdout (<WARN) / stderr (>=WARN)
//   FileAppender   - O_CLOEXEC writes, EINTR-safe, size & daily rotation
//   SyslogAppender - syslog(3) integration with level mapping
//   AsyncLogAppender - double-buffered dispatch via background thread
//   Logger         - hierarchical named loggers with parent-chain inheritance
//   Global registry - thread-safe get/root/all_loggers via Mutex
//   FATAL handling  - flush all appenders then std::abort()
//
// Thread safety: ConsoleAppender and FileAppender use internal std::mutex.
// Logger::get() and Logger::root() use a global Mutex with double-checked locking.
// AsyncLogAppender uses mutex + condition_variable for the worker thread.

// condition_variable must be included before log.h (AsyncLogAppender uses it internally)
#include <condition_variable>

#include "zero/log/log.h"

#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <libgen.h>
#include <atomic>
#include <functional>

#include "zero/fiber/fiber.h"
#include "zero/thread/mutex.h"
#include "zero/base/macro.h"

namespace zero {

// ============================================================
// LogEvent::format_time
// ============================================================

std::string LogEvent::format_time(const char* fmt) const {
    time_t secs = static_cast<time_t>(timestamp_us / 1000000);
    int usec     = static_cast<int>(timestamp_us % 1000000);

    struct tm tm_buf;
    localtime_r(&secs, &tm_buf);

    char buf[128];
    size_t len = strftime(buf, sizeof(buf), fmt, &tm_buf);
    if (len == 0) return {};

    char us_buf[16];
    snprintf(us_buf, sizeof(us_buf), ".%06d", usec);
    return std::string(buf, len) + us_buf;
}

// ============================================================
// LogFormatter::FormatItem — type-erased format token
// ============================================================
// Defined in the .cc file as required by the forward declaration in log.h.
// Uses std::function internally to avoid the need for subclassing,
// which would require access to the private nested type.

struct LogFormatter::FormatItem {
    using FormatFn = std::function<void(std::ostream&, const LogEvent&)>;
    FormatFn fn;

    explicit FormatItem(FormatFn f) : fn(std::move(f)) {}
};

// ============================================================
// LogFormatter
// ============================================================

LogFormatter::LogFormatter(const std::string& pattern)
    : pattern_(pattern) {
    parse_pattern();
}

std::string LogFormatter::format(const LogEvent& event) const {
    std::ostringstream oss;
    for (const auto& item : items_) {
        if (item) item->fn(oss, event);
    }
    return oss.str();
}

void LogFormatter::set_pattern(const std::string& pattern) {
    pattern_ = pattern;
    parse_pattern();
}

void LogFormatter::parse_pattern() {
    items_.clear();

    const char* p   = pattern_.c_str();
    const char* end = p + pattern_.size();
    std::string literal;

    auto flush = [&]() {
        if (!literal.empty()) {
            auto text = std::move(literal);
            items_.push_back(std::make_unique<FormatItem>(
                [text = std::move(text)](std::ostream& os, const LogEvent&) {
                    os << text;
                }));
            literal.clear();
        }
    };

    while (p < end) {
        if (*p != '%') {
            literal.push_back(*p++);
            continue;
        }

        ++p; // skip '%'
        if (p >= end) {
            literal.push_back('%');
            break;
        }

        flush();

        switch (*p) {
            case '%':
                items_.push_back(std::make_unique<FormatItem>(
                    [](std::ostream& os, const LogEvent&) { os << '%'; }));
                break;
            case 't':
                items_.push_back(std::make_unique<FormatItem>(
                    [](std::ostream& os, const LogEvent& event) {
                        os << event.format_time("%Y-%m-%d %H:%M:%S");
                    }));
                break;
            case 'T':
                items_.push_back(std::make_unique<FormatItem>(
                    [](std::ostream& os, const LogEvent& event) {
                        os << event.thread_id;
                    }));
                break;
            case 'F':
                items_.push_back(std::make_unique<FormatItem>(
                    [](std::ostream& os, const LogEvent& event) {
                        os << event.fiber_id;
                    }));
                break;
            case 'N':
                items_.push_back(std::make_unique<FormatItem>(
                    [](std::ostream& os, const LogEvent& event) {
                        os << event.logger_name;
                    }));
                break;
            case 'L':
                items_.push_back(std::make_unique<FormatItem>(
                    [](std::ostream& os, const LogEvent& event) {
                        os << log_level_name(event.level);
                    }));
                break;
            case 'f':
                items_.push_back(std::make_unique<FormatItem>(
                    [](std::ostream& os, const LogEvent& event) {
                        if (event.file && event.file[0]) {
                            const char* s = strrchr(event.file, '/');
                            os << (s ? s + 1 : event.file);
                        } else {
                            os << "???";
                        }
                    }));
                break;
            case 'l':
                items_.push_back(std::make_unique<FormatItem>(
                    [](std::ostream& os, const LogEvent& event) {
                        os << event.line;
                    }));
                break;
            case 'm':
                items_.push_back(std::make_unique<FormatItem>(
                    [](std::ostream& os, const LogEvent& event) {
                        os << event.message;
                    }));
                break;
            case 'n':
                items_.push_back(std::make_unique<FormatItem>(
                    [](std::ostream& os, const LogEvent&) { os << '\n'; }));
                break;
            default:
                // Unknown specifier: keep literal %X
                literal.push_back('%');
                literal.push_back(*p);
                break;
        }
        ++p;
    }
    flush();
}

// ============================================================
// ANSI color helpers
// ============================================================

namespace {

const char* level_color(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::TRACE: return "\033[37m";    // white
        case LogLevel::DEBUG: return "\033[36m";    // cyan
        case LogLevel::INFO:  return "\033[32m";    // green
        case LogLevel::WARN:  return "\033[33m";    // yellow
        case LogLevel::ERROR: return "\033[31m";    // red
        case LogLevel::FATAL: return "\033[1;35m";  // bold magenta
        default:              return "\033[0m";
    }
}
constexpr const char* kColorReset = "\033[0m";

inline bool is_tty(FILE* fp) {
    int fd = fileno(fp);
    return fd >= 0 && isatty(fd) == 1;
}

} // anonymous namespace

// ============================================================
// ConsoleAppender
// ============================================================

ConsoleAppender::ConsoleAppender(bool use_stderr)
    : use_stderr_(use_stderr)
    , formatter_(std::make_shared<LogFormatter>()) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IOLBF, 0);
}

void ConsoleAppender::log(const LogEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string formatted = formatter_->format(event);

    bool to_stderr = use_stderr_ &&
        static_cast<int>(event.level) >= static_cast<int>(LogLevel::WARN);
    FILE* out = to_stderr ? stderr : stdout;

    if (is_tty(out)) {
        fprintf(out, "%s%s%s", level_color(event.level),
                formatted.c_str(), kColorReset);
    } else {
        fwrite(formatted.data(), 1, formatted.size(), out);
    }

    if (to_stderr) fflush(out);
}

void ConsoleAppender::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    fflush(stdout);
    fflush(stderr);
}

void ConsoleAppender::set_formatter(std::shared_ptr<LogFormatter> fmt) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fmt) formatter_ = std::move(fmt);
}

std::shared_ptr<LogFormatter> ConsoleAppender::get_formatter() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    return formatter_;
}

// ============================================================
// FileAppender
// ============================================================

FileAppender::FileAppender(const std::string& filename)
    : filename_(filename)
    , formatter_(std::make_shared<LogFormatter>()) {
    fd_ = ::open(filename_.c_str(),
                 O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
    if (fd_ < 0) {
        fprintf(stderr, "[zero::FileAppender] open('%s') failed: %s\n",
                filename_.c_str(), strerror(errno));
    } else {
        struct stat st;
        if (::fstat(fd_, &st) == 0)
            current_size_ = static_cast<size_t>(st.st_size);
        time_t now = time(nullptr);
        struct tm tm_buf;
        localtime_r(&now, &tm_buf);
        last_rotation_day_ = tm_buf.tm_yday;
    }
}

FileAppender::~FileAppender() {
    if (fd_ >= 0) {
        flush();
        ::close(fd_);
        fd_ = -1;
    }
}

void FileAppender::log(const LogEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (fd_ < 0) return;

    rotate_if_needed();
    if (fd_ < 0) return;

    std::string formatted = formatter_->format(event);
    const char* data      = formatted.data();
    size_t remaining      = formatted.size();

    while (remaining > 0) {
        ssize_t n = ::write(fd_, data, remaining);
        if (n < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "[zero::FileAppender] write(%s) failed: %s\n",
                    filename_.c_str(), strerror(errno));
            ::close(fd_);
            fd_ = -1;
            return;
        }
        remaining -= static_cast<size_t>(n);
        data      += n;
    }
    current_size_ += formatted.size();
}

void FileAppender::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fd_ >= 0) {
        int rc;
        do { rc = ::fdatasync(fd_); } while (rc < 0 && errno == EINTR);
    }
}

void FileAppender::set_formatter(std::shared_ptr<LogFormatter> fmt) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fmt) formatter_ = std::move(fmt);
}

std::shared_ptr<LogFormatter> FileAppender::get_formatter() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    return formatter_;
}

void FileAppender::enable_rotation(size_t max_size_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_size_ = max_size_bytes;
}

void FileAppender::enable_daily_rotation() {
    std::lock_guard<std::mutex> lock(mutex_);
    daily_rotation_ = true;
}

bool FileAppender::reopen() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    fd_ = ::open(filename_.c_str(),
                 O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
    if (fd_ >= 0) {
        current_size_ = 0;
        struct stat st;
        if (::fstat(fd_, &st) == 0)
            current_size_ = static_cast<size_t>(st.st_size);
    }
    return fd_ >= 0;
}

void FileAppender::rotate_if_needed() {
    // Called under mutex_
    bool need = false;

    if (max_size_ > 0 && current_size_ >= max_size_) need = true;

    if (daily_rotation_) {
        time_t now = time(nullptr);
        struct tm tm_buf;
        localtime_r(&now, &tm_buf);
        if (tm_buf.tm_yday != last_rotation_day_) {
            last_rotation_day_ = tm_buf.tm_yday;
            need = true;
        }
    }

    if (!need) return;

    ::close(fd_);
    fd_ = -1;

    if (max_files_ > 0) {
        // Remove oldest backup
        std::string oldest = filename_ + "." + std::to_string(max_files_ - 1);
        ::unlink(oldest.c_str());

        // Shift backups: filename.i -> filename.i+1
        for (size_t i = max_files_ - 1; i > 0; --i) {
            std::string from = filename_ + "." + std::to_string(i - 1);
            std::string to   = filename_ + "." + std::to_string(i);
            ::rename(from.c_str(), to.c_str());
        }
        // Current -> filename.0
        ::rename(filename_.c_str(), (filename_ + ".0").c_str());
    } else {
        char ts[32];
        time_t now = time(nullptr);
        struct tm tm_buf;
        localtime_r(&now, &tm_buf);
        strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tm_buf);
        ::rename(filename_.c_str(), (filename_ + "." + ts).c_str());
    }

    fd_ = ::open(filename_.c_str(),
                 O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
    current_size_ = 0;

    if (fd_ < 0) {
        fprintf(stderr, "[zero::FileAppender] rotate reopen('%s') failed: %s\n",
                filename_.c_str(), strerror(errno));
    }
}

// ============================================================
// SyslogAppender
// ============================================================

namespace {

int to_syslog_priority(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::TRACE: return LOG_DEBUG;
        case LogLevel::DEBUG: return LOG_DEBUG;
        case LogLevel::INFO:  return LOG_INFO;
        case LogLevel::WARN:  return LOG_WARNING;
        case LogLevel::ERROR: return LOG_ERR;
        case LogLevel::FATAL: return LOG_CRIT;
        default:              return LOG_INFO;
    }
}

} // anonymous namespace

SyslogAppender::SyslogAppender(const std::string& ident)
    : ident_(ident)
    , formatter_(std::make_shared<LogFormatter>("%m")) {
    ::openlog(ident_.c_str(), LOG_PID | LOG_NDELAY, LOG_USER);
}

SyslogAppender::~SyslogAppender() {
    ::closelog();
}

void SyslogAppender::log(const LogEvent& event) {
    int pri = to_syslog_priority(event.level);
    std::string msg = formatter_->format(event);
    ::syslog(pri, "%s", msg.c_str());
}

void SyslogAppender::flush() {
    // syslog has no explicit flush — no-op
}

void SyslogAppender::set_formatter(std::shared_ptr<LogFormatter> fmt) {
    if (fmt) formatter_ = std::move(fmt);
}

std::shared_ptr<LogFormatter> SyslogAppender::get_formatter() const {
    return formatter_;
}

// ============================================================
// AsyncLogAppender — double-buffered background writer
// ============================================================

AsyncLogAppender::AsyncLogAppender(std::shared_ptr<LogAppender> appender,
                                     size_t ring_buffer_size)
    : appender_(std::move(appender))
    , max_size_(ring_buffer_size) {
    buffer_.reserve(max_size_);
    swap_buffer_.reserve(max_size_);
    worker_thread_ = std::thread(&AsyncLogAppender::worker_loop, this);
}

AsyncLogAppender::~AsyncLogAppender() {
    stop();
}

void AsyncLogAppender::log(const LogEvent& event) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (buffer_.size() >= max_size_) {
            ++dropped_;
            return;
        }
        buffer_.push_back(event);
    }
    cv_.notify_one();
}

void AsyncLogAppender::flush() {
    cv_.notify_one();
    std::this_thread::yield();
    if (appender_) appender_->flush();
}

void AsyncLogAppender::set_formatter(std::shared_ptr<LogFormatter> fmt) {
    if (appender_) appender_->set_formatter(std::move(fmt));
}

std::shared_ptr<LogFormatter> AsyncLogAppender::get_formatter() const {
    return appender_ ? appender_->get_formatter() : nullptr;
}

void AsyncLogAppender::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_.exchange(false)) return;
    }
    cv_.notify_all();
    if (worker_thread_.joinable()) worker_thread_.join();

    if (appender_) {
        for (auto& e : buffer_)      appender_->log(e);
        for (auto& e : swap_buffer_) appender_->log(e);
        appender_->flush();
    }
    buffer_.clear();
    swap_buffer_.clear();
}

void AsyncLogAppender::worker_loop() {
    while (running_.load(std::memory_order_relaxed)) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return !running_.load(std::memory_order_relaxed) ||
                       !buffer_.empty();
            });
            if (!running_.load(std::memory_order_relaxed) && buffer_.empty())
                return;
            buffer_.swap(swap_buffer_);
        }
        if (appender_) {
            for (const auto& e : swap_buffer_) appender_->log(e);
            appender_->flush();
        }
        swap_buffer_.clear();
    }
}

// ============================================================
// Global logger registry
// ============================================================

namespace {

zero::Mutex                            s_registry_mutex;
std::unordered_map<std::string, Logger*> s_logger_map;
std::vector<Logger*>                   s_logger_list;
Logger*                                s_root = nullptr;

std::pair<std::string, std::string> split_name(const std::string& name) {
    auto pos = name.rfind('.');
    if (pos == std::string::npos) return {"", name};
    return {name.substr(0, pos), name.substr(pos + 1)};
}

// Recursively ensure all ancestor loggers exist along the path.
// parent_ is NOT set here (it's private to Logger). Instead,
// collect_appenders() resolves parents by name at log time.
Logger* ensure_ancestors(const std::string& name) {
    auto [parent_name, leaf] = split_name(name);
    if (parent_name.empty()) return Logger::root();

    Logger* parent = nullptr;
    {
        zero::LockGuard<zero::Mutex> lock(s_registry_mutex);
        auto it = s_logger_map.find(parent_name);
        if (it != s_logger_map.end()) parent = it->second;
    }

    if (!parent) {
        ensure_ancestors(parent_name); // ensure grandparent exists
        parent = new Logger(parent_name);

        zero::LockGuard<zero::Mutex> lock(s_registry_mutex);
        auto it = s_logger_map.find(parent_name);
        if (it != s_logger_map.end()) {
            delete parent;
            parent = it->second;
        } else {
            s_logger_map[parent_name] = parent;
            s_logger_list.push_back(parent);
        }
    }
    return parent;
}

} // anonymous namespace

// ============================================================
// Logger
// ============================================================

Logger::Logger(const std::string& name)
    : name_(name) {
    Logger* r = root();
    if (r && r != this) level_ = r->level();
}

Logger::~Logger() = default;

void Logger::log(LogLevel level, const char* file, int line,
                 const std::string& msg) {
    if (static_cast<int>(level) < static_cast<int>(level_)) return;

    LogEvent event;
    event.file        = file;
    event.line        = line;
    event.level       = level;
    event.logger_name = name_;
    event.message     = msg;

    struct timeval tv;
    if (::gettimeofday(&tv, nullptr) == 0) {
        event.timestamp_us = static_cast<int64_t>(tv.tv_sec)  * 1000000LL
                           + static_cast<int64_t>(tv.tv_usec);
    }
    event.thread_id = static_cast<uint64_t>(::syscall(SYS_gettid));
    event.fiber_id  = Fiber::GetFiberId();

    std::vector<std::shared_ptr<LogAppender>> appenders;
    collect_appenders(appenders);

    for (auto& a : appenders) {
        if (a) a->log(event);
    }

    if (level == LogLevel::FATAL) {
        for (auto& a : appenders) if (a) a->flush();
        std::abort();
    }
}

void Logger::add_appender(std::shared_ptr<LogAppender> appender) {
    if (!appender) return;
    std::lock_guard<std::mutex> lock(mutex_);
    appenders_.push_back(std::move(appender));
}

void Logger::clear_appenders() {
    std::lock_guard<std::mutex> lock(mutex_);
    appenders_.clear();
}

void Logger::collect_appenders(
    std::vector<std::shared_ptr<LogAppender>>& out) const {
    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        for (auto& a : appenders_) out.push_back(a);
    }
    if (inherit_appenders_) {
        // Resolve parent by name: strip last component from logger name
        auto pos = name_.rfind('.');
        if (pos != std::string::npos) {
            std::string parent_name = name_.substr(0, pos);
            Logger* p = Logger::get(parent_name);
            if (p && p != this) p->collect_appenders(out);
        } else if (this != root()) {
            Logger* r = root();
            if (r && r != this) r->collect_appenders(out);
        }
    }
}

// Static: hierarchical logger lookup / creation
Logger* Logger::get(const std::string& name) {
    {
        zero::LockGuard<zero::Mutex> lock(s_registry_mutex);
        auto it = s_logger_map.find(name);
        if (it != s_logger_map.end()) return it->second;
    }

    if (!s_root) root();

    auto* logger = new Logger(name);
    ensure_ancestors(name);  // ensure parent loggers exist in the registry

    {
        zero::LockGuard<zero::Mutex> lock(s_registry_mutex);
        auto it = s_logger_map.find(name);
        if (it != s_logger_map.end()) {
            delete logger;
            return it->second;
        }
        s_logger_map[name] = logger;
        s_logger_list.push_back(logger);
    }
    return logger;
}

// Static: root logger singleton
Logger* Logger::root() {
    if (ZERO_LIKELY(s_root != nullptr)) return s_root;

    static zero::Mutex init_lock;
    zero::LockGuard<zero::Mutex> lock(init_lock);

    if (s_root == nullptr) {
        s_root = new Logger("root");
        s_root->set_level(LogLevel::INFO);
        s_root->set_inherit_appenders(false);
        s_root->add_appender(std::make_shared<ConsoleAppender>(true));

        s_logger_map["root"] = s_root;
        s_logger_list.push_back(s_root);
    }
    return s_root;
}

// Static: snapshot of all loggers
const std::vector<Logger*>& Logger::all_loggers() {
    return s_logger_list;
}

} // namespace zero
