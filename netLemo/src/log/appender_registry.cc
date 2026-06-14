#include "lemo/log/appender_registry.h"

#include "lemo/log/async_appender.h"
#include "lemo/log/console_appender.h"
#include "lemo/log/file_appender.h"
#include "lemo/log/log_paths.h"
#include "lemo/log/rolling_file_appender.h"

#include <cstdlib>
#include <sstream>

namespace lemo {
namespace log {

std::string AppenderConfig::Get(const std::string& key,
                                const std::string& default_value) const {
  std::map<std::string, std::string>::const_iterator it = properties.find(key);
  if (it == properties.end()) return default_value;
  return it->second;
}

int AppenderConfig::GetInt(const std::string& key, int default_value) const {
  const std::string v = Get(key, "");
  if (v.empty()) return default_value;
  return std::atoi(v.c_str());
}

uint64_t AppenderConfig::GetUint64(const std::string& key,
                                   uint64_t default_value) const {
  const std::string v = Get(key, "");
  if (v.empty()) return default_value;
  return static_cast<uint64_t>(std::strtoull(v.c_str(), NULL, 10));
}

AppenderRegistry::AppenderRegistry() {
  Register("console", [](const AppenderConfig& cfg) {
    const std::string target = cfg.Get("target", "stdout");
    ConsoleTarget t = ConsoleTarget::kStdout;
    if (target == "stderr") t = ConsoleTarget::kStderr;
    return Appender::ptr(new ConsoleAppender(t));
  });

  Register("file", [](const AppenderConfig& cfg) {
    return Appender::ptr(new FileAppender(ResolveAppenderFilePath(cfg)));
  });

  Register("rolling_file", [](const AppenderConfig& cfg) {
    RollInterval interval = RollInterval::kDay;
    const std::string iv = cfg.Get("roll_interval", "day");
    if (iv == "none") interval = RollInterval::kNone;
    if (iv == "hour") interval = RollInterval::kHour;
    return Appender::ptr(new RollingFileAppender(
        ResolveAppenderFilePath(cfg),
        cfg.GetUint64("max_bytes", 100 * 1024 * 1024),
        static_cast<uint32_t>(cfg.GetInt("max_files", 10)), interval));
  });

  Register("async", [](const AppenderConfig& cfg) {
    const std::string inner = cfg.Get("delegate", "console");
    AppenderConfig inner_cfg = cfg;
    inner_cfg.properties.erase("delegate");
    Appender::ptr delegate = AppenderRegistry::Instance().Create(inner, inner_cfg);
    if (!delegate) return Appender::ptr();
    return MakeAsync(delegate);
  });
}

AppenderRegistry& AppenderRegistry::Instance() {
  static AppenderRegistry reg;
  return reg;
}

void AppenderRegistry::Register(const std::string& type, AppenderFactory factory) {
  factories_[type] = factory;
}

Appender::ptr AppenderRegistry::Create(const std::string& type,
                                       const AppenderConfig& config) const {
  std::map<std::string, AppenderFactory>::const_iterator it = factories_.find(type);
  if (it == factories_.end()) return Appender::ptr();
  return it->second(config);
}

bool AppenderRegistry::Has(const std::string& type) const {
  return factories_.find(type) != factories_.end();
}

void RegisterBuiltInAppenders() {
  (void)AppenderRegistry::Instance();
}

}  // namespace log
}  // namespace lemo
