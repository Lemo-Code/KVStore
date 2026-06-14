#pragma once

#include "ledis/command/command.h"
#include "ledis/command/command_result.h"
#include "ledis/session/session_context.h"

#include <cstdint>

namespace ledis {

/** IO → DB：已解析命令。 */
struct CommandEnvelope {
  uint64_t conn_id = 0;
  uint32_t io_thread_id = 0;
  uint64_t seq = 0;
  SessionContext ctx;
  Command cmd;
};

/** DB → IO：执行结果。 */
struct ReplyEnvelope {
  uint64_t conn_id = 0;
  uint32_t io_thread_id = 0;
  uint64_t seq = 0;
  SessionContext ctx;
  CommandResult result;
  /** Pub/Sub 推送等无对应请求帧的回复。 */
  bool unsolicited = false;
};

}  // namespace ledis
