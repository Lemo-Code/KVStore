// test_log.cpp — Comprehensive Logger unit tests
// Tests Logger singleton/hierarchy (root, named loggers),
// Log levels (TRACE, DEBUG, INFO, WARN, ERROR, FATAL),
// log() with format args, set_level() filtering,
// ZERO_LOG_INFO/DEBUG/WARN/ERROR macros,
// ConsoleAppender (stdout/stderr output),
// FileAppender (write to temp file, verify content),
// LogFormatter with pattern placeholders,
// Timestamp formatting, thread safety with concurrent logging,
// AsyncLogAppender, logger hierarchy, edge cases.

#include <gtest/gtest.h>
#include "zero/zero.h"
#include "zero/log/log.h"

#include <fstream>
#include <cstdio>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <sstream>

using namespace zero;

// ============================================================
// Logger hierarchy: root() and get()
// ============================================================

TEST(Log, RootLoggerExists) {
    Logger* root = Logger::root();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->name(), "");
}

TEST(Log, NamedLogger) {
    Logger* log = Logger::get("test.component");
    ASSERT_NE(log, nullptr);
    EXPECT_EQ(log->name(), "test.component");
}

TEST(Log, SameNameReturnsSameLogger) {
    Logger* a = Logger::get("myapp.server");
    Logger* b = Logger::get("myapp.server");
    EXPECT_EQ(a, b);
}

TEST(Log, DifferentNamesReturnDifferentLoggers) {
    Logger* a = Logger::get("app.module1");
    Logger* b = Logger::get("app.module2");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);
}

TEST(Log, AllLoggers) {
    Logger::get("test.logger1");
    Logger::get("test.logger2");
    const auto& loggers = Logger::all_loggers();
    EXPECT_GE(loggers.size(), 2u);
}

// ============================================================
// Log levels
// ============================================================

TEST(Log, LogLevelNames) {
    EXPECT_STREQ(log_level_name(LogLevel::TRACE), "TRACE");
    EXPECT_STREQ(log_level_name(LogLevel::DEBUG), "DEBUG");
    EXPECT_STREQ(log_level_name(LogLevel::INFO), "INFO ");
    EXPECT_STREQ(log_level_name(LogLevel::WARN), "WARN ");
    EXPECT_STREQ(log_level_name(LogLevel::ERROR), "ERROR");
    EXPECT_STREQ(log_level_name(LogLevel::FATAL), "FATAL");
}

TEST(Log, LogLevelOrdering) {
    EXPECT_LT(static_cast<int>(LogLevel::TRACE), static_cast<int>(LogLevel::DEBUG));
    EXPECT_LT(static_cast<int>(LogLevel::DEBUG), static_cast<int>(LogLevel::INFO));
    EXPECT_LT(static_cast<int>(LogLevel::INFO), static_cast<int>(LogLevel::WARN));
    EXPECT_LT(static_cast<int>(LogLevel::WARN), static_cast<int>(LogLevel::ERROR));
    EXPECT_LT(static_cast<int>(LogLevel::ERROR), static_cast<int>(LogLevel::FATAL));
}

// ============================================================
// set_level() and level filtering
// ============================================================

TEST(Log, SetAndGetLevel) {
    Logger* log = Logger::get("test.set_level");
    ASSERT_NE(log, nullptr);

    log->set_level(LogLevel::TRACE);
    EXPECT_EQ(log->level(), LogLevel::TRACE);

    log->set_level(LogLevel::DEBUG);
    EXPECT_EQ(log->level(), LogLevel::DEBUG);

    log->set_level(LogLevel::INFO);
    EXPECT_EQ(log->level(), LogLevel::INFO);

    log->set_level(LogLevel::WARN);
    EXPECT_EQ(log->level(), LogLevel::WARN);

    log->set_level(LogLevel::ERROR);
    EXPECT_EQ(log->level(), LogLevel::ERROR);

    log->set_level(LogLevel::FATAL);
    EXPECT_EQ(log->level(), LogLevel::FATAL);
}

