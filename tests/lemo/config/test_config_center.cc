#include "test_common.h"

#include "lemo/config/config_center.h"

int main() {
  lemo::config::ConfigCenter::Clear();
  lemo::config::ConfigVar<std::string>::ptr port =
      lemo::config::ConfigCenter::Lookup<std::string>("server.port", "8080");
  LEMO_CHECK(port.get() != NULL);
  LEMO_CHECK(port->GetValue() == "8080");

  const std::string conf = "server.port = 9090\nunknown.key = 1\n";
  LEMO_CHECK(lemo::config::ConfigCenter::LoadFromString(conf));
  LEMO_CHECK(port->GetValue() == "9090");
  LEMO_CHECK(lemo::config::ConfigCenter::Lookup<std::string>("unknown.key").get() ==
             NULL);

  lemo::config::ConfigCenter::Clear();
  std::printf("PASS test_config_center\n");
  return 0;
}
