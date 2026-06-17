#include "zero/config/config.h"
#include <yaml-cpp/yaml.h>
namespace zero {

ConfigVarBase::ConfigVarBase(const std::string& name, const std::string& description)
    : name_(name), description_(description) {}

void Config::LoadFromYaml(const YAML::Node& root) {
    for (auto it = root.begin(); it != root.end(); ++it) {
        std::string key = it->first.as<std::string>();
        auto var = Config::LookupBase(key);
        if (var) var->fromYaml(it->second);
    }
}
bool Config::LoadFromYamlFile(const std::string& path) {
    try {
        YAML::Node root = YAML::LoadFile(path);
        LoadFromYaml(root);
        return true;
    } catch (const std::exception& e) {
        fprintf(stderr, "Config::LoadFromYaml error: %s\n", e.what());
        return false;
    }
}
}
