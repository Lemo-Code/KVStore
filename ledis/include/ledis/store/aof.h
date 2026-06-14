#pragma once

#include "ledis/command/command.h"
#include "ledis/command/command_result.h"
#include "ledis/command/registry.h"
#include "ledis/session/session_context.h"
#include "ledis/store/db_manager.h"
#include "ledis/types.h"

namespace ledis {

enum class AppendFsyncPolicy { kAlways, kEverySec, kNo };

AppendFsyncPolicy parseAppendFsyncPolicy(const String& value, bool* ok);
String appendFsyncPolicyName(AppendFsyncPolicy policy);

String makeAofPath(const String& dir, const String& appendfilename);

/** 写命令是否应记入 AOF（不含 SAVE/BGSAVE 等管理命令）。 */
bool isAofWriteCommand(const Command& cmd);

class AofWriter {
 public:
  AofWriter() = default;
  ~AofWriter();

  bool open(const String& path, bool truncate);
  void close();
  bool isOpen() const { return fp_ != nullptr; }

  bool append(const Command& cmd);
  bool appendRaw(const String& wire);
  bool flushBuffer();
  bool fsyncDisk();
  bool flush();

 private:
  FILE* fp_ = nullptr;
  String path_;
};

/** 根据当前 DB 状态写入新的 AOF 文件（不含 .tmp rename）。 */
bool rewriteAofFromDb(const DBManager& db, const String& path, int64_t now_ms);

bool appendRawCommandsToFile(const String& path, const StdVector<String>& frames);

bool loadAof(const String& path, DBManager* db, CommandRegistry* registry,
             SessionContext* ctx);

}  // namespace ledis
