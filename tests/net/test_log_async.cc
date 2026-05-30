#include "test_common.h"

#include "log/log.h"

#include <fstream>
#include <string>
#include <vector>

int main() {
  const std::string log_path = net_test::LogPath("async_test.log");

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

  std::vector<net::Thread::ptr> threads;
  threads.push_back(net::Thread::ptr(
      new net::Thread(std::bind(worker, 0, 50), "async_w0")));
  threads.push_back(net::Thread::ptr(
      new net::Thread(std::bind(worker, 50, 100), "async_w1")));
  for (auto& t : threads) {
    t->join();
  }

  net_test::FlushAsyncLogs();

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
