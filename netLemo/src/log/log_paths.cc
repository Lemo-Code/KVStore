#include "lemo/log/log_paths.h"

namespace lemo {
namespace log {

std::string SanitizeLoggerNameForPath(const std::string& logger_name) {
  if (logger_name.empty()) return "unknown";
  std::string out;
  out.reserve(logger_name.size());
  for (size_t i = 0; i < logger_name.size(); ++i) {
    const char c = logger_name[i];
    if (c == '/' || c == '\\') {
      out.push_back('_');
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::string ResolveLoggerFilePath(const std::string& logger_name,
                                  const std::string& base_path) {
  const std::string safe = SanitizeLoggerNameForPath(logger_name);
  if (base_path.empty()) {
    return safe + "_lemo.log";
  }
  const size_t slash = base_path.rfind('/');
  if (slash == std::string::npos) {
    return safe + "_" + base_path;
  }
  return base_path.substr(0, slash + 1) + safe + "_" +
         base_path.substr(slash + 1);
}

std::string ResolveAppenderFilePath(const AppenderConfig& cfg) {
  return ResolveLoggerFilePath(cfg.Get("logger_name", "root"),
                               cfg.Get("path", "lemo.log"));
}

}  // namespace log
}  // namespace lemo
