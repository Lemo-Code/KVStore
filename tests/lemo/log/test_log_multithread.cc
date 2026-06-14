/**
 * @file test_log_multithread.cc
 * @brief 多线程日志正确性：各 appender type + 并发 GetLogger + 层级 logger
 */
#include "test_common.h"

#include "lemo/log/appender_registry.h"
#include "lemo/log/log_paths.h"
#include "lemo/log/async_appender.h"
#include "lemo/log/console_appender.h"
#include "lemo/log/file_appender.h"
#include "lemo/log/log.h"
#include "lemo/log/logger_repository.h"
#include "lemo/log/pattern_layout.h"
#include "lemo/log/rolling_file_appender.h"
#include "lemo/thread/module.h"

#include <atomic>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace {

const int kThreads = 4;
const int kMsgsPerThread = 500;

std::string ReadAll(const std::string& path) {
  std::ifstream in(path.c_str());
  if (!in.good()) return "";
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

std::string ReadRollingLogs(const std::string& path, uint32_t max_files) {
  std::string content = ReadAll(path);
  for (uint32_t i = 1; i <= max_files; ++i) {
    content += ReadAll(path + "." + std::to_string(i));
  }
  return content;
}

std::string MakeMarker(int t, int m) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "W%02dM%04d", t, m);
  return std::string(buf);
}

size_t CountUniqueMarkers(const std::string& content, int threads,
                          int msgs_per_thread) {
  size_t count = 0;
  for (int t = 0; t < threads; ++t) {
    for (int m = 0; m < msgs_per_thread; ++m) {
      if (content.find(MakeMarker(t, m)) != std::string::npos) {
        ++count;
      }
    }
  }
  return count;
}

lemo::log::Logger::ptr MakeLogger(const std::string& name,
                                  lemo::log::Appender::ptr appender) {
  lemo::log::Logger::ptr logger =
      lemo::log::LoggerRepository::Instance().GetLogger(name);
  logger->ClearAppenders();
  logger->SetAdditive(false);
  logger->SetLevel(lemo::log::LogLevel::INFO);
  logger->SetLayout(lemo::log::Layout::ptr(new lemo::log::PatternLayout("%m")));
  logger->AddAppender(appender);
  return logger;
}

void RunWorkers(lemo::log::Logger::ptr logger, std::atomic<int>* errors) {
  std::vector<lemo::thread::Thread::ptr> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.push_back(lemo::thread::Thread::ptr(new lemo::thread::Thread(
        [logger, t, errors]() {
          for (int m = 0; m < kMsgsPerThread; ++m) {
            LEMO_LOG_INFO(logger) << MakeMarker(t, m);
          }
        },
        "log_t" + std::to_string(t))));
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->join();
  }
  (void)errors;
}

void TestFileType(const char* label,
                  const std::function<lemo::log::Appender::ptr()>& make_appender,
                  const std::string& path, bool rolling = false,
                  uint32_t max_files = 0) {
  std::remove(path.c_str());
  if (rolling) {
    for (uint32_t i = 1; i <= max_files; ++i) {
      std::remove((path + "." + std::to_string(i)).c_str());
    }
  }
  lemo::log::Logger::ptr logger =
      MakeLogger(std::string("mt_") + label, make_appender());
  std::atomic<int> errors{0};
  RunWorkers(logger, &errors);
  logger->Flush();

  const std::string content =
      rolling ? ReadRollingLogs(path, max_files) : ReadAll(path);
  LEMO_CHECK(!content.empty());
  const size_t expected =
      static_cast<size_t>(kThreads) * static_cast<size_t>(kMsgsPerThread);
  const size_t unique =
      CountUniqueMarkers(content, kThreads, kMsgsPerThread);
  LEMO_CHECK(unique == expected);
  std::printf("  PASS %-22s unique=%zu/%zu bytes=%zu\n", label, unique,
              expected, content.size());
}

void TestConsoleNoCrash() {
  lemo::log::Logger::ptr logger = MakeLogger(
      "mt_console", lemo::log::Appender::ptr(new lemo::log::ConsoleAppender()));
  std::atomic<int> errors{0};
  RunWorkers(logger, &errors);
  logger->Flush();
  std::printf("  PASS %-22s (no crash/hang)\n", "console");
}

void TestRegistryAsyncFile() {
  const std::string base = "/tmp/lemo_mt_registry_async.log";
  const std::string logger_name = "mt_registry_async/file";
  const std::string path =
      lemo::log::ResolveLoggerFilePath(logger_name, base);
  TestFileType(
      "registry_async/file",
      [base, logger_name]() {
        lemo::log::AppenderConfig cfg;
        cfg.properties["logger_name"] = logger_name;
        cfg.properties["path"] = base;
        cfg.properties["delegate"] = "file";
        return lemo::log::AppenderRegistry::Instance().Create("async", cfg);
      },
      path);
}

