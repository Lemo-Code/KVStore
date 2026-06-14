#include "lemo/config/yaml_loader.h"

#include "lemo/utils/string_util.h"

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

namespace lemo {
namespace config {
namespace {

bool IsValidFlattenKey(const std::string& key) {
  if (key.empty()) {
    return false;
  }
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

void FlattenNode(const std::string& prefix, const YAML::Node& node,
                 std::map<std::string, std::string>* out) {
  if (!out) {
    return;
  }

  if (node.IsScalar()) {
    const std::string key = utils::ToLower(prefix);
    if (IsValidFlattenKey(key)) {
      (*out)[key] = node.Scalar();
    }
    return;
  }

  if (node.IsSequence()) {
    YAML::Emitter emitter;
    emitter << node;
    const std::string key = utils::ToLower(prefix);
    if (IsValidFlattenKey(key)) {
      (*out)[key] = emitter.c_str();
    }
    return;
  }

  if (node.IsMap()) {
    for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
      const std::string part = it->first.as<std::string>();
      const std::string child_prefix =
          prefix.empty() ? part : prefix + "." + part;
      FlattenNode(child_prefix, it->second, out);
    }
  }
}

bool LoadRoot(const YAML::Node& root, std::map<std::string, std::string>* out) {
  if (!out) {
    return false;
  }
  out->clear();
  if (!root.IsDefined() || root.IsNull()) {
    return false;
  }
  FlattenNode("", root, out);
  return !out->empty();
}

}  // namespace

bool YamlLoader::LoadString(const std::string& content,
                            std::map<std::string, std::string>* out) {
  if (!out) {
    return false;
  }
  try {
    const YAML::Node root = YAML::Load(content);
    return LoadRoot(root, out);
  } catch (...) {
    return false;
  }
}

bool YamlLoader::LoadFile(const std::string& path,
                          std::map<std::string, std::string>* out) {
  if (!out) {
    return false;
  }
  try {
    const YAML::Node root = YAML::LoadFile(path);
    return LoadRoot(root, out);
  } catch (...) {
    return false;
  }
}

}  // namespace config
}  // namespace lemo
