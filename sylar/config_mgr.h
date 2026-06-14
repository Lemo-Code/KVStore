/**
 * @file config_mgr.h
 * @brief 统一配置管理入口：YAML 加载、目录监听、热更新
 *
 * 与 Config/ConfigVar 配合使用：各模块通过 Config::Lookup 声明配置项，
 * ConfigMgr 负责从文件/目录加载并填充，可选热更新。
 */
#ifndef __SYLAR_CONFIG_MGR_H__
#define __SYLAR_CONFIG_MGR_H__

#include "sylar/config.h"
#include "sylar/singleton.h"
#include <string>

namespace sylar {

/**
 * @brief 配置管理单例
 * 提供统一加载入口与热更新，具体配置项仍由 Config::Lookup 在各模块声明
 */
class ConfigMgr {
public:
    using ptr = std::shared_ptr<ConfigMgr>;

    /**
     * @brief 从配置目录加载所有 .yml 文件（约定优于配置，只覆盖已 Lookup 的项）
     * @param path 相对或绝对路径，通常来自 -c 或 getConfigPath()
     */
    void loadFromConfDir(const std::string& path);

    /**
     * @brief 从单个 YAML 文件加载
     */
    void loadFromFile(const std::string& path);

    /**
     * @brief 再次从上次的配置目录加载，用于热更新
     * 若未设置过目录则使用 getConfigPath()
     */
    void reload();

    /**
     * @brief 设置默认配置目录（供 reload 使用）
     */
    void setConfDir(const std::string& path) { conf_dir_ = path; }
    const std::string& getConfDir() const { return conf_dir_; }

private:
    ConfigMgr() = default;
    friend class Singleton<ConfigMgr>;

    std::string conf_dir_;
};

typedef Singleton<ConfigMgr> ConfigMgrInstance;

} // namespace sylar

#endif // __SYLAR_CONFIG_MGR_H__
