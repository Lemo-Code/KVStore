#ifndef NET_LOG_SINK_H
#define NET_LOG_SINK_H

#include "log/appender.h"
#include "log/file_sink.h"
#include "log/level.h"
#include "log/logger.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace net {

/**
 * @file sink.h
 * @brief 用户可配置的多种「输出地方案」统一入口。
 *
 * | 方案 | SinkKind | 行为摘要 |
 * |------|----------|----------|
 * | 一直写一个文件 | FixedFile | 固定 path，持续追加 |
 * | 并行写 N 个文件 | 多个 FixedFile Spec | 各写各的 path，互不影响 |
 * | 按日期/时间换新文件 | TimeRotateFile | 新时间段 → 新文件，旧文件保留 |
 * | 大小链式切片 | SizeChainFile | path → path.1 → … 保留历史 |
 * | N 槽环形覆盖 | CircularFiles | 0→1→…→N-1→0，回到 0 时丢弃原内容 |
 * | 控制台 | Stdout | 标准输出 |
 *
 * 扩展：新增 SinkKind + Appender 类 + MakeAppender 分支即可。
 *
 * 异步边界：各 Appender 在入队前按 event->getTime() 解析目标路径；
 * 后台 flush 只追加，不在刷盘时刻做切日/换槽/轮转。
 */

enum class SinkKind {
  Stdout = 0,
  FixedFile = 1,       ///< 始终追加到同一文件
  TimeRotateFile = 2,  ///< 按日/按小时顺序生成新文件
  SizeChainFile = 3,   ///< 单文件写满后 rename 为 .1、.2 …
  CircularFiles = 4,     ///< N 个槽位环形复用，覆盖最旧槽
  File = FixedFile,            ///< 兼容旧名
  RollingFile = SizeChainFile, ///< 兼容旧名
};

struct SinkSpec {
  SinkKind kind = SinkKind::FixedFile;
  std::string path;
  std::vector<std::string> paths;  ///< CircularFiles 可显式指定 N 个路径
  uint64_t max_bytes = NET_LOG_ROLL_DEFAULT_MAX_BYTES;
  uint32_t max_files = NET_LOG_ROLL_DEFAULT_MAX_FILES;  ///< SizeChain 保留片数
  uint32_t slot_count = 3;                             ///< Circular 槽位数
  uint64_t max_bytes_per_slot = 1024 * 1024;           ///< Circular 每槽上限
  file_sink::RollInterval roll_interval = file_sink::RollInterval::DAY;
  LogLevel::Level level = LogLevel::UNKNOWN;
};

struct SinkSet {
  std::vector<SinkSpec> specs;

  SinkSet() = default;
  explicit SinkSet(std::vector<SinkSpec> s) : specs(std::move(s)) {}

  SinkSet& Add(const SinkSpec& spec) {
    specs.push_back(spec);
    return *this;
  }

  /** 一直往同一个文件写 */
  static SinkSet FixedFile(const std::string& path);
  static SinkSet SingleFile(const std::string& path) { return FixedFile(path); }

  /** 同时输出到 N 个独立文件（非环形） */
  static SinkSet MultiFile(const std::vector<std::string>& paths);

  /** 按日/按小时顺序切换到新文件 */
  static SinkSet TimeRotate(const std::string& base_path,
                            file_sink::RollInterval interval =
                                file_sink::RollInterval::DAY);

  /** 写满后链式改名为 .1 .2 …（保留历史） */
  static SinkSet SizeChain(const std::string& path,
                           uint64_t max_bytes = NET_LOG_ROLL_DEFAULT_MAX_BYTES,
                           uint32_t max_files = NET_LOG_ROLL_DEFAULT_MAX_FILES);

  static SinkSet Rolling(const std::string& path, uint64_t max_bytes,
                         uint32_t max_files,
                         file_sink::RollInterval interval =
                             file_sink::RollInterval::NONE) {
    SinkSpec spec;
    spec.kind = SinkKind::SizeChainFile;
    spec.path = path;
    spec.max_bytes = max_bytes;
    spec.max_files = max_files;
    spec.roll_interval = interval;
    SinkSet set;
    set.specs.push_back(spec);
    return set;
  }

  /**
   * N 个槽位环形写：写满槽 i 后切到 i+1，超过 N-1 回到 0 并覆盖原 0 号文件。
   */
  static SinkSet CircularRing(const std::string& base_path, uint32_t slot_count,
                              uint64_t max_bytes_per_slot,
                              const std::vector<std::string>& paths = {});

  static SinkSet ConsoleAndFile(const std::string& path);
};

LogAppender::ptr MakeAppender(const SinkSpec& spec);
void ApplySinkSet(const Logger::ptr& logger, const SinkSet& sinks);

}  // namespace net

#endif  // NET_LOG_SINK_H
