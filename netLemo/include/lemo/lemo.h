#pragma once

/**
 * @file lemo.h
 * @brief lemo 总入口：配置 + 日志 + 线程 + 协程。
 *
 * 典型用法：
 *   #include "lemo/lemo.h"
 *   lemo::Init("lemo.conf");
 *   LEMO_LOG_INFO(LEMO_LOG_ROOT()) << "started";
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
