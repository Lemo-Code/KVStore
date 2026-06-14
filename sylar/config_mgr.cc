/**
 * @file config_mgr.cc
 * @brief ConfigMgr 实现：委托 Config 与 Env 完成加载
 */
#include "sylar/config_mgr.h"
#include "sylar/env.h"
#include "sylar/log.h"
#include <sys/stat.h>

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

void ConfigMgr::loadFromConfDir(const std::string& path) {
    conf_dir_ = path;
    std::string absolute = sylar::EnvMgr::GetInstance()->getAbsolutePath(path);
    Config::LoadFromConfDir(absolute);
}

void ConfigMgr::loadFromFile(const std::string& path) {
    try {
        YAML::Node root = YAML::LoadFile(path);
        Config::LoadFromYaml(root);
        SYLAR_LOG_INFO(g_logger) << "ConfigMgr loadFromFile ok: " << path;
    } catch (const std::exception& e) {
        SYLAR_LOG_ERROR(g_logger) << "ConfigMgr loadFromFile failed: " << path << " " << e.what();
    }
}

void ConfigMgr::reload() {
    std::string path = conf_dir_.empty() ? sylar::EnvMgr::GetInstance()->getConfigPath() : conf_dir_;
    loadFromConfDir(path);
}

} // namespace sylar
