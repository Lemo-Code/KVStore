#ifndef NET_CONFIG_MGR_H
#define NET_CONFIG_MGR_H

#include "config/config_center.h"
#include "common/singleton.h"

#include <string>
#include <vector>

namespace net {

/**
 * @brief 配置管理器：统一加载入口与目录监听
 * 
 * 与 ConfigCenter 配合使用：
 * 1. 各模块通过 ConfigCenter::lookup 声明配置项
 * 2. ConfigMgr 负责从文件/目录加载并填充
 * 3. 支持热更新（可选）
 */
class ConfigMgr {
 public:
  /**
   * @brief 从配置目录加载所有 .yml 和 .yaml 文件
   * 
   * 按文件名排序后顺序加载，后加载的配置会覆盖先加载的同名配置
   * 
   * @param path 目录路径
   * @return 成功加载的文件数
   */
  size_t loadFromConfDir(const std::string& path);

  /**
   * @brief 从单个 YAML 文件加载
   * @param path 文件路径
   * @return 是否成功
   */
  bool loadFromFile(const std::string& path);

  /**
   * @brief 再次从上次的配置目录加载（热更新）
   * @return 成功加载的文件数
   */
  size_t reload();

  /**
   * @brief 设置默认配置目录
   */
  void setConfDir(const std::string& path) { conf_dir_ = path; }

  /**
   * @brief 获取当前配置目录
   */
  const std::string& getConfDir() const { return conf_dir_; }

  /**
   * @brief 列出目录下所有配置项
   */
  std::vector<std::string> listConfigVars();

 private:
  ConfigMgr() = default;
  friend class Singleton<ConfigMgr>;

  std::string conf_dir_;
};

typedef Singleton<ConfigMgr> ConfigMgrInstance;

}  // namespace net

#endif  // NET_CONFIG_MGR_H
