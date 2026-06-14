#pragma once

#include "ledis/command/command.h"
#include "ledis/protocol/parse_result.h"
#include "ledis/protocol/protocol_limits.h"
#include "ledis/protocol/resp_value.h"

#include "lemo/buffer/chain_buffer.h"

namespace ledis {

/** RESP2 增量解析器（服务端读 Command / 客户端读 Response 共用）。 */
class RespReader {
 public:
  explicit RespReader(lemo::buffer::ChainBuffer& chain,
                      const ProtocolLimits& limits = ProtocolLimits());

  lemo::buffer::ChainBuffer& chain() { return chain_; }
  const lemo::buffer::ChainBuffer& chain() const { return chain_; }

  ParseResult tryParseOne(RespValue* out, size_t* consumed);
  ParseResult tryParseCommand(Command* cmd, size_t* consumed);
  ParseResult parseOne(RespValue* out);
  ParseResult parseCommand(Command* cmd);

 private:
  ParseResult peekValueAt(size_t skip, RespValue* out, size_t* frame_len) const;
  ParseResult valueToCommand(const RespValue& val, Command* cmd) const;
  static void uppercaseInPlace(Sds* s);

  lemo::buffer::ChainBuffer& chain_;
  const ProtocolLimits& limits_;
};

}  // namespace ledis
