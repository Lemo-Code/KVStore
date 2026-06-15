/**
 * @file    config.h
 * @brief   Log configuration loader — bridges Config + Logger systems.
 *
 * Reads YAML config and sets up loggers, appenders, and formatters.
 * Supports hot-reload through Config change callbacks.
 *
 * YAML format:
 * @code
 * log:
 *   root_level: DEBUG
 *   loggers:
 *     zero.reactor: { level: TRACE }
 *     zero.scheduler: { level: INFO }
 *   appenders:
 *     - type: console
 *       color: true
 *       pattern: "%d [%p] [%N] %m%n"
 *     - type: file
 *       file: /var/log/zero.log
 *       max_size: 104857600   # 100MB
 *       max_files: 10
 *       pattern: "%d{%Y-%m-%d %H:%M:%S} [%p] [%t] [%F:%l] %m%n"
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

namespace zero {

/**
 * @brief  Configures loggers and appenders from YAML.
 *
 * Usage:
 * @code
 *   LogConfig::LoadFromYaml("/etc/zero/log.yaml");
 *   // Or programmatically:
 *   LogConfig::Setup();
 * @endcode
 */
class LogConfig {
public:
    /// Parse a YAML node under the "log" key and apply configuration.
    static void LoadFromYaml(const YAML::Node& log_node);

    /// Convenience: load from a YAML file.
    static void LoadFromYamlFile(const std::string& filepath);

    /// Convenience: set up from Config system (reads config keys like "log.root_level").
    static void SetupFromConfig();

    /// Quick setup: console appender with color, optional file appender.
    static void QuickSetup(bool console = true, const std::string& logfile = "",
                           LogLevel::Level level = LogLevel::DEBUG);

private:
    /// Parse a single appender definition from YAML.
    static LogAppender::ptr parseAppender(const YAML::Node& node);

    /// Apply logger-level overrides from YAML.
    static void applyLoggerLevels(const YAML::Node& loggers_node);
};

} // namespace zero