TEST(Log, IsTraceIsDebug) {
    Logger* log = Logger::get("test.level_check");
    log->set_level(LogLevel::TRACE);
    EXPECT_TRUE(log->is_trace());
    EXPECT_TRUE(log->is_debug());

    log->set_level(LogLevel::INFO);
    EXPECT_FALSE(log->is_trace());
    EXPECT_FALSE(log->is_debug());

    log->set_level(LogLevel::WARN);
    EXPECT_FALSE(log->is_trace());
    EXPECT_FALSE(log->is_debug());
}

// ============================================================
// log() at each level
// ============================================================

TEST(Log, LogAtEachLevel) {
    Logger* log = Logger::get("test.levels");
    log->set_level(LogLevel::TRACE);

    // These should not crash
    log->log(LogLevel::TRACE, __FILE__, __LINE__, "trace message");
    log->log(LogLevel::DEBUG, __FILE__, __LINE__, "debug message");
    log->log(LogLevel::INFO, __FILE__, __LINE__, "info message");
    log->log(LogLevel::WARN, __FILE__, __LINE__, "warn message");
    log->log(LogLevel::ERROR, __FILE__, __LINE__, "error message");

    SUCCEED();
}

TEST(Log, ConvenienceMethods) {
    Logger* log = Logger::get("test.convenience");
    log->set_level(LogLevel::TRACE);

    log->trace(__FILE__, __LINE__, "trace via method");
    log->debug(__FILE__, __LINE__, "debug via method");
    log->info(__FILE__, __LINE__, "info via method");
    log->warn(__FILE__, __LINE__, "warn via method");
    log->error(__FILE__, __LINE__, "error via method");

    SUCCEED();
}

TEST(Log, EmptyMessage) {
    Logger* log = Logger::get("test.empty");
    log->set_level(LogLevel::INFO);
    log->info(__FILE__, __LINE__, "");
    SUCCEED();
}

TEST(Log, LongMessage) {
    Logger* log = Logger::get("test.long");
    log->set_level(LogLevel::INFO);
    std::string long_msg(10000, 'x');
    log->info(__FILE__, __LINE__, long_msg);
    SUCCEED();
}

// ============================================================
// ZERO_LOG macros
// ============================================================

TEST(Log, MacrosDontCrash) {
    Logger* log = Logger::get("test.macros");
    log->set_level(LogLevel::INFO);

    // Using streaming macros
    ZERO_LOG_INFO(log, "info macro test " << 42);
    ZERO_LOG_WARN(log, "warn macro test " << 3.14);

    log->set_level(LogLevel::TRACE);
    ZERO_LOG_TRACE(log, "trace macro " << "test");
    ZERO_LOG_DEBUG(log, "debug macro " << 100);

    log->set_level(LogLevel::ERROR);
    ZERO_LOG_ERROR(log, "error macro " << "something_wrong");

    SUCCEED();
}

TEST(Log, RootMacrosDontCrash) {
    ZERO_ROOT_LOG_INFO("root info message " << 123);
    ZERO_ROOT_LOG_WARN("root warn message " << "warning");
    ZERO_ROOT_LOG_ERROR("root error message " << 456);

    SUCCEED();
}

TEST(Log, MacroLevelFiltering) {
    Logger* log = Logger::get("test.macro_filter");
    log->set_level(LogLevel::WARN);

    // These should be filtered out by the macro (no crash either way)
    ZERO_LOG_TRACE(log, "should be filtered");
    ZERO_LOG_DEBUG(log, "should be filtered");
    ZERO_LOG_INFO(log, "should be filtered");
    ZERO_LOG_WARN(log, "should appear");
    ZERO_LOG_ERROR(log, "should also appear");

    SUCCEED();
}

