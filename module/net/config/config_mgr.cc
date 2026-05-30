/**
 * @file config_mgr.cc
 */
#include "config/config_mgr.h"

#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

namespace net {

namespace {

/**
 * @brief 检查文件是否为常规文件且以指定后缀结尾
 */
bool isYamlFile(const std::string& filename) {
  if (filename.size() >= 5 &&
      filename.compare(filename.size() - 4, 4, ".yml") == 0) {
    return true;
  }
  if (filename.size() >= 6 &&
      filename.compare(filename.size() - 5, 5, ".yaml") == 0) {
    return true;
  }
  return false;
}

/**
 * @brief 获取目录下所有 YAML 文件（按文件名排序）
 */
std::vector<std::string> listYamlFiles(const std::string& dir) {
  std::vector<std::string> files;
  
  DIR* dp = opendir(dir.c_str());
  if (dp == nullptr) {
    return files;
  }
  
  struct dirent* entry;
  while ((entry = readdir(dp)) != nullptr) {
    std::string name = entry->d_name;
    if (isYamlFile(name)) {
      files.push_back(dir + "/" + name);
    }
  }
  
  closedir(dp);
  
  // 按文件名排序，确保加载顺序一致
  std::sort(files.begin(), files.end());
  return files;
}

/**
 * @brief 检查文件是否存在且可读
 */
bool fileExists(const std::string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

}  // namespace

size_t ConfigMgr::loadFromConfDir(const std::string& path) {
  conf_dir_ = path;
  
  std::vector<std::string> files = listYamlFiles(path);
  size_t success_count = 0;
  
  for (const auto& file : files) {
    if (ConfigCenter::loadFromYamlFile(file)) {
      ++success_count;
    }
  }
  
  return success_count;
}

bool ConfigMgr::loadFromFile(const std::string& path) {
  if (!fileExists(path)) {
    return false;
  }
  return ConfigCenter::loadFromYamlFile(path);
}

size_t ConfigMgr::reload() {
  if (conf_dir_.empty()) {
    return 0;
  }
  return loadFromConfDir(conf_dir_);
}

std::vector<std::string> ConfigMgr::listConfigVars() {
  std::vector<std::string> names;
  ConfigCenter::visit([&names](ConfigVarBase::ptr var) {
    names.push_back(var->getName());
  });
  return names;
}

}  // namespace net