void TestConcurrentGetLogger() {
  const std::string path = "/tmp/lemo_mt_getlogger.log";
  std::remove(path.c_str());

  std::atomic<int> ready{0};
  std::vector<lemo::thread::Thread::ptr> threads;
  lemo::log::Logger::ptr loggers[kThreads];

  for (int t = 0; t < kThreads; ++t) {
    threads.push_back(lemo::thread::Thread::ptr(new lemo::thread::Thread(
        [&loggers, t, &ready]() {
          loggers[t] =
              lemo::log::LoggerRepository::Instance().GetLogger("concurrent.new");
          ++ready;
        },
        "getlog_" + std::to_string(t))));
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->join();
  }
  LEMO_CHECK(ready.load() == kThreads);
  for (int t = 1; t < kThreads; ++t) {
    LEMO_CHECK(loggers[t].get() == loggers[0].get());
  }

  lemo::log::Logger::ptr logger = loggers[0];
  logger->ClearAppenders();
  logger->SetAdditive(false);
  logger->SetLevel(lemo::log::LogLevel::INFO);
  logger->SetLayout(lemo::log::Layout::ptr(new lemo::log::PatternLayout("%m")));
  logger->AddAppender(lemo::log::Appender::ptr(new lemo::log::FileAppender(path)));

  for (int t = 0; t < kThreads; ++t) {
    for (int m = 0; m < 100; ++m) {
      LEMO_LOG_INFO(logger) << MakeMarker(t, m);
    }
  }
  logger->Flush();
  const size_t unique = CountUniqueMarkers(ReadAll(path), kThreads, 100);
  LEMO_CHECK(unique == static_cast<size_t>(kThreads * 100));
  std::printf("  PASS %-22s unique=%zu\n", "concurrent_getlogger", unique);
}

void TestHierarchyAdditive() {
  const std::string path = "/tmp/lemo_mt_hierarchy.log";
  std::remove(path.c_str());

  lemo::log::Logger::ptr root = LEMO_LOG_ROOT();
  root->ClearAppenders();
  root->SetAdditive(false);
  root->SetLevel(lemo::log::LogLevel::INFO);
  root->SetLayout(lemo::log::Layout::ptr(new lemo::log::PatternLayout("%m")));
  root->AddAppender(lemo::log::Appender::ptr(new lemo::log::FileAppender(path)));

  lemo::log::Logger::ptr child = LEMO_LOG_NAME("parent.child");
  child->ClearAppenders();
  child->SetAdditive(true);
  child->SetLevel(lemo::log::LogLevel::INFO);

  std::vector<lemo::thread::Thread::ptr> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.push_back(lemo::thread::Thread::ptr(new lemo::thread::Thread(
        [child, t]() {
          for (int m = 0; m < 200; ++m) {
            LEMO_LOG_INFO(child) << MakeMarker(t, m);
          }
        },
        "hier_" + std::to_string(t))));
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->join();
  }
  root->Flush();

  const std::string content = ReadAll(path);
  const size_t expected =
      static_cast<size_t>(kThreads) * 200u;
  const size_t count = CountUniqueMarkers(content, kThreads, 200);
  LEMO_CHECK(count == expected);
  std::printf("  PASS %-22s unique=%zu/%zu\n", "hierarchy_additive", count,
              expected);
}

}  // namespace

int main() {
  std::printf("=== lemo 多线程日志正确性测试 ===\n");
  std::printf("threads=%d msgs_per_thread=%d total=%d\n\n", kThreads,
              kMsgsPerThread, kThreads * kMsgsPerThread);

  TestFileType(
      "file",
      []() {
        return lemo::log::Appender::ptr(
            new lemo::log::FileAppender("/tmp/lemo_mt_file.log"));
      },
      "/tmp/lemo_mt_file.log");

  TestFileType(
      "rolling_file",
      []() {
        return lemo::log::Appender::ptr(new lemo::log::RollingFileAppender(
            "/tmp/lemo_mt_rolling.log", 4096, 5,
            lemo::log::RollInterval::kNone));
      },
      "/tmp/lemo_mt_rolling.log", true, 5);

  TestFileType(
      "async/file",
      []() {
        return lemo::log::MakeAsync(lemo::log::Appender::ptr(
            new lemo::log::FileAppender("/tmp/lemo_mt_async_file.log")));
      },
      "/tmp/lemo_mt_async_file.log");

  TestFileType(
      "async/rolling",
      []() {
        return lemo::log::MakeAsync(lemo::log::Appender::ptr(
            new lemo::log::RollingFileAppender("/tmp/lemo_mt_async_roll.log",
                                               4096, 5,
                                               lemo::log::RollInterval::kNone)));
      },
      "/tmp/lemo_mt_async_roll.log", true, 5);

  TestConsoleNoCrash();
  TestRegistryAsyncFile();
  TestConcurrentGetLogger();
  TestHierarchyAdditive();

  std::printf("\nPASS test_log_multithread\n");
  return 0;
}
