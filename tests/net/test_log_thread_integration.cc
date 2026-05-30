/**
 * @file test_log_thread_integration.cc
 * @brief 日志与线程模块整合：TLS 线程名/ID 写入 LogEvent，异步 worker 使用 net::Thread。
 */
#include "test_common.h"

#include "log/log.h"
#include "thread/module.h"

#include <atomic>
#include <fstream>
#include <string>
#include <vector>

namespace {

void log_from_named_thread(const std::string& name, const std::string& tag,
                           net::Logger::ptr logger) {
  net::Thread::ptr t(new net::Thread(
      [logger, tag]() { NET_LOG_INFO(logger) << tag; }, name));
  t->join();
}

}  // namespace

int main() {
  const std::string log_path = net_test::LogPath("thread_integration.log");

  net::Thread::SetName("main");

  auto logger = net::AsyncLoggerMgr::GetInstance()->getLogger("thread_int");
  logger->clearAppenders();
  logger->setLevel(net::LogLevel::DEBUG);
  logger->setFormatter("[%t][%N] %m%n");
  logger->addAppender(
      net::LogAppender::ptr(new net::FileLogAppender(log_path)));

  const uint32_t main_tid = net::GetThreadId();
  NET_LOG_INFO(logger) << "from_main";

  log_from_named_thread("biz_worker", "from_worker", logger);

  net_test::FlushAsyncLogs();

  std::ifstream ifs(log_path.c_str());
  NET_CHECK(ifs.good());
  const std::string content((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());

  NET_CHECK(content.find("from_main") != std::string::npos);
  NET_CHECK(content.find("from_worker") != std::string::npos);
  NET_CHECK(content.find("main") != std::string::npos);
  NET_CHECK(content.find("biz_worker") != std::string::npos);

  const std::string main_marker = "[" + std::to_string(main_tid) + "]";
  NET_CHECK(content.find(main_marker) != std::string::npos);

  std::printf("PASS test_log_thread_integration\n");
  return 0;
}
