#include "zero/log/log.h"
#include <yaml-cpp/yaml.h>
namespace zero {
struct LogAppenderDefine {
    std::string type; std::string file; std::string pattern; int level = 0;
};
void LoadLogConfigFromYaml(const std::string& path) {
    try {
        YAML::Node root = YAML::LoadFile(path);
        if (root["logs"]) {
            auto logs = root["logs"];
            for (auto item : logs) {  // YAML::Node iterator returns by value
                LogAppenderDefine def;
                if (item["type"]) def.type = item["type"].as<std::string>();
                if (item["file"]) def.file = item["file"].as<std::string>();
                if (item["pattern"]) def.pattern = item["pattern"].as<std::string>();
                if (item["level"]) def.level = item["level"].as<int>();
            }
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "LoadLogConfigFromYaml error: %s\n", e.what());
    }
}
}