TEST(Log, MacroWithStreamExpr) {
    Logger* log = Logger::get("test.stream");
    log->set_level(LogLevel::INFO);

    ZERO_LOG_INFO(log, "multi-part: " << 1 << ", " << 2 << ", " << 3);
    ZERO_LOG_INFO(log, "string: " << std::string("hello"));
    ZERO_LOG_INFO(log, "bool: " << true << " and " << false);

    SUCCEED();
}

// ============================================================
// ConsoleAppender
// ============================================================

TEST(Log, ConsoleAppenderCreation) {
    ConsoleAppender appender(true);
    SUCCEED();
}

TEST(Log, ConsoleAppenderLog) {
    ConsoleAppender appender(true);
    Logger* log = Logger::get("test.console");
    log->set_level(LogLevel::INFO);
    log->add_appender(std::make_shared<ConsoleAppender>(false));

    log->info(__FILE__, __LINE__, "console log test");
    SUCCEED();
}

// ============================================================
// FileAppender — write to file and verify
// ============================================================

TEST(Log, FileAppenderWritesToFile) {
    std::string path = "/tmp/zero_test_log_" +
        std::to_string(getpid()) + ".log";
    std::remove(path.c_str());

    Logger* log = Logger::get("test.file");
    log->set_level(LogLevel::INFO);
    log->clear_appenders();
    log->add_appender(std::make_shared<FileAppender>(path));

    log->info(__FILE__, __LINE__, "file test message");
    log->error(__FILE__, __LINE__, "error in file");

    // Flush or give time for writes
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::ifstream ifs(path);
    EXPECT_TRUE(ifs.good());
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    EXPECT_FALSE(content.empty());
    // Content should contain our messages
    EXPECT_NE(content.find("file test message"), std::string::npos);
    EXPECT_NE(content.find("error in file"), std::string::npos);

    std::remove(path.c_str());
}

TEST(Log, FileAppenderMultipleLogs) {
    std::string path = "/tmp/zero_test_log_multi_" +
        std::to_string(getpid()) + ".log";
    std::remove(path.c_str());

    Logger* log = Logger::get("test.file_multi");
    log->set_level(LogLevel::TRACE);
    log->clear_appenders();
    log->add_appender(std::make_shared<FileAppender>(path));

    for (int i = 0; i < 50; ++i) {
        std::ostringstream oss;
        oss << "log message number " << i;
        log->info(__FILE__, __LINE__, oss.str());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::ifstream ifs(path);
    EXPECT_TRUE(ifs.good());
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());

    for (int i = 0; i < 50; ++i) {
        std::ostringstream oss;
        oss << "log message number " << i;
        EXPECT_NE(content.find(oss.str()), std::string::npos);
    }

    std::remove(path.c_str());
}

TEST(Log, FileAppenderSpecialCharacters) {
    std::string path = "/tmp/zero_test_log_special_" +
        std::to_string(getpid()) + ".log";
    std::remove(path.c_str());

    Logger* log = Logger::get("test.special");
    log->set_level(LogLevel::INFO);
    log->clear_appenders();
    log->add_appender(std::make_shared<FileAppender>(path));

    log->info(__FILE__, __LINE__, "special chars: !@#$%^&*()_+-={}[]|:;'<>,.?/~`");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::ifstream ifs(path);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("special chars:"), std::string::npos);

    std::remove(path.c_str());
}

TEST(Log, FileAppenderUnicode) {
    std::string path = "/tmp/zero_test_log_unicode_" +
        std::to_string(getpid()) + ".log";
    std::remove(path.c_str());

    Logger* log = Logger::get("test.unicode");
    log->set_level(LogLevel::INFO);
    log->clear_appenders();
    log->add_appender(std::make_shared<FileAppender>(path));

    log->info(__FILE__, __LINE__, "unicode: hello world 你好世界");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::ifstream ifs(path);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("hello world"), std::string::npos);

    std::remove(path.c_str());
}

