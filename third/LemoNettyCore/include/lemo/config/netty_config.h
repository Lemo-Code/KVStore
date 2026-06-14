#pragma once

#include "lemo/nettycore_export.h"

#include <cstdio>
#include <cstdint>
#include <memory>
#include <string>

namespace lemo {
namespace io {
class Runtime;
}
namespace config {

/** 从 YAML 读取的网络栈运行参数（需先 InitNettyConfigVars / Load）。 */
struct LEMO_NETTYCORE_API NettySettings {
  std::string server_name = "app";
  std::string server_host = "0.0.0.0";
  uint16_t server_port = 9000;

  size_t io_threads = 4;
  bool io_use_caller = false;
  std::string io_name = "main";

  uint32_t stackpool_max_tls_cached = 32;
  uint32_t stackpool_max_global_cached = 64;
};

void LEMO_NETTYCORE_API InitNettyConfigVars();
void LEMO_NETTYCORE_API ApplyNettyConfig();

NettySettings LEMO_NETTYCORE_API GetNettySettings();

std::shared_ptr<io::Runtime> LEMO_NETTYCORE_API CreateRuntimeFromConfig(
    const std::string& name_override = "");

bool LEMO_NETTYCORE_API LoadNettyConfigYamlFile(const std::string& path);
bool LEMO_NETTYCORE_API LoadNettyConfigYamlString(const std::string& content);
bool LEMO_NETTYCORE_API LoadNettyConfigFile(const std::string& path);
bool LEMO_NETTYCORE_API LoadNettyConfigFileVerbose(const std::string& path,
                                                   FILE* out = stdout);

void LEMO_NETTYCORE_API PrintNettySettings(FILE* out = stdout);

}  // namespace config
}  // namespace lemo
