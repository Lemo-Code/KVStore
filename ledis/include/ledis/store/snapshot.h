#pragma once

#include "ledis/store/db_manager.h"
#include "ledis/types.h"

#include <cstdint>

namespace ledis {

String makeSnapshotPath(const String& dir, const String& dbfilename);

/** 将非过期 key 写入快照文件（原子写：先 .tmp 再 rename）。 */
bool saveSnapshot(const DBManager& db, const String& path, int64_t now_ms);

/** 从快照文件恢复；文件不存在时返回 true 且不修改 db。 */
bool loadSnapshot(const String& path, DBManager* db);

/** 深拷贝当前有效数据，供 BGSAVE 后台写盘。 */
DBManager cloneDbSnapshot(const DBManager& db, int64_t now_ms);

}  // namespace ledis