// ============================================================
// LogFormatter
// ============================================================

TEST(Log, LogFormatterDefaultPattern) {
    LogFormatter fmt;
    EXPECT_FALSE(fmt.pattern().empty());

    LogEvent event;
    event.file = "test.cpp";
    event.line = 42;
    event.level = LogLevel::INFO;
    event.logger_name = "test.formatter";
    event.message = "test message";

    std::string output = fmt.format(event);
    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("INFO"), std::string::npos);
    EXPECT_NE(output.find("test.formatter"), std::string::npos);
    EXPECT_NE(output.find("test.cpp"), std::string::npos);
    EXPECT_NE(output.find("test message"), std::string::npos);
}

TEST(Log, LogFormatterCustomPattern) {
    LogFormatter fmt("%L: %m%n");
    EXPECT_EQ(fmt.pattern(), "%L: %m%n");

    LogEvent event;
    event.level = LogLevel::ERROR;
    event.message = "something broke";

    std::string output = fmt.format(event);
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("something broke"), std::string::npos);
}

TEST(Log, LogFormatterSetPattern) {
    LogFormatter fmt;
    fmt.set_pattern("[%L] %N - %m%n");

    LogEvent event;
    event.level = LogLevel::WARN;
    event.logger_name = "myapp";
    event.message = "warning text";

    std::string output = fmt.format(event);
    EXPECT_NE(output.find("WARN"), std::string::npos);
    EXPECT_NE(output.find("myapp"), std::string::npos);
    EXPECT_NE(output.find("warning text"), std::string::npos);
}

// ============================================================
// Timestamp in log output
// ============================================================

TEST(Log, TimestampFormatting) {
    std::string path = "/tmp/zero_test_log_ts_" +
        std::to_string(getpid()) + ".log";
    std::remove(path.c_str());

    Logger* log = Logger::get("test.timestamp");
    log->set_level(LogLevel::INFO);
    log->clear_appenders();

    auto file_appender = std::make_shared<FileAppender>(path);
    file_appender->set_formatter(std::make_shared<LogFormatter>(
        "%t [%L] %m%n"));
    log->add_appender(file_appender);

    log->info(__FILE__, __LINE__, "timestamp test");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::ifstream ifs(path);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    // Verify content is non-trivial
    EXPECT_GT(content.size(), 10u);
    EXPECT_NE(content.find("timestamp test"), std::string::npos);

    std::remove(path.c_str());
}

// ============================================================
// Logger hierarchy: appender inheritance
// ============================================================

TEST(Log, HierarchyAppenders) {
    Logger* parent = Logger::get("parent");
    parent->set_level(LogLevel::INFO);
    parent->clear_appenders();
    parent->add_appender(std::make_shared<ConsoleAppender>(false));

    Logger* child = Logger::get("parent.child");
    child->set_level(LogLevel::INFO);

    // Child inherits parent's appenders by default
    std::vector<std::shared_ptr<LogAppender>> collected;
    child->collect_appenders(collected);
    // Should have at least the parent's appender
    EXPECT_GE(collected.size(), 1u);
}

// ============================================================
// Thread safety: log from multiple threads
// ============================================================

TEST(Log, ThreadSafetyMultiThreadLogging) {
    Logger* log = Logger::get("test.thread_safety");
    log->set_level(LogLevel::INFO);
    log->clear_appenders();

    std::string path = "/tmp/zero_test_log_ts_" +
        std::to_string(getpid()) + ".log";
    log->add_appender(std::make_shared<FileAppender>(path));

    const int kThreads = 16;
    const int kPerThread = 50;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int i = 0; i < kPerThread; ++i) {
                std::ostringstream oss;
                oss << "thread " << t << " iteration " << i;
                log->info(__FILE__, __LINE__, oss.str());
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();

    // Verify file content
    std::ifstream ifs(path);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    EXPECT_GT(content.size(), 10u);

    for (int t = 0; t < kThreads; ++t) {
        std::ostringstream oss;
        oss << "thread " << t;
        EXPECT_NE(content.find(oss.str()), std::string::npos);
    }

    std::remove(path.c_str());
}

