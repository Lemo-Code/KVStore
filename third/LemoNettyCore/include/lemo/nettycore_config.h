#pragma once

/**
 * @file nettycore_config.h
 * @brief LemoNettyCore 伴生库入口：config + log + 网络栈配置。
 *
 *   #include "lemo/nettycore_config.h"
 *   lemo::Init("nettycore.yaml");
 *   auto rt = lemo::config::CreateRuntimeFromConfig();
 *   lemo::server::TcpServer server(lemo::config::GetNettySettings().server_name, rt.get());
 */

#include "lemo/config/config.h"
#include "lemo/config/netty_config.h"
#include "lemo/log/module.h"

#include <string>

namespace lemo {

inline bool Init(const std::string& conf_path) {
  return config::LoadNettyConfigFile(conf_path);
}

inline bool InitVerbose(const std::string& conf_path, FILE* out = stdout) {
  return config::LoadNettyConfigFileVerbose(conf_path, out);
}

}  // namespace lemo
