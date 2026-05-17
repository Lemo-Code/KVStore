#include "test_common.h"

#include "log.h"

#include <cstdio>
#include <fstream>
#include <string>

int main() {
  const std::string log_path = "/tmp/net_log_sync_test.log";

  auto logger = net::LoggerMgr::GetInstance()->getLogger("sync_test");
  logger->clearAppenders();
  logger->setLevel(net::LogLevel::DEBUG);
  logger->setFormatter("[%p] %m%n");

  logger->addAppender(
      net::LogAppender::ptr(new net::StdoutLogAppender("sync_stdout")));
  logger->addAppender(
      net::LogAppender::ptr(new net::FileLogAppender(log_path)));

  NET_LOG_INFO(logger) << "sync message";
  NET_LOG_FMT_WARN(logger, "code=%d", 42);

  {
    std::ifstream ifs(log_path.c_str());
    NET_CHECK(ifs.good());
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    NET_CHECK(content.find("[INFO]") != std::string::npos);
    NET_CHECK(content.find("sync message") != std::string::npos);
    NET_CHECK(content.find("[WARN]") != std::string::npos);
    NET_CHECK(content.find("code=42") != std::string::npos);
  }

  std::printf("PASS test_log_sync\n");
  return 0;
}