// ============================================================
// Edge cases
// ============================================================

TEST(Log, LogWithNullLoggerDoesNotCrash) {
    // Testing the macro's null check
    ZERO_LOG(nullptr, LogLevel::INFO, "should not crash");
    SUCCEED();
}

TEST(Log, MultipleFileAppenders) {
    std::string path1 = "/tmp/zero_test_multi1_" +
        std::to_string(getpid()) + ".log";
    std::string path2 = "/tmp/zero_test_multi2_" +
        std::to_string(getpid()) + ".log";
    std::remove(path1.c_str());
    std::remove(path2.c_str());

    Logger* log = Logger::get("test.multi_appender");
    log->set_level(LogLevel::INFO);
    log->clear_appenders();
    log->add_appender(std::make_shared<FileAppender>(path1));
    log->add_appender(std::make_shared<FileAppender>(path2));

    log->info(__FILE__, __LINE__, "dual appender test");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::ifstream ifs1(path1);
    std::ifstream ifs2(path2);
    EXPECT_TRUE(ifs1.good());
    EXPECT_TRUE(ifs2.good());

    std::remove(path1.c_str());
    std::remove(path2.c_str());
}

TEST(Log, ClearAppenders) {
    Logger* log = Logger::get("test.clear_appenders");
    log->add_appender(std::make_shared<ConsoleAppender>(false));
    log->clear_appenders();

    std::vector<std::shared_ptr<LogAppender>> collected;
    log->collect_appenders(collected);
    EXPECT_EQ(collected.size(), 0u);
}

TEST(Log, InheritAppendersToggle) {
    Logger* parent = Logger::get("inherit.parent");
    parent->add_appender(std::make_shared<ConsoleAppender>(false));

    Logger* child = Logger::get("inherit.parent.child");
    child->set_inherit_appenders(false);

    std::vector<std::shared_ptr<LogAppender>> collected;
    child->collect_appenders(collected);
    // With inherit off, should not get parent's appenders
    EXPECT_EQ(collected.size(), 0u);
}

// ============================================================
// AsyncLogAppender
// ============================================================

TEST(Log, AsyncAppenderWrapsOther) {
    auto console = std::make_shared<ConsoleAppender>(false);
    auto async = std::make_shared<AsyncLogAppender>(console, 1024);

    LogEvent event;
    event.message = "async test";
    event.level = LogLevel::INFO;

    async->log(event);

    // Stop and flush
    async->stop();

    SUCCEED();
}

// ============================================================
// LogEvent::should_log
// ============================================================

TEST(Log, ShouldLog) {
    LogEvent event;

    event.level = LogLevel::DEBUG;
    EXPECT_TRUE(event.should_log(LogLevel::TRACE));
    EXPECT_TRUE(event.should_log(LogLevel::DEBUG));
    EXPECT_FALSE(event.should_log(LogLevel::INFO));
    EXPECT_FALSE(event.should_log(LogLevel::WARN));

    event.level = LogLevel::ERROR;
    EXPECT_TRUE(event.should_log(LogLevel::DEBUG));
    EXPECT_TRUE(event.should_log(LogLevel::ERROR));
}

// ============================================================
// LogEvent::format_time
// ============================================================

TEST(Log, LogEventFormatTime) {
    LogEvent event;
    event.timestamp_us = 1719600000000000LL;  // Some timestamp

    std::string time_str = event.format_time("%Y-%m-%d %H:%M:%S");
    EXPECT_FALSE(time_str.empty());
}

// ============================================================
// SyslogAppender
// ============================================================

TEST(Log, SyslogAppenderCreation) {
    SyslogAppender appender("zero_test");
    SUCCEED();
}
