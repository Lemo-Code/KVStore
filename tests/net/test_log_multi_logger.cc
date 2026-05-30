/**
 * @file test_log_multi_logger.cc
 * @brief 多 Logger 写同一文件 / 各写独立文件：行完整性、条数、串文件、乱码。
 */
#include "test_common.h"

#include "log/log.h"
#include "thread/module.h"

#include <cstdio>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

std::string Tmp(const char* suffix) {
  return net_test::LogPath("multi_logger_" + std::to_string(getpid()) + suffix);
}

size_t CountSubstr(const std::string& hay, const std::string& needle) {
  size_t n = 0;
  size_t pos = 0;
  while ((pos = hay.find(needle, pos)) != std::string::npos) {
    ++n;
    pos += needle.size();
  }
  return n;
}

size_t CountLines(const std::string& content) {
  if (content.empty()) {
    return 0;
  }
  size_t lines = 0;
  for (char c : content) {
    if (c == '\n') {
      ++lines;
    }
  }
  if (content.back() != '\n') {
    ++lines;
  }
  return lines;
}

void CheckNoGarbledBytes(const std::string& content) {
  NET_CHECK(content.find('\0') == std::string::npos);
  for (unsigned char c : content) {
    if (c < 0x09) {
      NET_CHECK(c == '\n' || c == '\t');
    }
  }
}

/** 每行须为 |L=logger|S=seq|payload|，且一行内仅出现一个 logger 标记 */
void CheckLineMarkers(const std::string& content,
                      const std::vector<std::string>& allowed_loggers) {
  std::istringstream iss(content);
  std::string line;
  size_t line_no = 0;
  while (std::getline(iss, line)) {
    ++line_no;
    if (line.empty()) {
      continue;
    }
    const size_t lpos = line.find("|L=");
    const size_t spos = line.find("|S=");
    const size_t ppos = line.find("|payload=");
    NET_CHECK(lpos != std::string::npos);
    NET_CHECK(spos != std::string::npos);
    NET_CHECK(ppos != std::string::npos);
    NET_CHECK(lpos < spos && spos < ppos);

    const size_t lend = line.find('|', lpos + 3);
    NET_CHECK(lend != std::string::npos);
    const std::string logger = line.substr(lpos + 3, lend - (lpos + 3));

    bool ok = false;
    for (const auto& name : allowed_loggers) {
      if (logger == name) {
        ok = true;
        break;
      }
    }
    NET_CHECK(ok);

    size_t logger_hits = 0;
    for (const auto& name : allowed_loggers) {
      if (line.find("|L=" + name + "|") != std::string::npos) {
        ++logger_hits;
      }
    }
    NET_CHECK(logger_hits == 1);
  }
  NET_CHECK(line_no > 0);
}

void LogBatch(const net::Logger::ptr& logger, const std::string& logger_id,
              int begin, int count, bool with_utf8) {
  for (int i = 0; i < count; ++i) {
    const int seq = begin + i;
    if (with_utf8 && (seq % 17 == 0)) {
      NET_LOG_FMT_INFO(logger, "|L=%s|S=%d|payload=中文seq_%d|",
                       logger_id.c_str(), seq, seq);
    } else {
      NET_LOG_FMT_INFO(logger, "|L=%s|S=%d|payload=ok_%d|", logger_id.c_str(),
                       seq, seq);
    }
  }
}

struct LoggerSet {
  std::vector<net::Logger::ptr> loggers;
  std::vector<std::string> names;
};

LoggerSet MakeLoggers(bool async, int n) {
  LoggerSet set;
  for (int i = 0; i < n; ++i) {
    const std::string name = "svc_" + std::to_string(i);
    net::Logger::ptr lg;
    if (async) {
      lg = net::AsyncLoggerMgr::GetInstance()->getLogger(name);
    } else {
      lg = net::LoggerMgr::GetInstance()->getLogger(name);
    }
    lg->clearAppenders();
    lg->setLevel(net::LogLevel::INFO);
    lg->setFormatter("[%c] %m%n");
    set.loggers.push_back(lg);
    set.names.push_back(name);
  }
  return set;
}

void TestManyLoggersOneFile(bool async) {
  const std::string path = Tmp(async ? "_one_async.log" : "_one_sync.log");
  const int kLoggers = 3;
  const int kPerLogger = 400;

  LoggerSet set = MakeLoggers(async, kLoggers);
  for (auto& lg : set.loggers) {
    lg->addAppender(net::LogAppender::ptr(new net::FileLogAppender(path)));
  }

  for (int i = 0; i < kLoggers; ++i) {
    LogBatch(set.loggers[static_cast<size_t>(i)], set.names[static_cast<size_t>(i)],
             i * 10000, kPerLogger, true);
  }

  if (async) {
    net_test::FlushAsyncLogs();
  }

  const std::string content = net_test::ReadFile(path);
  CheckNoGarbledBytes(content);
  CheckLineMarkers(content, set.names);

  const size_t total_lines = CountLines(content);
  const size_t expected =
      static_cast<size_t>(kLoggers) * static_cast<size_t>(kPerLogger);
  NET_CHECK(total_lines == expected);

  for (int i = 0; i < kLoggers; ++i) {
    const std::string marker = "|L=" + set.names[static_cast<size_t>(i)] + "|";
    NET_CHECK(CountSubstr(content, marker) == static_cast<size_t>(kPerLogger));
  }

  NET_CHECK(content.find("中文seq_") != std::string::npos);
  std::printf("  PASS one_file %s (%zu lines)\n", async ? "async" : "sync",
              total_lines);
}

