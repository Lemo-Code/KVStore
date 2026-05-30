#ifndef NET_CONFIG_CENTER_H
#define NET_CONFIG_CENTER_H

#include "config/config_var.h"

#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace net {

/**
 * @brief 配置中心：统一管理所有配置项
 * 
 * 采用"约定优于配置"原则：
 * 1. 配置项必须通过 Lookup 预先声明才能被加载
 * 2. 配置名称统一小写
 * 3. 只覆盖已存在的配置项
 */
class ConfigCenter {
 public:
  typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;
  typedef std::mutex MutexType;

  /**
   * @brief 获取单例实例
   */
  static ConfigCenter& instance();

  /**
   * @brief 查找或创建配置项
   * 
   * @tparam T 配置值类型
   * @param name 配置项名称
   * @param default_val 默认值
   * @param description 描述
   * @return 配置项智能指针
   */
  template<typename T>
  static typename ConfigVar<T>::ptr lookup(const std::string& name,
                                              const T& default_val,
                                              const std::string& description = "") {
    std::lock_guard<MutexType> lock(getMutex());
    
    std::string key = name;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    
    auto it = getDatas().find(key);
    if (it != getDatas().end()) {
      // 已存在，检查类型是否匹配
      auto var = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
      if (var) {
        return var;
      }
      // 类型不匹配，返回空指针
      return nullptr;
    }
    
    // 检查名称合法性
    if (key.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.") != std::string::npos) {
      return nullptr;
    }
    
    // 创建新配置项
    typename ConfigVar<T>::ptr var(new ConfigVar<T>(key, default_val, description));
    getDatas()[key] = var;
    return var;
  }

  /**
   * @brief 查找已存在的配置项
   * 
   * @tparam T 配置值类型
   * @param name 配置项名称
   * @return 配置项智能指针，不存在返回 nullptr
   */
  template<typename T>
  static typename ConfigVar<T>::ptr lookup(const std::string& name) {
    std::lock_guard<MutexType> lock(getMutex());
    
    std::string key = name;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    
    auto it = getDatas().find(key);
    if (it == getDatas().end()) {
      return nullptr;
    }
    return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
  }

  /**
   * @brief 查找配置项基类（用于通用操作）
   * @param name 配置项名称
   * @return 配置项基类指针，不存在返回 nullptr
   */
  static ConfigVarBase::ptr lookupBase(const std::string& name);

  /**
   * @brief 遍历所有配置项
   * @param cb 回调函数
   */
  static void visit(std::function<void(ConfigVarBase::ptr)> cb);

  /**
   * @brief 从 YAML 字符串加载配置
   * 
   * 格式示例：
   * logs:
   *   - name: root
   *     level: INFO
   * system:
   *   port: 8080
   *   workers: 4
   * 
   * 会被打平为：logs.name=root, logs.level=INFO, system.port=8080, system.workers=4
   * 
   * @param yaml_str YAML 格式字符串
   * @return 是否成功
   */
  static bool loadFromYamlString(const std::string& yaml_str);

  /**
   * @brief 从 YAML 文件加载配置
   * @param path 文件路径
   * @return 是否成功
   */
  static bool loadFromYamlFile(const std::string& path);

  /**
   * @brief 清空所有配置项
   */
  static void clear();

 private:
  ConfigCenter() = default;

  static ConfigVarMap& getDatas();
  static MutexType& getMutex();
};

}  // namespace net

#endif  // NET_CONFIG_CENTER_H
