/**
 * @file    config.cc
 * @brief   LogConfig — YAML → Logger 配置加载器
 *
 * 约定优于配置:
 *   - LoggerManager 构造时已添加 ConsoleLogAppender (默认约定)
 *   - 加载 YAML 后覆盖 (配置)
 *   - 未注册的 YAML key 静默忽略
 */

#include "zero/log/config.h"
#include "zero/log/async_log.h"
#include "zero/config/config.h"
#include <yaml-cpp/yaml.h>
#include <ctime>
#include <iomanip>

namespace zero {

// ============================================================
// DailyFileLogAppender
// ============================================================

DailyFileLogAppender::DailyFileLogAppender(const std::string& filename)
    : filename_(filename) {
    reopen();
}

void DailyFileLogAppender::log(std::shared_ptr<Logger>, LogLevel::Level,
                                LogEvent::ptr event) {
    Mutex::Lock lock(mutex_);

    // 检查日期是否变化
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char date_buf[16];
    strftime(date_buf, sizeof(date_buf), "%Y%m%d", &tm_buf);
    std::string today(date_buf);

    if (today != current_date_) {
        if (stream_.is_open()) stream_.close();
        reopen();
    }

    if (stream_.is_open()) {
        std::string output;
        if (formatter_) {
            output = formatter_->format(nullptr, level_, event);
        } else {
            output = event->getSS().str() + "\n";
        }
        stream_ << output << std::flush;
    }
}

void DailyFileLogAppender::flush() {
    if (stream_.is_open()) stream_.flush();
}

void DailyFileLogAppender::reopen() {
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char date_buf[16];
    strftime(date_buf, sizeof(date_buf), "%Y%m%d", &tm_buf);
    current_date_ = date_buf;

    std::string dated_name = filename_ + "." + current_date_;
    if (stream_.is_open()) stream_.close();
    stream_.open(dated_name, std::ios::app);
}

// ============================================================
// LogConfig implementation
// ============================================================

std::vector<uint64_t> LogConfig::s_hot_reload_ids;

void LogConfig::LoadFromYaml(const YAML::Node& log_node) {
    if (!log_node.IsMap()) return;

    auto root_logger = LoggerMgr::GetInstance()->getRoot();

    // 1. root_level
    if (log_node["root_level"] && log_node["root_level"].IsScalar()) {
        auto level = LogLevel::FromString(log_node["root_level"].as<std::string>());
        if (level != LogLevel::UNKNOWN) {
            root_logger->setLevel(level);
        }
    }

    // 2. root appenders (如果配置了, 清空默认 console)
    if (log_node["appenders"] && log_node["appenders"].IsSequence()) {
        static bool first_load = true;
        if (first_load) {
            root_logger->clearAppenders();
            first_load = false;
        }
        for (const auto& item : log_node["appenders"]) {
            auto ap = parseAppender(item);
            if (ap) root_logger->addAppender(ap);
        }
    }

    // 3. named loggers (支持 list 和 map 两种格式)
    if (log_node["loggers"]) {
        if (log_node["loggers"].IsSequence()) {
            // 新格式: list of {name, level, formatter, appenders}
            applyLoggerConfig(log_node["loggers"]);
        } else if (log_node["loggers"].IsMap()) {
            // 旧格式: map {name: {level: ...}}
            for (auto it = log_node["loggers"].begin(); it != log_node["loggers"].end(); ++it) {
                std::string name = it->first.as<std::string>();
                configureLogger(name, it->second);
            }
        }
    }

    // 4. root formatter
    if (log_node["formatter"] && log_node["formatter"].IsScalar()) {
        root_logger->setFormatter(log_node["formatter"].as<std::string>());
    }
}

void LogConfig::LoadFromYamlFile(const std::string& filepath) {
    try {
        YAML::Node root = YAML::LoadFile(filepath);
        if (root["log"] && root["log"].IsMap()) {
            LoadFromYaml(root["log"]);
        } else if (root["zero"] && root["zero"]["log"] && root["zero"]["log"].IsMap()) {
            LoadFromYaml(root["zero"]["log"]);
        } else {
            LoadFromYaml(root);
        }
    } catch (const std::exception& e) {
        // 日志系统尚未初始化，无法用 logger 输出
    }
}

void LogConfig::SetupFromConfig() {
    auto cv_level = Config::Lookup<std::string>("log.root_level");
    LogLevel::Level level = LogLevel::INFO;
    if (cv_level) {
        auto parsed = LogLevel::FromString(cv_level->getValue());
        if (parsed != LogLevel::UNKNOWN) level = parsed;
    }

    auto cv_logfile = Config::Lookup<std::string>("log.file");
    std::string logfile;
    uint64_t max_size = 104857600;
    uint32_t max_files = 10;
    if (cv_logfile && !cv_logfile->getValue().empty()) {
        logfile = cv_logfile->getValue();
        auto cv_maxsize = Config::Lookup<uint64_t>("log.max_size", 104857600);
        auto cv_maxfiles = Config::Lookup<uint32_t>("log.max_files", 10);
        if (cv_maxsize) max_size = cv_maxsize->getValue();
        if (cv_maxfiles) max_files = cv_maxfiles->getValue();
    }
    SetupAsyncLog(false, logfile, level, max_size, max_files);
}

void LogConfig::SetupAsyncLog(bool console, const std::string& logfile,
                               LogLevel::Level level,
                               uint64_t max_size, uint32_t max_files) {
    auto& writer = AsyncLogWriter::GetInstance();
    writer.stop();
    writer.clearAppenders();
    if (console)
        writer.addAppender(std::make_shared<AsyncStdoutAppender>());
    if (!logfile.empty())
        writer.addAppender(std::make_shared<AsyncFileAppender>(
            logfile, max_size, max_files, false));
    writer.start();

    auto root = LoggerMgr::GetInstance()->getRoot();
    root->setLevel(level);
    root->clearAppenders();
    root->addAppender(std::make_shared<AsyncBridgeLogAppender>());
}

void LogConfig::SetupBenchSilent() {
    SetupAsyncLog(false, "", LogLevel::OFF);
}

void LogConfig::QuickSetup(bool console, const std::string& logfile,
                            LogLevel::Level level) {
    SetupAsyncLog(console, logfile, level);
}

void LogConfig::EnableHotReload() {
    // 监听 log.root_level 变更
    auto cv_level = Config::Lookup<std::string>("log.root_level");
    if (cv_level) {
        auto id = cv_level->addListener([](const std::string&, const std::string& new_val) {
            auto level = LogLevel::FromString(new_val);
            if (level != LogLevel::UNKNOWN) {
                LoggerMgr::GetInstance()->getRoot()->setLevel(level);
            }
        });
        s_hot_reload_ids.push_back(id);
    }
}

// ---- private ----

LogAppender::ptr LogConfig::parseAppender(const YAML::Node& node) {
    if (!node["type"] || !node["type"].IsScalar()) return nullptr;

    std::string type = node["type"].as<std::string>();
    LogAppender::ptr appender;

    if (type == "console" || type == "stdout") {
        bool color = true;
        if (node["color"] && node["color"].IsScalar())
            color = node["color"].as<bool>();
        appender = std::make_shared<ConsoleLogAppender>(color);
    } else if (type == "file") {
        if (!node["file"] || !node["file"].IsScalar()) return nullptr;
        std::string file = node["file"].as<std::string>();
        uint64_t max_size = 0;
        uint32_t max_files = 10;
        if (node["max_size"] && node["max_size"].IsScalar())
            max_size = node["max_size"].as<uint64_t>();
        if (node["max_files"] && node["max_files"].IsScalar())
            max_files = node["max_files"].as<uint32_t>();
        appender = std::make_shared<FileLogAppender>(file, max_size, max_files);
    } else if (type == "daily_file") {
        if (!node["file"] || !node["file"].IsScalar()) return nullptr;
        appender = std::make_shared<DailyFileLogAppender>(node["file"].as<std::string>());
    }

    if (!appender) return nullptr;

    // level
    if (node["level"] && node["level"].IsScalar()) {
        auto lvl = LogLevel::FromString(node["level"].as<std::string>());
        if (lvl != LogLevel::UNKNOWN) appender->setLevel(lvl);
    }
    // pattern / formatter
    if (node["pattern"] && node["pattern"].IsScalar()) {
        appender->setFormatter(std::make_shared<LogFormatter>(node["pattern"].as<std::string>()));
    }

    return appender;
}

void LogConfig::applyLoggerConfig(const YAML::Node& loggers_node) {
    for (const auto& item : loggers_node) {
        if (!item["name"] || !item["name"].IsScalar()) continue;
        std::string name = item["name"].as<std::string>();
        configureLogger(name, item);
    }
}

void LogConfig::configureLogger(const std::string& name, const YAML::Node& cfg) {
    auto logger = LoggerMgr::GetInstance()->getLogger(name);

    // level
    if (cfg["level"] && cfg["level"].IsScalar()) {
        auto lvl = LogLevel::FromString(cfg["level"].as<std::string>());
        if (lvl != LogLevel::UNKNOWN) logger->setLevel(lvl);
    }

    // formatter
    if (cfg["formatter"] && cfg["formatter"].IsScalar()) {
        logger->setFormatter(cfg["formatter"].as<std::string>());
    }

    // logger 专属 appenders
    if (cfg["appenders"] && cfg["appenders"].IsSequence()) {
        for (const auto& item : cfg["appenders"]) {
            auto ap = parseAppender(item);
            if (ap) logger->addAppender(ap);
        }
    }
}

// ---- Legacy ----
void LoadLogConfigFromYaml(const std::string& path) {
    LogConfig::LoadFromYamlFile(path);
}

} // namespace zero