void TestManyLoggersManyFiles(bool async) {
  const int kLoggers = 4;
  const int kPerLogger = 300;

  LoggerSet set = MakeLoggers(async, kLoggers);
  std::vector<std::string> paths;
  paths.reserve(static_cast<size_t>(kLoggers));

  for (int i = 0; i < kLoggers; ++i) {
    const std::string path =
        Tmp(async ? "_many_async_" : "_many_sync_") + std::to_string(i) + ".log";
    paths.push_back(path);
    set.loggers[static_cast<size_t>(i)]->addAppender(
        net::LogAppender::ptr(new net::FileLogAppender(path)));
  }

  for (int i = 0; i < kLoggers; ++i) {
    LogBatch(set.loggers[static_cast<size_t>(i)], set.names[static_cast<size_t>(i)],
             0, kPerLogger, false);
  }

  if (async) {
    net_test::FlushAsyncLogs();
  }

  for (int i = 0; i < kLoggers; ++i) {
    const std::string content = net_test::ReadFile(paths[static_cast<size_t>(i)]);
    CheckNoGarbledBytes(content);
    CheckLineMarkers(content, {set.names[static_cast<size_t>(i)]});

    NET_CHECK(CountLines(content) == static_cast<size_t>(kPerLogger));
    NET_CHECK(CountSubstr(content, "|L=" + set.names[static_cast<size_t>(i)] + "|") ==
              static_cast<size_t>(kPerLogger));

    for (int j = 0; j < kLoggers; ++j) {
      if (i == j) {
        continue;
      }
      const std::string foreign = "|L=" + set.names[static_cast<size_t>(j)] + "|";
      NET_CHECK(CountSubstr(content, foreign) == 0);
    }
  }
  std::printf("  PASS many_files %s\n", async ? "async" : "sync");
}

void TestMtManyLoggersOneFile(bool async) {
  const std::string path = Tmp(async ? "_mt_one_async.log" : "_mt_one_sync.log");
  const int kLoggers = 3;
  const int kThreads = 6;
  const int kPerThreadPerLogger = 80;

  LoggerSet set = MakeLoggers(async, kLoggers);
  for (auto& lg : set.loggers) {
    lg->addAppender(net::LogAppender::ptr(new net::FileLogAppender(path)));
  }

  std::vector<net::Thread::ptr> workers;
  for (int t = 0; t < kThreads; ++t) {
    workers.push_back(net::Thread::ptr(new net::Thread(
        [&set, t, kPerThreadPerLogger]() {
          for (int i = 0; i < kLoggers; ++i) {
            LogBatch(set.loggers[static_cast<size_t>(i)],
                     set.names[static_cast<size_t>(i)], t * 1000 + i * 100,
                     kPerThreadPerLogger, (t + i) % 5 == 0);
          }
        },
        "mlog_t" + std::to_string(t))));
  }
  for (auto& w : workers) {
    w->join();
  }

  if (async) {
    net_test::FlushAsyncLogs();
  }

  const std::string content = net_test::ReadFile(path);
  CheckNoGarbledBytes(content);
  CheckLineMarkers(content, set.names);

  const size_t expected =
      static_cast<size_t>(kLoggers) * static_cast<size_t>(kThreads) *
      static_cast<size_t>(kPerThreadPerLogger);
  NET_CHECK(CountLines(content) == expected);

  for (int i = 0; i < kLoggers; ++i) {
    const size_t want = static_cast<size_t>(kThreads) *
                        static_cast<size_t>(kPerThreadPerLogger);
    NET_CHECK(CountSubstr(content, "|L=" + set.names[static_cast<size_t>(i)] + "|") ==
              want);
  }
  std::printf("  PASS mt_one_file %s (%zu lines)\n", async ? "async" : "sync",
              expected);
}

/** 两个 Logger 共享同一 FileLogAppender 实例（单锁）写同一文件 */
void TestSharedAppender(bool async) {
  const std::string path = Tmp(async ? "_shared_app.log" : "_shared_app_sync.log");
  const int kPer = 200;

  auto app = net::LogAppender::ptr(new net::FileLogAppender(path));
  LoggerSet set = MakeLoggers(async, 2);
  for (auto& lg : set.loggers) {
    lg->addAppender(app);
  }

  std::vector<net::Thread::ptr> workers;
  workers.push_back(net::Thread::ptr(new net::Thread(
      [&]() { LogBatch(set.loggers[0], set.names[0], 0, kPer, true); },
      "shared_a")));
  workers.push_back(net::Thread::ptr(new net::Thread(
      [&]() { LogBatch(set.loggers[1], set.names[1], 0, kPer, true); },
      "shared_b")));
  for (auto& w : workers) {
    w->join();
  }

  if (async) {
    net_test::FlushAsyncLogs();
  }

  const std::string content = net_test::ReadFile(path);
  CheckNoGarbledBytes(content);
  CheckLineMarkers(content, set.names);
  NET_CHECK(CountLines(content) == static_cast<size_t>(2 * kPer));
  NET_CHECK(CountSubstr(content, "|L=" + set.names[0] + "|") ==
            static_cast<size_t>(kPer));
  NET_CHECK(CountSubstr(content, "|L=" + set.names[1] + "|") ==
            static_cast<size_t>(kPer));
  std::printf("  PASS shared_appender %s\n", async ? "async" : "sync");
}

}  // namespace

int main() {
  std::printf("=== test_log_multi_logger ===\n");

  TestManyLoggersOneFile(false);
  TestManyLoggersOneFile(true);
  TestManyLoggersManyFiles(false);
  TestManyLoggersManyFiles(true);
  TestMtManyLoggersOneFile(false);
  TestMtManyLoggersOneFile(true);
  TestSharedAppender(false);
  TestSharedAppender(true);

  std::printf("PASS test_log_multi_logger\n");
  return 0;
}
