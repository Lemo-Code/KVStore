#pragma once

#include "lemo/config/config_var.h"
#include "lemo/config/load_stats.h"

#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace lemo {
namespace config {

// 约定优于配置：仅 Lookup 声明过的 key 可被加载覆盖
class ConfigCenter {
 public:
  static ConfigCenter& Instance();

  template <typename T>
  static typename ConfigVar<T>::ptr Lookup(const std::string& name,
                                           const T& default_val,
                                           const std::string& description = "") {
    return Instance().LookupImpl<T>(name, default_val, description);
  }

  template <typename T>
  static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
    return Instance().LookupOnly<T>(name);
  }

  static ConfigVarBase::ptr LookupBase(const std::string& name);
  static bool LoadFromFile(const std::string& path);
  static bool LoadFromFile(const std::string& path, LoadStats* stats);
  static bool LoadFromString(const std::string& content);
  static bool LoadFromString(const std::string& content, LoadStats* stats);
  static bool LoadFlatMap(const std::map<std::string, std::string>& kv);
  static bool LoadFlatMap(const std::map<std::string, std::string>& kv,
                          LoadStats* stats);
  static void Visit(std::function<void(ConfigVarBase::ptr)> cb);
  static void Clear();

  ConfigCenter(const ConfigCenter&) = delete;
  ConfigCenter& operator=(const ConfigCenter&) = delete;

 private:
  ConfigCenter() {}

  template <typename T>
  typename ConfigVar<T>::ptr LookupImpl(const std::string& name,
                                        const T& default_val,
                                        const std::string& description) {
    const std::string key = NormalizeKey(name);
    if (!IsValidKey(key)) return typename ConfigVar<T>::ptr();

    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, ConfigVarBase::ptr>::iterator it = vars_.find(key);
    if (it != vars_.end()) {
      return std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
    }
    typename ConfigVar<T>::ptr var(new ConfigVar<T>(key, default_val, description));
    vars_[key] = var;
    return var;
  }

  template <typename T>
  typename ConfigVar<T>::ptr LookupOnly(const std::string& name) {
    const std::string key = NormalizeKey(name);
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, ConfigVarBase::ptr>::iterator it = vars_.find(key);
    if (it == vars_.end()) return typename ConfigVar<T>::ptr();
    return std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
  }

  static std::string NormalizeKey(const std::string& name);
  static bool IsValidKey(const std::string& key);
  bool ApplyFlatMap(const std::map<std::string, std::string>& kv,
                    LoadStats* stats);

  std::map<std::string, ConfigVarBase::ptr> vars_;
  std::mutex mutex_;
};

}  // namespace config
}  // namespace lemo
