/**
 * @file config_center.cc
 * @brief 配置中心实现（基于 yaml-cpp）
 */
#include "config/config_center.h"

#include "log/config/log_config_bridge.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <sstream>

namespace net {

namespace {

void listAllMember(const std::string& prefix, const YAML::Node& node,
                   std::vector<std::pair<std::string, std::string>>& output) {
  if (!prefix.empty() &&
      prefix.find_first_not_of(
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_.0123456789") !=
          std::string::npos) {
    return;
  }

  if (node.IsScalar()) {
    output.push_back({prefix, node.Scalar()});
    return;
  }

  if (node.IsSequence()) {
    YAML::Emitter emitter;
    emitter << node;
    output.push_back({prefix, emitter.c_str()});
    return;
  }

  if (node.IsMap()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
      std::string key = it->first.as<std::string>();
      std::string new_prefix = prefix.empty() ? key : prefix + "." + key;
      listAllMember(new_prefix, it->second, output);
    }
  }
}

bool applyFlattenedYaml(const std::vector<std::pair<std::string, std::string>>& flattened) {
  for (const auto& kv : flattened) {
    if (kv.first.empty()) {
      continue;
    }

    std::string key = kv.first;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    ConfigVarBase::ptr var = ConfigCenter::lookupBase(key);
    if (var) {
      var->fromString(kv.second);
    }
  }
  return true;
}

}  // namespace

ConfigCenter::ConfigVarMap& ConfigCenter::getDatas() {
  static ConfigVarMap datas;
  return datas;
}

ConfigCenter::MutexType& ConfigCenter::getMutex() {
  static MutexType mutex;
  return mutex;
}

ConfigCenter& ConfigCenter::instance() {
  static ConfigCenter center;
  return center;
}

ConfigVarBase::ptr ConfigCenter::lookupBase(const std::string& name) {
  std::lock_guard<MutexType> lock(getMutex());

  std::string key = name;
  std::transform(key.begin(), key.end(), key.begin(), ::tolower);

  auto it = getDatas().find(key);
  if (it == getDatas().end()) {
    return nullptr;
  }
  return it->second;
}

void ConfigCenter::visit(std::function<void(ConfigVarBase::ptr)> cb) {
  std::lock_guard<MutexType> lock(getMutex());
  for (auto& kv : getDatas()) {
    cb(kv.second);
  }
}

bool ConfigCenter::loadFromYamlString(const std::string& yaml_str) {
  InitLogConfigBridge();
  try {
    YAML::Node root = YAML::Load(yaml_str);

    std::vector<std::pair<std::string, std::string>> flattened;
    listAllMember("", root, flattened);
    return applyFlattenedYaml(flattened);
  } catch (const YAML::Exception&) {
    return false;
  } catch (const std::exception&) {
    return false;
  }
}

bool ConfigCenter::loadFromYamlFile(const std::string& path) {
  InitLogConfigBridge();
  try {
    YAML::Node root = YAML::LoadFile(path);

    std::vector<std::pair<std::string, std::string>> flattened;
    listAllMember("", root, flattened);
    return applyFlattenedYaml(flattened);
  } catch (const YAML::Exception&) {
    return false;
  } catch (const std::exception&) {
    return false;
  }
}

void ConfigCenter::clear() {
  ResetLogConfigBridge();
  std::lock_guard<MutexType> lock(getMutex());
  getDatas().clear();
}

}  // namespace net
