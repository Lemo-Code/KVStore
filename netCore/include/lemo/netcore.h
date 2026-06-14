#pragma once

/**
 * @file netcore.h
 * @brief netCore 总入口：utils/thread/log/config + 协程网络栈。
 *
 *   #include "lemo/netcore.h"
 *   lemo::Init("lemo.conf");
 *   lemo::io::IOManager iom(4, true, "main");
 */

#include "lemo/config/config.h"
#include "lemo/fiber/module.h"
#include "lemo/io/module.h"
#include "lemo/buffer/module.h"
#include "lemo/socket/module.h"
#include "lemo/log/module.h"
#include "lemo/thread/module.h"

#include <string>

namespace lemo {

inline bool Init(const std::string& conf_path) {
  return config::LoadLogConfigFile(conf_path);
}

inline bool InitVerbose(const std::string& conf_path, FILE* out = stdout) {
  return config::LoadLogConfigFileVerbose(conf_path, out);
}

}  // namespace lemo
