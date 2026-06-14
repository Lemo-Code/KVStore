#pragma once

#include "lemo/log/appender.h"

#include <functional>
#include <map>
#include <string>

namespace lemo {
namespace log {

struct AppenderConfig {
  std::map<std::string, std::string> properties;

  std::string Get(const std::string& key,
                  const std::string& default_value = "") const;
  int GetInt(const std::string& key, int default_value) const;
  uint64_t GetUint64(const std::string& key, uint64_t default_value) const;
};

typedef std::function<Appender::ptr(const AppenderConfig&)> AppenderFactory;

// SPI：按 type 名创建 Appender，支持插件式扩展
class AppenderRegistry {
 public:
  static AppenderRegistry& Instance();

  void Register(const std::string& type, AppenderFactory factory);
  Appender::ptr Create(const std::string& type,
                       const AppenderConfig& config) const;
  bool Has(const std::string& type) const;

  AppenderRegistry(const AppenderRegistry&) = delete;
  AppenderRegistry& operator=(const AppenderRegistry&) = delete;

 private:
  AppenderRegistry();
  std::map<std::string, AppenderFactory> factories_;
};

void RegisterBuiltInAppenders();

}  // namespace log
}  // namespace lemo
