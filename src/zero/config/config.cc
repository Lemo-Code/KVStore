#include "zero/config/config.h"
#include <yaml-cpp/yaml.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

namespace zero {

ConfigVarBase::ConfigVarBase(const std::string& name, const std::string& description)
    : name_(name), description_(description) {}

// 前向声明
static void LoadFromYamlRecursive(const YAML::Node& node, const std::string& prefix);

void Config::LoadFromYaml(const YAML::Node& root) {
    // 使用递归匹配以支持嵌套 key (如 zero.log.root_level)
    LoadFromYamlRecursive(root, "");
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

// 递归遍历 YAML 树，生成 "a.b.c" 风格的 key 并尝试匹配 ConfigVar
static void LoadFromYamlRecursive(const YAML::Node& node, const std::string& prefix) {
    if (!node.IsMap()) return;
    for (auto it = node.begin(); it != node.end(); ++it) {
        std::string key = it->first.as<std::string>();
        std::string full_key = prefix.empty() ? key : prefix + "." + key;
        auto var = Config::LookupBase(full_key);
        if (var) {
            // 精确匹配: 直接设置值
            var->fromYaml(it->second);
        } else if (it->second.IsMap()) {
            // 没有精确匹配的 ConfigVar，继续递归
            LoadFromYamlRecursive(it->second, full_key);
        }
        // 不是 Map 且没有匹配的 ConfigVar → 忽略 (约定优于配置)
    }
}

void Config::LoadFromConfDir(const std::string& dir_path, bool recursive) {
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        fprintf(stderr, "Config::LoadFromConfDir: cannot open dir %s: %s\n",
                dir_path.c_str(), strerror(errno));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);

        // 跳过 . 和 ..
        if (name == "." || name == "..") continue;

        std::string full_path = dir_path + "/" + name;

        struct stat st;
        if (stat(full_path.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            // 子目录: 递归进入 (如果 recursive=true)
            if (recursive) {
                LoadFromConfDir(full_path, true);
            }
        } else if (S_ISREG(st.st_mode)) {
            // 检查扩展名: .yaml 或 .yml
            size_t dot = name.find_last_of('.');
            if (dot != std::string::npos) {
                std::string ext = name.substr(dot);
                if (ext == ".yaml" || ext == ".yml") {
                    try {
                        YAML::Node root = YAML::LoadFile(full_path);
                        // 使用递归匹配：config 文件中可以用嵌套结构表示 "a.b.c"
                        LoadFromYamlRecursive(root, "");
                        fprintf(stderr, "Config::LoadFromConfDir: loaded %s\n", full_path.c_str());
                    } catch (const std::exception& e) {
                        fprintf(stderr, "Config::LoadFromConfDir: error loading %s: %s\n",
                                full_path.c_str(), e.what());
                    }
                }
            }
        }
    }
    closedir(dir);
}

} // namespace zero
