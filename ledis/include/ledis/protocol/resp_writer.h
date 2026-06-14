#pragma once

#include "ledis/command/command.h"
#include "ledis/command/command_result.h"
#include "ledis/protocol/resp_value.h"
#include "ledis/types.h"

namespace ledis {

/** RESP2 编码器（服务端 Response / 客户端 Request 共用）。 */
class RespWriter {
 public:
  static String encode(const RespValue& val);
  static String encodeSimpleString(const Sds& s);
  static String encodeError(const Sds& msg);
  static String encodeError(const char* msg);
  static String encodeInteger(int64_t n);
  static String encodeBulk(const Sds& s);
  static String encodeNullBulk();
  static String encodeArray(const RespArray& elems);
  static String encodeCommand(const SdsArgList& argv);
  static String encodeCommand(const Command& cmd);
};

}  // namespace ledis
