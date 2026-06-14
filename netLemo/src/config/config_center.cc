#include "lemo/config/config_center.h"

#include "lemo/config/property_loader.h"

namespace lemo {
namespace config {

ConfigCenter& ConfigCenter::Instance() {
  static ConfigCenter center;
  return center;
}

std::string ConfigCenter::NormalizeKey(const std::string& name) {
  return utils::ToLower(utils::Trim(name));
}

bool ConfigCenter::IsValidKey(const std::string& key) {
  if (key.empty()) return false;
  for (size_t i = 0; i < key.size(); ++i) {
    const char c = key[i];
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
        c == '_') {
      continue;
    }
    return false;
  }
  return true;
}

ConfigVarBase::ptr ConfigCenter::LookupBase(const std::string& name) {
  const std::string key = NormalizeKey(name);
  std::lock_guard<std::mutex> lock(Instance().mutex_);
  std::map<std::string, ConfigVarBase::ptr>::iterator it =
      Instance().vars_.find(key);
  if (it == Instance().vars_.end()) return ConfigVarBase::ptr();
  return it->second;
}

bool ConfigCenter::ApplyFlatMap(const std::map<std::string, std::string>& kv,
                                LoadStats* stats) {
  bool ok = true;
  for (std::map<std::string, std::string>::const_iterator it = kv.begin();
       it != kv.end(); ++it) {
    ConfigVarBase::ptr var = LookupBase(it->first);
    if (!var) {
      if (stats) {
        ++stats->ignored_keys;
        stats->ignored.push_back(it->first);
      }
      continue;
    }
    if (!var->FromString(it->second)) {
      ok = false;
      if (stats) stats->failed.push_back(it->first);
    } else if (stats) {
      ++stats->applied_keys;
    }
  }
  return ok;
}

bool ConfigCenter::LoadFromString(const std::string& content, LoadStats* stats) {
  std::map<std::string, std::string> kv;
  if (!PropertyLoader::LoadString(content, &kv)) return false;
  if (stats) stats->parsed_keys = kv.size();
  return Instance().ApplyFlatMap(kv, stats);
}

bool ConfigCenter::LoadFromString(const std::string& content) {
  return LoadFromString(content, NULL);
}

bool ConfigCenter::LoadFromFile(const std::string& path, LoadStats* stats) {
  std::map<std::string, std::string> kv;
  if (!PropertyLoader::LoadFile(path, &kv)) return false;
  if (stats) stats->parsed_keys = kv.size();
  return Instance().ApplyFlatMap(kv, stats);
}

bool ConfigCenter::LoadFromFile(const std::string& path) {
  return LoadFromFile(path, NULL);
}

bool ConfigCenter::LoadFlatMap(const std::map<std::string, std::string>& kv,
                               LoadStats* stats) {
  if (stats) stats->parsed_keys = kv.size();
  return Instance().ApplyFlatMap(kv, stats);
}

bool ConfigCenter::LoadFlatMap(const std::map<std::string, std::string>& kv) {
  return Instance().ApplyFlatMap(kv, NULL);
}

void ConfigCenter::Visit(std::function<void(ConfigVarBase::ptr)> cb) {
  if (!cb) return;
  std::lock_guard<std::mutex> lock(Instance().mutex_);
  for (std::map<std::string, ConfigVarBase::ptr>::iterator it =
           Instance().vars_.begin();
       it != Instance().vars_.end(); ++it) {
    cb(it->second);
  }
}

void ConfigCenter::Clear() {
  std::lock_guard<std::mutex> lock(Instance().mutex_);
  Instance().vars_.clear();
}

}  // namespace config
}  // namespace lemo
