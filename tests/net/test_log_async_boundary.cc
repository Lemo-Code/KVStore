#include "test_common.h"

#include "log.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

namespace {

std::string ReadFile(const std::string& path) {
  std::ifstream ifs(path.c_str());
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
}

time_t MakeLocalTime(int year, int mon, int mday, int hour = 12) {
  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = mon - 1;
  t.tm_mday = mday;
  t.tm_hour = hour;
  t.tm_isdst = -1;
  return mktime(&t);
}

net::LogEvent::ptr MakeEvent(const net::Logger::ptr& logger,
                             net::LogLevel::Level level, time_t when,
                             const std::string& msg) {
  auto event = std::make_shared<net::LogEvent>(
      logger, level, __FILE__, __LINE__, 0, 0, 0,
      static_cast<uint64_t>(when), "test");
  event->getSS() << msg;
  return event;
}

void WaitAsyncFlush() {
  std::this_thread::sleep_for(
      std::chrono::milliseconds(NET_LOG_ASYNC_FLUSH_MS + 300));
}

}  // namespace

int main() {
  // --- 按 event 时间切日：异步不得把 2 号写进 1 号文件 ---
  {
    const std::string base = "/tmp/net_async_boundary_time";
    const time_t day1 = MakeLocalTime(2025, 5, 1);
    const time_t day2 = MakeLocalTime(2025, 5, 2);

    auto logger =
        net::AsyncLoggerMgr::GetInstance()->getLogger("async_boundary_time");
    logger->clearAppenders();
    logger->setFormatter("%m%n");
    logger->addAppender(net::LogAppender::ptr(
        new net::TimeRotateFileLogAppender(base, net::file_sink::RollInterval::DAY)));

    logger->log(net::LogLevel::INFO, MakeEvent(logger, net::LogLevel::INFO, day1, "on-day1"));
    logger->log(net::LogLevel::INFO, MakeEvent(logger, net::LogLevel::INFO, day2, "on-day2"));
    WaitAsyncFlush();

    const std::string path1 =
        net::file_sink::DatedPath(base, net::file_sink::RollInterval::DAY, day1);
    const std::string path2 =
        net::file_sink::DatedPath(base, net::file_sink::RollInterval::DAY, day2);

    const std::string f1 = ReadFile(path1);
    const std::string f2 = ReadFile(path2);
    NET_CHECK(f1.find("on-day1") != std::string::npos);
    NET_CHECK(f1.find("on-day2") == std::string::npos);
    NET_CHECK(f2.find("on-day2") != std::string::npos);
    NET_CHECK(f2.find("on-day1") == std::string::npos);
  }

  // --- 环形槽：入队时已换槽，延迟刷盘仍写入对应槽文件 ---
  {
    const std::string ring0 = "/tmp/net_async_boundary_ring.0";
    const std::string ring1 = "/tmp/net_async_boundary_ring.1";
    const std::string ring2 = "/tmp/net_async_boundary_ring.2";

    auto logger =
        net::AsyncLoggerMgr::GetInstance()->getLogger("async_boundary_ring");
    logger->clearAppenders();
    logger->setFormatter("%m");
    logger->addAppender(net::LogAppender::ptr(new net::CircularFileLogAppender(
        "/tmp/net_async_boundary_ring", 3, 9, {ring0, ring1, ring2})));

    for (int i = 0; i < 40; ++i) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "r%02d", i);
      logger->log(net::LogLevel::INFO,
                  MakeEvent(logger, net::LogLevel::INFO, time(nullptr), buf));
    }
    WaitAsyncFlush();

    const std::string r0 = ReadFile(ring0);
    NET_CHECK(r0.find("r") != std::string::npos);
    NET_CHECK(r0.find("r00") == std::string::npos);
  }

  // --- 大小链式轮转：异步在入队前 roll，旧内容进 .1 ---
  {
    const std::string roll_path = "/tmp/net_async_boundary_roll.log";
    auto logger =
        net::AsyncLoggerMgr::GetInstance()->getLogger("async_boundary_roll");
    logger->clearAppenders();
    logger->setFormatter("%m");
    logger->addAppender(net::LogAppender::ptr(new net::RollingFileLogAppender(
        roll_path, 12, 3, net::file_sink::RollInterval::NONE)));

    for (int i = 0; i < 10; ++i) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "m%02d", i);
      logger->log(net::LogLevel::INFO,
                  MakeEvent(logger, net::LogLevel::INFO, time(nullptr), buf));
    }
    WaitAsyncFlush();

    const std::string active = ReadFile(roll_path);
    const std::string rolled = ReadFile(roll_path + ".1") + ReadFile(roll_path + ".2") +
                               ReadFile(roll_path + ".3");
    NET_CHECK(active.find("m09") != std::string::npos);
    NET_CHECK(active.find("m00") == std::string::npos);
    NET_CHECK(rolled.find("m00") != std::string::npos);
  }

  std::printf("PASS test_log_async_boundary\n");
  return 0;
}
