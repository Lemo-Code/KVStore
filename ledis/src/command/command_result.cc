#include "ledis/command/command_result.h"
#include "ledis/types.h"

namespace ledis {

CommandResult CommandResult::ok() {
  return fromValue(RespValue::makeSimpleString(Sds("OK")));
}

CommandResult CommandResult::pong() {
  return fromValue(RespValue::makeSimpleString(Sds("PONG")));
}

CommandResult CommandResult::error(const char* message) {
  return fromValue(RespValue::makeError(Sds(message)));
}

CommandResult CommandResult::wrongType() {
  return error("ERR wrong type");
}

CommandResult CommandResult::noAuth() {
  return error("NOAUTH Authentication required.");
}

CommandResult CommandResult::integer(int64_t n) {
  return fromValue(RespValue::makeInteger(n));
}

CommandResult CommandResult::bulk(Sds data) {
  return fromValue(RespValue::makeBulk(Move(data)));
}

CommandResult CommandResult::nullBulk() {
  return fromValue(RespValue::makeNull());
}

CommandResult CommandResult::queued() {
  return fromValue(RespValue::makeSimpleString(Sds("QUEUED")));
}

CommandResult CommandResult::fromValue(const RespValue& v) {
  CommandResult r;
  r.value = v;
  return r;
}

}  // namespace ledis
