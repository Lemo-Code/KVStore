/**
 * @file test_log_full.cc
 * @brief 日志模块综合功能测试（同步 + 异步 + 全部 Sink 方案）。
 */
#include "test_common.h"

#include "log/log.h"

#include <ctime>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

std::string TmpPath(const char* suffix) {
  return net_test::LogPath("full_" + std::to_string(getpid()) + suffix);
}

time_t MakeTime(int y, int m, int d) {
  struct tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  t.tm_isdst = -1;
  return mktime(&t);
}

net::LogEvent::ptr MakeEvent(const net::Logger::ptr& logger,
                             net::LogLevel::Level level, time_t when,
                             const char* msg) {
  auto e = std::make_shared<net::LogEvent>(logger, level, __FILE__, __LINE__, 0,
                                           0, 0, static_cast<uint64_t>(when),
                                           "test");
  e->stream() << msg;
  return e;
}

}  // namespace

int main() {
  const std::string fixed = TmpPath("_fixed.log");
  const std::string roll = TmpPath("_roll.log");
  const std::string time_base = TmpPath("_time");
  const std::string ring0 = TmpPath("_ring.0");
  const std::string ring1 = TmpPath("_ring.1");
  const std::string ring2 = TmpPath("_ring.2");
  const std::string async_file = TmpPath("_async.log");

  // --- 级别过滤 ---
  {
    auto logger = net::LoggerMgr::GetInstance()->getLogger("full_level");
    logger->clearAppenders();
    logger->setLevel(net::LogLevel::WARN);
    logger->setFormatter("%m");
    logger->addAppender(
        net::LogAppender::ptr(new net::FileLogAppender(fixed + ".level")));
    NET_LOG_DEBUG(logger) << "skip-debug";
    NET_LOG_WARN(logger) << "keep-warn";
    const std::string c = net_test::ReadFile(fixed + ".level");
    NET_CHECK(c.find("skip-debug") == std::string::npos);
    NET_CHECK(c.find("keep-warn") != std::string::npos);
  }

  // --- 同步：SinkSet 全方案 ---
  {
    auto logger = net::LoggerMgr::GetInstance()->getLogger("full_sync");
    logger->clearAppenders();
    logger->setFormatter("%m");

    net::ApplySinkSet(logger, net::SinkSet::FixedFile(fixed));
    NET_LOG_INFO(logger) << "sync-fixed";
    logger->clearAppenders();

    net::ApplySinkSet(logger, net::SinkSet::SizeChain(roll, 24, 3));
    for (int i = 0; i < 8; ++i) {
      NET_LOG_FMT_INFO(logger, "s%02d", i);
    }
    logger->clearAppenders();

    net::ApplySinkSet(
        logger, net::SinkSet::CircularRing(TmpPath("_ring"), 3, 9, {ring0, ring1, ring2}));
    for (int i = 0; i < 25; ++i) {
      NET_LOG_FMT_INFO(logger, "c%02d", i);
    }
    logger->clearAppenders();

    net::ApplySinkSet(logger,
                      net::SinkSet::TimeRotate(time_base, net::file_sink::RollInterval::DAY));
    NET_LOG_INFO(logger) << "sync-dated";
    const std::string dated = net::file_sink::DatedPath(
        time_base, net::file_sink::RollInterval::DAY, time(nullptr));
    NET_CHECK(net_test::ReadFile(dated).find("sync-dated") != std::string::npos);
  }

  NET_CHECK(net_test::ReadFile(fixed).find("sync-fixed") != std::string::npos);
  NET_CHECK(net_test::ReadFile(roll).find("s07") != std::string::npos);

  // --- 异步：多线程 + flush ---
  {
    auto logger = net::AsyncLoggerMgr::GetInstance()->getLogger("full_async");
    logger->clearAppenders();
    logger->setFormatter("[%p] %m%n");
    net::ApplySinkSet(logger, net::SinkSet::FixedFile(async_file));

    auto worker = [&](int from, int to) {
      for (int i = from; i < to; ++i) {
        NET_LOG_FMT_INFO(logger, "a%d", i);
      }
    };
    std::thread t1(worker, 0, 50);
    std::thread t2(worker, 50, 100);
    t1.join();
    t2.join();
    net_test::FlushAsyncLogs();

    const std::string c = net_test::ReadFile(async_file);
    NET_CHECK(c.find("a0") != std::string::npos);
    NET_CHECK(c.find("a99") != std::string::npos);
  }

  // --- 异步：按 event 时间切日 ---
  {
    const std::string base = TmpPath("_async_time");
    const time_t d1 = MakeTime(2025, 6, 1);
    const time_t d2 = MakeTime(2025, 6, 2);
    auto logger =
        net::AsyncLoggerMgr::GetInstance()->getLogger("full_async_time");
    logger->clearAppenders();
    logger->setFormatter("%m");
    logger->addAppender(net::LogAppender::ptr(
        new net::TimeRotateFileLogAppender(base, net::file_sink::RollInterval::DAY)));

    logger->log(net::LogLevel::INFO, MakeEvent(logger, net::LogLevel::INFO, d1, "d1"));
    logger->log(net::LogLevel::INFO, MakeEvent(logger, net::LogLevel::INFO, d2, "d2"));
    net_test::FlushAsyncLogs();

    const std::string p1 =
        net::file_sink::DatedPath(base, net::file_sink::RollInterval::DAY, d1);
    const std::string p2 =
        net::file_sink::DatedPath(base, net::file_sink::RollInterval::DAY, d2);
    NET_CHECK(net_test::ReadFile(p1).find("d1") != std::string::npos);
    NET_CHECK(net_test::ReadFile(p1).find("d2") == std::string::npos);
    NET_CHECK(net_test::ReadFile(p2).find("d2") != std::string::npos);
  }

  // --- MakeAppender / SinkKind 别名 ---
  {
    net::SinkSpec spec;
    spec.kind = net::SinkKind::File;
    NET_CHECK(net::MakeAppender(spec) != nullptr);
    spec.kind = net::SinkKind::RollingFile;
    spec.path = fixed;
    NET_CHECK(net::MakeAppender(spec) != nullptr);
  }

  std::printf("PASS test_log_full\n");
  return 0;
}
