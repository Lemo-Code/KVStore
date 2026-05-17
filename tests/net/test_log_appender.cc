#include "test_common.h"

#include "log.h"

#include <fstream>
#include <string>

namespace {

std::string ReadFile(const std::string& path) {
  std::ifstream ifs(path.c_str());
  return std::string((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  const std::string fixed = "/tmp/net_sink_fixed.log";
  const std::string para_a = "/tmp/net_sink_para_a.log";
  const std::string para_b = "/tmp/net_sink_para_b.log";
  const std::string ring0 = "/tmp/net_sink_ring.0";
  const std::string ring1 = "/tmp/net_sink_ring.1";
  const std::string ring2 = "/tmp/net_sink_ring.2";
  const std::string time_base = "/tmp/net_sink_time";

  auto logger = net::LoggerMgr::GetInstance()->getLogger("sink_schemes");
  logger->setFormatter("%m");

  // 1) 固定单文件
  net::ApplySinkSet(logger, net::SinkSet::FixedFile(fixed));
  NET_LOG_INFO(logger) << "fixed-only";
  logger->clearAppenders();

  // 2) 并行 N 文件
  net::ApplySinkSet(logger, net::SinkSet::MultiFile({para_a, para_b}));
  NET_LOG_INFO(logger) << "parallel";
  logger->clearAppenders();

  // 3) 环形 3 槽，每槽 48 字节，写满后覆盖
  auto ring_logger = net::LoggerMgr::GetInstance()->getLogger("sink_ring");
  ring_logger->setFormatter("%m");
  // 每槽 9 字节（约 3 条 "rXX"），写 40 条会绕满 3 槽并回到 0 覆盖
  net::ApplySinkSet(
      ring_logger,
      net::SinkSet::CircularRing("/tmp/net_sink_ring", 3, 9,
                                 {ring0, ring1, ring2}));
  for (int i = 0; i < 40; ++i) {
    NET_LOG_FMT_INFO(ring_logger, "r%02d", i);
  }
  NET_CHECK(ReadFile(ring0).find("r") != std::string::npos);
  const std::string r0 = ReadFile(ring0);
  NET_CHECK(r0.find("r00") == std::string::npos);
  NET_CHECK(r0.find("r01") == std::string::npos);

  // 4) 按时间切文件（此处只验证能写出当前 dated 文件）
  auto time_logger = net::LoggerMgr::GetInstance()->getLogger("sink_time");
  time_logger->setFormatter("%m%n");
  net::ApplySinkSet(time_logger,
                    net::SinkSet::TimeRotate(time_base, net::file_sink::RollInterval::DAY));
  NET_LOG_INFO(time_logger) << "dated";
  const std::string dated_suffix =
      net::file_sink::DatedPath(time_base, net::file_sink::RollInterval::DAY,
                                time(nullptr));
  NET_CHECK(ReadFile(dated_suffix).find("dated") != std::string::npos);

  NET_CHECK(ReadFile(fixed).find("fixed-only") != std::string::npos);
  NET_CHECK(ReadFile(para_a).find("parallel") != std::string::npos);

  std::printf("PASS test_log_appender\n");
  return 0;
}
