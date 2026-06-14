#include "test_common.h"

#include "lemo/config/config_center.h"
#include "lemo/config/netty_config.h"
#include "lemo/config/property_loader.h"
#include "lemo/config/yaml_loader.h"
#include "lemo/io/runtime.h"
#include "lemo/log/log.h"
#include "lemo/log/level.h"
#include "lemo/memory/stack_pool.h"

#include <map>
#include <string>

#ifndef NETTYCORE_TEST_CONF
#define NETTYCORE_TEST_CONF "tests/lemo/config/fixtures/nettycore_test.yaml"
#endif

namespace {

void CheckStringVar(const std::string& key, const std::string& expected) {
  lemo::config::ConfigVar<std::string>::ptr var =
      lemo::config::ConfigCenter::Lookup<std::string>(key);
  LEMO_CHECK(var.get() != nullptr);
  LEMO_CHECK(var->GetValue() == expected);
}

}  // namespace

int main() {
  lemo::config::ConfigCenter::Clear();

  const std::string conf_path = NETTYCORE_TEST_CONF;

  std::map<std::string, std::string> kv;
  LEMO_CHECK(lemo::config::YamlLoader::LoadFile(conf_path, &kv));
  LEMO_CHECK(kv["server.port"] == "9000");
  LEMO_CHECK(kv["io.threads"] == "4");
  LEMO_CHECK(kv["log.level"] == "INFO");

  LEMO_CHECK(lemo::config::LoadNettyConfigFile(conf_path));

  CheckStringVar("server.name", "echo");
  CheckStringVar("server.host", "0.0.0.0");
  CheckStringVar("server.port", "9000");
  CheckStringVar("io.threads", "4");
  CheckStringVar("io.use_caller", "false");
  CheckStringVar("io.name", "main");
  CheckStringVar("log.level", "INFO");
  CheckStringVar("log.logger.net.io.level", "DEBUG");

  const lemo::config::NettySettings settings =
      lemo::config::GetNettySettings();
  LEMO_CHECK(settings.server_name == "echo");
  LEMO_CHECK(settings.server_host == "0.0.0.0");
  LEMO_CHECK(settings.server_port == 9000);
  LEMO_CHECK(settings.io_threads == 4);
  LEMO_CHECK(settings.io_use_caller == false);
  LEMO_CHECK(settings.io_name == "main");
  LEMO_CHECK(settings.stackpool_max_tls_cached == 32);
  LEMO_CHECK(settings.stackpool_max_global_cached == 64);

  const lemo::memory::StackPool::Config pool_cfg = lemo::memory::StackPool::config();
  LEMO_CHECK(pool_cfg.max_tls_cached == 32);
  LEMO_CHECK(pool_cfg.max_global_cached == 64);

  lemo::io::Runtime::ptr rt = lemo::config::CreateRuntimeFromConfig();
  LEMO_CHECK(rt.get() != nullptr);
  rt->stop();

  lemo::log::Logger::ptr net_io = LEMO_LOG_NAME("net.io");
  LEMO_CHECK(net_io->GetLevel() == lemo::log::LogLevel::DEBUG);

  lemo::config::ConfigCenter::Clear();
  std::printf("PASS test_netty_config (conf=%s)\n", conf_path.c_str());
  return 0;
}
