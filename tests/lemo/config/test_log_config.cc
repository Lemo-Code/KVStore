#include "test_common.h"

#include "lemo/config/config_center.h"
#include "lemo/config/log_config.h"
#include "lemo/log/log.h"
#include "lemo/log/level.h"
#include "lemo/log/log_paths.h"

#include <fstream>
#include <string>

int main() {
  lemo::config::ConfigCenter::Clear();

  const std::string base_path = "/tmp/lemo_log_config_test.log";
  const std::string path = lemo::log::ResolveLoggerFilePath("root", base_path);
  std::remove(path.c_str());

  const std::string conf =
      "log.level = WARN\n"
      "log.pattern = %m\n"
      "log.appender = file\n"
      "log.file.path = " +
      base_path +
      "\n"
      "log.logger.com.example.level = DEBUG\n"
      "log.logger.com.example.additive = false\n";

  LEMO_CHECK(lemo::config::LoadLogConfigString(conf));

  lemo::config::ConfigVar<std::string>::ptr additive_cfg =
      lemo::config::ConfigCenter::Lookup<std::string>(
          "log.logger.com.example.additive");
  LEMO_CHECK(additive_cfg.get() != NULL);
  LEMO_CHECK(additive_cfg->GetValue() == "false");

  lemo::log::Logger::ptr root = LEMO_LOG_ROOT();
  LEMO_CHECK(root->GetLevel() == lemo::log::LogLevel::WARN);

  lemo::log::Logger::ptr child = LEMO_LOG_NAME("com.example");
  LEMO_CHECK(child->GetLevel() == lemo::log::LogLevel::DEBUG);
  LEMO_CHECK(child->IsAdditive() == false);

  LEMO_LOG_WARN(root) << "warn_msg";
  root->Flush();

  std::ifstream in(path.c_str());
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  in.close();
  LEMO_CHECK(content.find("warn_msg") != std::string::npos);
  LEMO_CHECK(content.find("[config] loaded from") != std::string::npos);

  std::remove(path.c_str());
  lemo::config::ConfigCenter::Clear();
  std::printf("PASS test_log_config\n");
  return 0;
}
