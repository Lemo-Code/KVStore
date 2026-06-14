#include "test_common.h"

#include "lemo/lemo.h"
#include "lemo/log/log_paths.h"

#include <atomic>
#include <fstream>
#include <string>

namespace {

std::string ReadAll(const std::string& path) {
  std::ifstream in(path.c_str());
  LEMO_CHECK(in.good());
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

void log_from_worker(const std::string& tag, lemo::log::Logger::ptr logger) {
  lemo::thread::Thread::ptr worker(new lemo::thread::Thread(
      [logger, tag]() { LEMO_LOG_INFO(logger) << tag; }, "biz_worker"));
  worker->join();
}

}  // namespace

#ifndef LEMO_INTEGRATION_CONF
#define LEMO_INTEGRATION_CONF \
  "tests/lemo/config/fixtures/lemo_integration.conf"
#endif

int main() {
  const std::string conf_path = LEMO_INTEGRATION_CONF;
  const std::string base_log_path = "/tmp/lemo_config_log_integration.log";
  const std::string log_path =
      lemo::log::ResolveLoggerFilePath("root", base_log_path);
  std::remove(log_path.c_str());

  lemo::config::ConfigCenter::Clear();

  LEMO_CHECK(lemo::Init(conf_path));
  LEMO_CHECK(lemo::utils::GetThreadName() == "main");

  lemo::log::Logger::ptr root = LEMO_LOG_ROOT();
  lemo::log::Logger::ptr biz = LEMO_LOG_NAME("biz");

  const uint32_t main_tid = lemo::log::GetThreadId();
  LEMO_LOG_INFO(root) << "from_main";
  log_from_worker("from_worker", biz);

  root->Flush();

  const std::string content = ReadAll(log_path);
  LEMO_CHECK(content.find("from_main") != std::string::npos);
  LEMO_CHECK(content.find("from_worker") != std::string::npos);
  LEMO_CHECK(content.find("main") != std::string::npos);
  LEMO_CHECK(content.find("biz_worker") != std::string::npos);
  LEMO_CHECK(content.find("[" + std::to_string(main_tid) + "]") !=
             std::string::npos);
  LEMO_CHECK(content.find("[config] loaded from") != std::string::npos);

  std::remove(log_path.c_str());
  lemo::config::ConfigCenter::Clear();
  std::printf("PASS test_config_log_integration (conf=%s)\n", conf_path.c_str());
  return 0;
}
