#pragma once

#include "ledis/types.h"

#include <cstdint>

namespace ledis {

struct LedisSettings {
  String host = "127.0.0.1";
  uint16_t port = 6379;
  uint32_t io_threads = 1;
  bool single_thread_mode = true;
  size_t max_pending_commands = 1024 * 64;
  size_t query_buffer_limit = 1024 * 1024;
  size_t maxclients = 10000;
  /** 0 表示不限制。 */
  size_t maxmemory = 0;
  String maxmemory_policy = "allkeys-lru";
  bool active_expire_enabled = true;
  size_t active_expire_cycle_keys = 20;
  /** 非空时除 AUTH/PING 外需先认证。 */
  String requirepass;
  /** RDB-lite 快照目录与文件名。 */
  String dir = ".";
  String dbfilename = "dump.ledis";
  /** AOF 追加持久化。 */
  bool appendonly = false;
  String appendfilename = "appendonly.aof";
  /** always / everysec / no */
  String appendfsync = "everysec";
  /** 非空时先加载 YAML，再由 CLI 覆盖。 */
  String config_file;
};

/** 解析命令行；先加载 YAML（--config）再应用 CLI 覆盖。 */
LedisSettings ParseLedisSettingsFromArgs(int argc, char** argv);

/** 在已有 settings 上应用 CLI / positional 覆盖（不重新加载 YAML）。 */
void ApplyLedisCliOverrides(int argc, char** argv, LedisSettings* settings);

}  // namespace ledis
