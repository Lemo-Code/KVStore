#pragma once

#include "lemo/log/appender_registry.h"

#include <string>

namespace lemo {
namespace log {

/** 将 logger 名中不适合做文件名的字符替换为 '_'。 */
std::string SanitizeLoggerNameForPath(const std::string& logger_name);

/**
 * 按 logger 名给文件路径加前缀，避免多个 logger 写同一 base path。
 * 例：root + /tmp/app.log -> /tmp/root_app.log
 */
std::string ResolveLoggerFilePath(const std::string& logger_name,
                                  const std::string& base_path);

/** 从 AppenderConfig 的 logger_name + path 解析最终落盘路径。 */
std::string ResolveAppenderFilePath(const AppenderConfig& cfg);

}  // namespace log
}  // namespace lemo
