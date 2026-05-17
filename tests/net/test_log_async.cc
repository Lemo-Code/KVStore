#include "test_common.h"

#include "log.h"

#include <chrono>
#include <fstream>
#include <string>
#include <thread>

int main() {
  const std::string log_path = "/tmp/net_log_async_test.log";

  auto logger = net::AsyncLoggerMgr::GetInstance()->getLogger("async_test");
  logger->clearAppenders();
  logger->setLevel(net::LogLevel::DEBUG);
  logger->setFormatter("[%p] %m%n");
  logger->addAppender(
      net::LogAppender::ptr(new net::FileLogAppender(log_path)));

  auto worker = [&](int begin, int end) {
    for (int i = begin; i < end; ++i) {
      NET_LOG_FMT_INFO(logger, "async-%d", i);
    }
  };

  std::thread t1(worker, 0, 50);
  std::thread t2(worker, 50, 100);
  t1.join();
  t2.join();

  // 等待后台刷盘线程写出
  std::this_thread::sleep_for(std::chrono::milliseconds(
      NET_LOG_ASYNC_FLUSH_MS + 200));

  {
    std::ifstream ifs(log_path.c_str());
    NET_CHECK(ifs.good());
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    NET_CHECK(content.find("[INFO]") != std::string::npos);
    NET_CHECK(content.find("async-0") != std::string::npos);
    NET_CHECK(content.find("async-99") != std::string::npos);
  }

  std::printf("PASS test_log_async\n");
  return 0;
}
