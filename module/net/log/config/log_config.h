#ifndef NET_LOG_CONFIG_LOG_CONFIG_H
#define NET_LOG_CONFIG_LOG_CONFIG_H

#include "build_config.h"

#include "thread/mutex.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace net {

/** 异步通道运行时参数（默认来自 config.h 宏，可在进程内覆盖） */
struct AsyncLogSettings {
  uint32_t flush_interval_ms;
  size_t buf_bytes;
  size_t flush_threshold;
  size_t soft_cap;
  int degrade_mode;
  uint32_t sample_rate;

  AsyncLogSettings();
};

/** 文件 Appender / 轮转默认（可被 YAML 覆盖） */
struct FileLogSettings {
  int reopen_sec;
  uint64_t roll_max_bytes;
  uint32_t roll_max_files;

  FileLogSettings();
};

/** 日志模块全局默认（级别、格式、异步参数） */
struct LogModuleSettings {
  int default_level;
  std::string default_pattern;
  AsyncLogSettings async;
  FileLogSettings file;

  LogModuleSettings();
};

/**
 * @brief net 日志配置中心：编译期默认值 + 运行时覆盖 + 过载降级策略。
 *
 * degrade_mode：
 *   0 — 不降级
 *   1 — 队列积压 >= soft_cap 时丢弃新日志
 *   2 — 积压超限时按 sample_rate 采样保留（每 N 条保留 1 条）
 */
class LogConfig {
 public:
  static LogConfig& instance();

  LogModuleSettings settings() const;

  void apply(const LogModuleSettings& settings);
  void applyAsync(const AsyncLogSettings& async);
  void resetToDefaults();

  uint32_t flushIntervalMs() const;
  size_t bufBytes() const;
  size_t flushThreshold() const;
  size_t softCap() const;
  int degradeMode() const;
  uint32_t sampleRate() const;
  int defaultLevel() const;
  const std::string& defaultPattern() const;
  int fileReopenSec() const;
  uint64_t rollMaxBytes() const;
  uint32_t rollMaxFiles() const;

  bool allowAsyncEnqueue(size_t pending_count);
  void recordEnqueueAccepted();
  void recordEnqueueDropped();
  void recordEnqueueSampled();

  uint64_t acceptedCount() const;
  uint64_t droppedCount() const;
  uint64_t sampledCount() const;
  void resetStats();

 private:
  LogConfig();

  mutable Spinlock mtx_;
  LogModuleSettings settings_;

  std::atomic<uint64_t> accepted_;
  std::atomic<uint64_t> dropped_;
  std::atomic<uint64_t> sampled_;
  std::atomic<uint64_t> sample_seq_;
};

}  // namespace net

#endif  // NET_LOG_CONFIG_LOG_CONFIG_H
