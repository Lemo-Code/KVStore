#pragma once

#include "ledis/command/command.h"
#include "ledis/command/command_result.h"
#include "ledis/session/session_context.h"
#include "ledis/store/db_manager.h"
#include "ledis/types.h"

namespace ledis {

class CommandRegistry {
 public:
  using Handler = Function<CommandResult(SessionContext&, DBManager&, const Command&)>;

  void registerHandler(const String& name, Handler handler);
  void setRequirePass(const String& pass);
  void setServerConfig(uint16_t port, size_t maxclients, size_t maxmemory = 0,
                       const String& maxmemory_policy = "allkeys-lru");
  uint16_t configPort() const { return config_port_; }
  size_t configMaxclients() const { return config_maxclients_; }
  void setSnapshotConfig(const String& dir, const String& dbfilename);
  void setAofConfig(bool appendonly, const String& appendfilename,
                    const String& appendfsync = "everysec");
  void registerDefaultCommands();
  CommandResult configGet(const String& param) const;
  CommandResult configSet(const String& param, const String& value);
  using ConfigApplyFn = Function<void(const String& param, const String& value)>;
  void setConfigApplyCallback(ConfigApplyFn fn);
  CommandResult dispatch(SessionContext& ctx, DBManager& db,
                         const Command& cmd) const;

 private:
  CommandResult dispatchOne(SessionContext& ctx, DBManager& db,
                            const Command& cmd) const;
  /** Handler 含 std::function，暂保留 StdUnorderedMap（lstl 联调后切换 SdsDict）。 */
  StdUnorderedMap<String, Handler> handlers_;
  String requirepass_;
  uint16_t config_port_ = 6379;
  size_t config_maxclients_ = 10000;
  size_t config_maxmemory_ = 0;
  String config_maxmemory_policy_ = "allkeys-lru";
  String config_dir_ = ".";
  String config_dbfilename_ = "dump.ledis";
  bool config_appendonly_ = false;
  String config_appendfilename_ = "appendonly.aof";
  String config_appendfsync_ = "everysec";
  ConfigApplyFn config_apply_;
};

}  // namespace ledis
