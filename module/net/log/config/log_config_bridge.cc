/**
 * @file log_config_bridge.cc
 * @brief ConfigCenter 与 LogConfig / Logger 的运行时整合
 */
#include "log/config/log_config_bridge.h"

#include "config/config_center.h"
#include "log/appender.h"
#include "log/config/log_config.h"
#include "log/config/log_define.h"
#include "log/log_paths.h"
#include "log/manager.h"

#include "thread/mutex.h"

#include <iostream>
#include <set>

namespace net {

namespace {

ConfigVar<int>::ptr g_log_level;
ConfigVar<std::string>::ptr g_log_pattern;
ConfigVar<uint32_t>::ptr g_async_flush_ms;
ConfigVar<uint64_t>::ptr g_async_buf_bytes;
ConfigVar<uint64_t>::ptr g_async_flush_threshold;
ConfigVar<uint64_t>::ptr g_async_soft_cap;
ConfigVar<int>::ptr g_async_degrade_mode;
ConfigVar<uint32_t>::ptr g_async_sample_rate;
ConfigVar<int>::ptr g_file_reopen_sec;
ConfigVar<uint64_t>::ptr g_roll_max_bytes;
ConfigVar<uint32_t>::ptr g_roll_max_files;
ConfigVar<std::set<LogDefine>>::ptr g_logs;

Spinlock g_bridge_mtx;
bool g_bridge_ready = false;

void ApplyLogDefines(const std::set<LogDefine>& old_values,
                     const std::set<LogDefine>& new_values);

LogAppender::ptr MakeAppender(const LogAppenderDefine& a) {
  LogConfig& cfg = LogConfig::instance();
  const std::string file = ResolveLogPath(a.file);
  if (a.type == 1) {
    return LogAppender::ptr(new FileLogAppender(file));
  }
  if (a.type == 2) {
    return LogAppender::ptr(new StdoutLogAppender());
  }
  if (a.type == 4) {
    const uint64_t max_size =
        a.roll_max_size > 0 ? a.roll_max_size : cfg.rollMaxBytes();
    const uint32_t max_files =
        a.roll_max_files > 0 ? a.roll_max_files : cfg.rollMaxFiles();
    return LogAppender::ptr(new RollingFileLogAppender(
        file, max_size, max_files, ParseRollInterval(a.roll_interval)));
  }
  if (a.type == 6) {
    return LogAppender::ptr(new TimeRotateFileLogAppender(
        file, ParseRollInterval(a.roll_interval)));
  }
  if (a.type == 7) {
    const uint32_t slots = a.slot_count > 0 ? a.slot_count : 3;
    const uint64_t max_bytes = a.max_bytes_per_slot > 0 ? a.max_bytes_per_slot
                                                        : cfg.rollMaxBytes();
    return LogAppender::ptr(
        new CircularFileLogAppender(file, slots, max_bytes));
  }
  return nullptr;
}

void ApplyOneLogger(const LogDefine& def) {
  Logger::ptr logger;
  if (def.async) {
    logger = AsyncLoggerMgr::GetInstance()->getLogger(def.name);
  } else {
    logger = LoggerMgr::GetInstance()->getLogger(def.name);
  }
  if (!logger) {
    return;
  }

  logger->setAsync(def.async);
  if (def.level != LogLevel::UNKNOWN) {
    logger->setLevel(def.level);
  }
  if (!def.formatter.empty()) {
    logger->setFormatter(def.formatter);
  }

  logger->clearAppenders();
  for (const auto& a : def.appenders) {
    LogAppender::ptr ap = MakeAppender(a);
    if (!ap) {
      continue;
    }
    if (a.level != LogLevel::UNKNOWN) {
      ap->setLevel(a.level);
    }
    if (!a.formatter.empty()) {
      LogFormatter::ptr fmt(new LogFormatter(a.formatter));
      if (!fmt->hasError()) {
        ap->setFormatter(fmt);
      } else {
        std::cerr << "log config: invalid appender formatter " << a.formatter
                  << '\n';
      }
    }
    logger->addAppender(ap);
  }
}

void DisableLoggerByName(const std::string& name) {
  Logger::ptr logger = LoggerMgr::GetInstance()->getLogger(name);
  if (logger) {
    logger->setLevel(static_cast<LogLevel::Level>(100));
    logger->clearAppenders();
  }
  Logger::ptr alogger = AsyncLoggerMgr::GetInstance()->getLogger(name);
  if (alogger) {
    alogger->setLevel(static_cast<LogLevel::Level>(100));
    alogger->clearAppenders();
  }
}

void ApplyLogDefines(const std::set<LogDefine>& old_values,
                     const std::set<LogDefine>& new_values) {
  if (new_values.empty() && old_values.empty()) {
    return;
  }

  for (const auto& i : new_values) {
    auto it = old_values.find(i);
    if (it == old_values.end() || !(*it == i)) {
      ApplyOneLogger(i);
    }
  }

  for (const auto& i : old_values) {
    if (new_values.find(i) == new_values.end()) {
      DisableLoggerByName(i.name);
    }
  }
}

void SyncLogConfigFromVars() {
  LogModuleSettings s = LogConfig::instance().settings();

  if (g_log_level) {
    s.default_level = g_log_level->getValue();
  }
  if (g_log_pattern) {
    s.default_pattern = g_log_pattern->getValue();
  }
  if (g_async_flush_ms) {
    s.async.flush_interval_ms = g_async_flush_ms->getValue();
  }
  if (g_async_buf_bytes) {
    s.async.buf_bytes = static_cast<size_t>(g_async_buf_bytes->getValue());
  }
  if (g_async_flush_threshold) {
    s.async.flush_threshold =
        static_cast<size_t>(g_async_flush_threshold->getValue());
  }
  if (g_async_soft_cap) {
    s.async.soft_cap = static_cast<size_t>(g_async_soft_cap->getValue());
  }
  if (g_async_degrade_mode) {
    s.async.degrade_mode = g_async_degrade_mode->getValue();
  }
  if (g_async_sample_rate) {
    s.async.sample_rate = g_async_sample_rate->getValue();
  }
  if (g_file_reopen_sec) {
    s.file.reopen_sec = g_file_reopen_sec->getValue();
  }
  if (g_roll_max_bytes) {
    s.file.roll_max_bytes = g_roll_max_bytes->getValue();
  }
  if (g_roll_max_files) {
    s.file.roll_max_files = g_roll_max_files->getValue();
  }

  LogConfig::instance().apply(s);
}

void ClearBridgeVars() {
  g_log_level.reset();
  g_log_pattern.reset();
  g_async_flush_ms.reset();
  g_async_buf_bytes.reset();
  g_async_flush_threshold.reset();
  g_async_soft_cap.reset();
  g_async_degrade_mode.reset();
  g_async_sample_rate.reset();
  g_file_reopen_sec.reset();
  g_roll_max_bytes.reset();
  g_roll_max_files.reset();
  g_logs.reset();
}

void RegisterVars() {
  LogModuleSettings defs = LogConfig::instance().settings();

  g_log_level = ConfigCenter::lookup<int>(
      "log.level", defs.default_level, "default logger level threshold");
  g_log_pattern = ConfigCenter::lookup<std::string>(
      "log.pattern", defs.default_pattern, "default log pattern");
  g_async_flush_ms = ConfigCenter::lookup<uint32_t>(
      "log.async.flush_interval_ms", defs.async.flush_interval_ms,
      "async flush thread interval (ms)");
  g_async_buf_bytes = ConfigCenter::lookup<uint64_t>(
      "log.async.buf_bytes", static_cast<uint64_t>(defs.async.buf_bytes),
      "async per-sink buffer bytes");
  g_async_flush_threshold = ConfigCenter::lookup<uint64_t>(
      "log.async.flush_threshold",
      static_cast<uint64_t>(defs.async.flush_threshold),
      "async flush threshold bytes");
  g_async_soft_cap = ConfigCenter::lookup<uint64_t>(
      "log.async.soft_cap", static_cast<uint64_t>(defs.async.soft_cap),
      "async queue soft cap for degrade");
  g_async_degrade_mode = ConfigCenter::lookup<int>(
      "log.async.degrade_mode", defs.async.degrade_mode,
      "async degrade mode 0/1/2");
  g_async_sample_rate = ConfigCenter::lookup<uint32_t>(
      "log.async.degrade_sample_rate", defs.async.sample_rate,
      "async sample rate when degrade_mode=2");
  g_file_reopen_sec = ConfigCenter::lookup<int>(
      "log.file.reopen_sec", defs.file.reopen_sec,
      "file appender reopen check interval (sec)");
  g_roll_max_bytes = ConfigCenter::lookup<uint64_t>(
      "log.roll.max_bytes", defs.file.roll_max_bytes,
      "default rolling file max bytes");
  g_roll_max_files = ConfigCenter::lookup<uint32_t>(
      "log.roll.max_files", defs.file.roll_max_files,
      "default rolling file retain count");
  g_logs = ConfigCenter::lookup<std::set<LogDefine>>(
      "logs", std::set<LogDefine>(), "per-logger definitions");

  if (!g_log_level || !g_log_pattern || !g_logs) {
    std::cerr << "log config bridge: failed to register ConfigVars\n";
    return;
  }

  g_log_level->addListener(
      [](const int&, const int&) { SyncLogConfigFromVars(); });
  g_log_pattern->addListener(
      [](const std::string&, const std::string&) { SyncLogConfigFromVars(); });
  g_async_flush_ms->addListener(
      [](const uint32_t&, const uint32_t&) { SyncLogConfigFromVars(); });
  g_async_buf_bytes->addListener(
      [](const uint64_t&, const uint64_t&) { SyncLogConfigFromVars(); });
  g_async_flush_threshold->addListener(
      [](const uint64_t&, const uint64_t&) { SyncLogConfigFromVars(); });
  g_async_soft_cap->addListener(
      [](const uint64_t&, const uint64_t&) { SyncLogConfigFromVars(); });
  g_async_degrade_mode->addListener(
      [](const int&, const int&) { SyncLogConfigFromVars(); });
  g_async_sample_rate->addListener(
      [](const uint32_t&, const uint32_t&) { SyncLogConfigFromVars(); });
  g_file_reopen_sec->addListener(
      [](const int&, const int&) { SyncLogConfigFromVars(); });
  g_roll_max_bytes->addListener(
      [](const uint64_t&, const uint64_t&) { SyncLogConfigFromVars(); });
  g_roll_max_files->addListener(
      [](const uint32_t&, const uint32_t&) { SyncLogConfigFromVars(); });

  g_logs->addListener([](const std::set<LogDefine>& old_v,
                         const std::set<LogDefine>& new_v) {
    ApplyLogDefines(old_v, new_v);
  });
}

}  // namespace

void ResetLogConfigBridge() {
  Spinlock::Lock lock(g_bridge_mtx);
  ClearBridgeVars();
  g_bridge_ready = false;
}

void InitLogConfigBridge() {
  Spinlock::Lock lock(g_bridge_mtx);
  if (g_bridge_ready) {
    return;
  }
  RegisterVars();
  if (!g_logs) {
    return;
  }
  g_bridge_ready = true;
  SyncLogConfigFromVars();
  ApplyLogDefines(std::set<LogDefine>(), g_logs->getValue());
}

}  // namespace net
