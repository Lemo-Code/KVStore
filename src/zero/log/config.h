/**
 * @file    config.h
 * @brief   Log configuration loader — bridges Config + Logger systems.
 *
 * 约定优于配置: 默认 console appender + INFO 级别，一行 YAML 即可覆盖。
 *
 * YAML 完整格式:
 * @code
 * log:
 *   root_level: INFO                    # 根日志级别
 *   loggers:                            # 命名 logger 配置
 *     - name: ledis.cluster             # logger 名称
 *       level: DEBUG                    # 级别覆盖
 *       formatter: "%d [%p] [%N] %m%n"  # 自定义格式 (可选)
 *       appenders:                      # 独立 appender (可选, 否则继承 root)
 *         - type: file
 *           file: /var/log/ledis-cluster.log
 *           max_size: 104857600
 *           max_files: 5
 *     - name: ledis.access
 *       level: INFO
 *   appenders:                          # root logger 的 appender
 *     - type: console
 *       color: true
 *       pattern: "%d{%H:%M:%S} [%p] [%N] %m%n"
 *     - type: file
 *       file: /var/log/ledis.log
 *       max_size: 104857600             # 100MB
 *       max_files: 10
 *       pattern: "%d{%Y-%m-%d %H:%M:%S} [%p] [%t] [%F:%l] %m%n"
 *     - type: daily_file                # 按天滚动
 *       file: /var/log/ledis-daily.log
 *       pattern: "%d [%p] %m%n"
 * @endcode
 *
 * @ingroup log
 */

#pragma once

#include "zero/log/log.h"
#include "zero/config/config.h"
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <map>

namespace zero {

/**
 * @brief 每日滚动文件 Appender (文件名自动加日期后缀)
 */
class DailyFileLogAppender : public LogAppender {
public:
    using ptr = std::shared_ptr<DailyFileLogAppender>;
    DailyFileLogAppender(const std::string& filename);
    void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
    void flush() override;
private:
    void reopen();
    std::string filename_;
    std::string current_date_;
    std::ofstream stream_;
};

/**
 * @brief  从 YAML 配置 loggers 和 appenders。
 *
 * 用法:
 * @code
 *   // 从文件加载
 *   LogConfig::LoadFromYamlFile("/etc/ledis/ledis.yaml");
 *   // 仅从 ConfigVar 读取
 *   LogConfig::SetupFromConfig();
 *   // 快速设置
 *   LogConfig::QuickSetup(true, "/var/log/ledis.log", LogLevel::INFO);
 * @endcode
 */
class LogConfig {
public:
    /// 解析 YAML "log:" 节点并应用配置
    static void LoadFromYaml(const YAML::Node& log_node);

    /// 从 YAML 文件加载
    static void LoadFromYamlFile(const std::string& filepath);

    /// 从 ConfigVar 系统读取 (读取 log.root_level 等)
    static void SetupFromConfig();

    /// 快速设置: console appender + 可选 file appender
    static void QuickSetup(bool console = true, const std::string& logfile = "",
                           LogLevel::Level level = LogLevel::DEBUG);

    /// 异步日志 (压测/生产默认): 后台写盘, 不阻塞业务线程
    static void SetupAsyncLog(bool console = false, const std::string& logfile = "",
                              LogLevel::Level level = LogLevel::INFO,
                              uint64_t max_size = 104857600, uint32_t max_files = 10);

    /// 压测模式: 异步日志关闭输出, 避免干扰 QPS
    static void SetupBenchSilent();

    /// 设置热加载: 监听 ConfigVar 变更自动应用
    static void EnableHotReload();

private:
    /// 解析单个 appender 定义
    static LogAppender::ptr parseAppender(const YAML::Node& node);

    /// 应用 logger 配置 (级别 + appender + formatter)
    static void applyLoggerConfig(const YAML::Node& loggers_node);

    /// 从 YAML 配置单个 logger (名称 → 配置节点)
    static void configureLogger(const std::string& name, const YAML::Node& cfg);

    /// 存储热加载回调 ID
    static std::vector<uint64_t> s_hot_reload_ids;
};

} // namespace zero
