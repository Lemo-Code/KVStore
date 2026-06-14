#include "test_common.h"

#include "lemo/config/config_center.h"
#include "lemo/config/log_config.h"
#include "lemo/config/property_loader.h"
#include "lemo/log/log.h"
#include "lemo/log/level.h"
#include "lemo/log/log_paths.h"

#include <fstream>
#include <map>
#include <string>

#ifndef LEMO_TEST_CONF
#define LEMO_TEST_CONF "tests/lemo/config/fixtures/lemo_test.conf"
#endif

namespace {

std::string ReadAll(const std::string& path) {
  std::ifstream in(path.c_str());
  LEMO_CHECK(in.good());
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

void CheckStringVar(const std::string& key, const std::string& expected) {
  lemo::config::ConfigVar<std::string>::ptr var =
      lemo::config::ConfigCenter::Lookup<std::string>(key);
  LEMO_CHECK(var.get() != NULL);
  LEMO_CHECK(var->GetValue() == expected);
}

}  // namespace

int main() {
  lemo::config::ConfigCenter::Clear();

  const std::string conf_path = LEMO_TEST_CONF;
  const std::string base_log_path = "/tmp/lemo_config_full_test.log";
  const std::string log_path =
      lemo::log::ResolveLoggerFilePath("root", base_log_path);
  std::remove(log_path.c_str());

  // 1) PropertyLoader 单独解析
  std::map<std::string, std::string> kv;
  LEMO_CHECK(lemo::config::PropertyLoader::LoadFile(conf_path, &kv));
  LEMO_CHECK(kv["log.level"] == "WARN");
  LEMO_CHECK(kv["log.file.path"] == base_log_path);
  LEMO_CHECK(kv["log.logger.com.example.level"] == "DEBUG");
  LEMO_CHECK(kv.find("ignored.key") != kv.end());

  // 2) ConfigCenter：先声明 server.*，再加载文件
  lemo::config::ConfigCenter::Lookup<std::string>("server.port", "8080");
  lemo::config::ConfigCenter::Lookup<std::string>("server.name", "default");
  LEMO_CHECK(lemo::config::ConfigCenter::LoadFromFile(conf_path));
  CheckStringVar("server.port", "9090");
  CheckStringVar("server.name", "kvstore");
  LEMO_CHECK(lemo::config::ConfigCenter::Lookup<std::string>("ignored.key").get() ==
             NULL);

  // 3) 日志配置：整文件加载 + 装配（会打印摘要并写入日志）
  lemo::config::ConfigCenter::Clear();
  std::printf("\n--- LoadLogConfigFile(%s) ---\n", conf_path.c_str());
  LEMO_CHECK(lemo::config::LoadLogConfigFile(conf_path));

  CheckStringVar("log.level", "WARN");
  CheckStringVar("log.pattern", "%m");
  CheckStringVar("log.appender", "file");
  CheckStringVar("log.file.path", base_log_path);
  CheckStringVar("log.file.max_bytes", "1048576");
  CheckStringVar("log.file.max_files", "5");
  CheckStringVar("log.file.roll_interval", "none");
  CheckStringVar("log.logger.com.example.level", "DEBUG");
  CheckStringVar("log.logger.com.example.additive", "false");
  CheckStringVar("log.logger.net.io.level", "ERROR");
  CheckStringVar("log.logger.net.io.additive", "true");

  lemo::log::Logger::ptr root = LEMO_LOG_ROOT();
  LEMO_CHECK(root->GetLevel() == lemo::log::LogLevel::WARN);

  lemo::log::Logger::ptr example = LEMO_LOG_NAME("com.example");
  LEMO_CHECK(example->GetLevel() == lemo::log::LogLevel::DEBUG);
  LEMO_CHECK(example->IsAdditive() == false);

  lemo::log::Logger::ptr net_io = LEMO_LOG_NAME("net.io");
  LEMO_CHECK(net_io->GetLevel() == lemo::log::LogLevel::ERROR);
  LEMO_CHECK(net_io->IsAdditive() == true);

  LEMO_LOG_WARN(root) << "full_conf_msg";
  root->Flush();

  const std::string content = ReadAll(log_path);
  LEMO_CHECK(content.find("full_conf_msg") != std::string::npos);
  LEMO_CHECK(content.find("[config] loaded from") != std::string::npos);
  LEMO_CHECK(content.find("log.logger.com.example.level=DEBUG") !=
             std::string::npos);

  std::remove(log_path.c_str());
  lemo::config::ConfigCenter::Clear();
  std::printf("PASS test_config_full (conf=%s)\n", conf_path.c_str());
  return 0;
}
