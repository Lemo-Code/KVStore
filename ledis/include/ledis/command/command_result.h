#pragma once

#include "ledis/protocol/resp_value.h"

namespace ledis {

struct CommandResult {
  RespValue value;
  /** 同一命令需连续写回的额外 RESP 帧（如 SUBSCRIBE 多 channel）。 */
  StdVector<RespValue> trailing;
  /** 阻塞命令（BLPOP 等）尚未完成，不应写回响应。 */
  bool blocked = false;

  static CommandResult ok();
  static CommandResult pong();
  static CommandResult error(const char* message);
  static CommandResult wrongType();
  static CommandResult noAuth();
  static CommandResult integer(int64_t n);
  static CommandResult bulk(Sds data);
  static CommandResult nullBulk();
  static CommandResult queued();
  static CommandResult fromValue(const RespValue& v);
};

}  // namespace ledis
