#pragma once

#include <string>
#include <string_view>
#include <lstl/container/vector.h>
#include <lstl/container/unordered_map.h>
#include <functional>
#include <cstdlib>
#include <fnmatch.h>

namespace ledis {

// ============================================================
// ConfigManager — 运行时配置管理 (CONFIG GET/SET/REWRITE)
// ============================================================
class ConfigManager {
public:
    using ConfigGetCallback = std::function<std::string()>;
    using ConfigSetCallback = std::function<bool(const std::string&)>;

    struct ConfigItem {
        std::string key;
        std::string value;
        bool        can_set = false;  // 是否支持运行时修改
        ConfigGetCallback getter;
        ConfigSetCallback setter;
    };

    ConfigManager() { registerDefaults(); }

    // CONFIG GET <pattern> — 返回匹配的 key-value 对
    lstl::vector<std::pair<std::string, std::string>> get(const std::string& pattern) {
        lstl::vector<std::pair<std::string, std::string>> result;
        for (auto& item : items_) {
            item.value = item.getter();
            if (fnmatch(pattern.c_str(), item.key.c_str(), 0) == 0) {
                result.push_back(item.key, item.value);
            }
        }
        return result;
    }

    // CONFIG SET <key> <value> — 运行时修改
    bool set(const std::string& key, const std::string& value) {
        for (auto& item : items_) {
            if (item.key == key) {
                if (!item.can_set) return false;
                return item.setter(value);
            }
        }
        return false;
    }

    // 注册配置项
    void registerItem(const std::string& key, bool can_set,
                      ConfigGetCallback getter, ConfigSetCallback setter) {
        items_.push_back({key, "", can_set, std::move(getter), std::move(setter)});
    }

    // ---- 便捷访问器 ----
    int         port()         const { return port_; }
    std::string bindAddr()     const { return bind_; }
    int         ioThreads()    const { return io_threads_; }
    int         maxClients()   const { return max_clients_; }
    std::string requirepass()  const { return requirepass_; }
    size_t      maxmemory()    const { return maxmemory_; }
    std::string maxmemoryPolicy() const { return maxmemory_policy_; }
    std::string aofPath()      const { return aof_path_; }
    std::string aofFsync()     const { return aof_fsync_; }
    int         slowlogSlowerThan() const { return slowlog_slower_than_; }
    int         slowlogMaxLen()     const { return slowlog_max_len_; }
    int         databases()    const { return databases_; }
    std::string logLevel()     const { return log_level_; }

    // 直接设置 (从命令行/配置文件初始化)
    void setPort(int v)              { port_ = v; }
    void setBind(const std::string& v) { bind_ = v; }
    void setIoThreads(int v)         { io_threads_ = v; }
    void setMaxClients(int v)        { max_clients_ = v; }
    void setRequirepass(const std::string& v) { requirepass_ = v; }
    void setMaxmemory(size_t v)      { maxmemory_ = v; }
    void setMaxmemoryPolicy(const std::string& v) { maxmemory_policy_ = v; }
    void setAofPath(const std::string& v)    { aof_path_ = v; }
    void setAofFsync(const std::string& v)   { aof_fsync_ = v; }
    void setSlowlogSlowerThan(int v)  { slowlog_slower_than_ = v; }
    void setSlowlogMaxLen(int v)      { slowlog_max_len_ = v; }
    void setDatabases(int v)         { databases_ = v; }
    void setLogLevel(const std::string& v) { log_level_ = v; }

private:
    void registerDefaults() {
        // 注册所有可 CONFIG GET 的项
        registerItem("port", false,
            [this]{ return std::to_string(port_); }, nullptr);
        registerItem("bind", false,
            [this]{ return bind_; }, nullptr);
        registerItem("io-threads", true,
            [this]{ return std::to_string(io_threads_); },
            [this](const std::string& v){ io_threads_ = atoi(v.c_str()); return true; });
        registerItem("maxclients", true,
            [this]{ return std::to_string(max_clients_); },
            [this](const std::string& v){ max_clients_ = atoi(v.c_str()); return true; });
        registerItem("requirepass", true,
            [this]{ return requirepass_; },
            [this](const std::string& v){ requirepass_ = v; return true; });
        registerItem("maxmemory", true,
            [this]{ return std::to_string(maxmemory_); },
            [this](const std::string& v){ maxmemory_ = strtoull(v.c_str(), nullptr, 10); return true; });
        registerItem("maxmemory-policy", true,
            [this]{ return maxmemory_policy_; },
            [this](const std::string& v){ maxmemory_policy_ = v; return true; });
        registerItem("slowlog-log-slower-than", true,
            [this]{ return std::to_string(slowlog_slower_than_); },
            [this](const std::string& v){ slowlog_slower_than_ = atoi(v.c_str()); return true; });
        registerItem("slowlog-max-len", true,
            [this]{ return std::to_string(slowlog_max_len_); },
            [this](const std::string& v){ slowlog_max_len_ = atoi(v.c_str()); return true; });
        registerItem("databases", false,
            [this]{ return std::to_string(databases_); }, nullptr);
        registerItem("aof-path", false,
            [this]{ return aof_path_; }, nullptr);
        registerItem("aof-fsync", true,
            [this]{ return aof_fsync_; },
            [this](const std::string& v){ aof_fsync_ = v; return true; });
    }

    lstl::vector<ConfigItem> items_;

    int         port_ = 6379;
    std::string bind_ = "0.0.0.0";
    int         io_threads_ = 3;
    int         max_clients_ = 10000;
    std::string requirepass_;
    size_t      maxmemory_ = 0;
    std::string maxmemory_policy_ = "noeviction";
    std::string aof_path_;
    std::string aof_fsync_ = "everysec";
    int         slowlog_slower_than_ = 10000;
    int         slowlog_max_len_ = 128;
    int         databases_ = 16;
    std::string log_level_ = "info";
};

} // namespace ledis
