#ifndef NET_LOG_FILE_SINK_H
#define NET_LOG_FILE_SINK_H

#include <cstdint>
#include <ctime>
#include <fstream>
#include <string>

namespace net {
namespace file_sink {

/**
 * @file file_sink.h
 * @brief 文件类 Appender 的公共 IO 与轮转辅助（保证单条日志原子写入同一文件）。
 */

// 轮转周期（仅同步路径生效）
enum class RollInterval {
  NONE = 0,  // 仅按大小轮转
  HOUR = 1,
  DAY = 2,
};

// 以追加方式打开，返回是否成功
bool OpenAppend(std::ofstream& out, const std::string& path);

// 截断后新建（用于环形槽位覆盖旧内容）
bool OpenTruncate(std::ofstream& out, const std::string& path);

// 环形槽位路径：base.0 / base.1 / …
std::string SlotPath(const std::string& base, uint32_t index);

// 按时间切分的新文件路径：base.2025-05-18 或 base.2025-05-18-13
std::string DatedPath(const std::string& base, RollInterval interval, time_t now);

// 查询当前文件大小（字节），失败返回 0
uint64_t FileSize(const std::string& path);

// 关闭后重新打开追加，并返回当前文件大小
bool ReopenAppend(std::ofstream& out, const std::string& path, uint64_t* out_size);

// 将 path 按 .1 .2 ... .max_files 链式后移，再新建 path（max_files>=1）
bool RollBySize(const std::string& path, uint32_t max_files);

// 将当前活动文件改名为 path.suffix，并新建空 path
bool RollByTimeSuffix(const std::string& path, const std::string& suffix);

// 是否到达按时间切割边界
bool ShouldRollByTime(RollInterval interval, time_t now, time_t last_roll);

// 生成按日/按小时的后缀
std::string MakeTimeSuffix(RollInterval interval, time_t now);

}  // namespace file_sink
}  // namespace net

#endif  // NET_LOG_FILE_SINK_H
