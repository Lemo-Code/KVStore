#pragma once

#include "lemo/buffer/chain_buffer.h"
#include "ledis/command/command.h"
#include "ledis/command/command_result.h"
#include "ledis/protocol/parse_result.h"
#include "ledis/protocol/protocol_limits.h"
#include "ledis/protocol/resp_reader.h"

#include "ledis/types.h"

#include <cstddef>
#include <sys/types.h>

namespace ledis {

/**
 * @brief 单连接字节流：读侧 ChainBuffer + RESP 增量解析。
 *
 * 长连接上所有未解析字节驻留 read_chain_；Command/CommandResult 编解码均基于此缓冲。
 */
class LedisStream {
 public:
  explicit LedisStream(size_t chunk_size = lemo::buffer::ChainBuffer::kDefaultChunkSize);

  lemo::buffer::ChainBuffer& readChain() { return read_chain_; }
  const lemo::buffer::ChainBuffer& readChain() const { return read_chain_; }

  void setQueryBufferLimit(size_t limit) { limits_.query_buffer_limit = limit; }
  size_t queryBufferLimit() const { return limits_.query_buffer_limit; }

  ssize_t readMore(int fd, size_t max_bytes = 65536, int* saved_errno = nullptr);

  /** 服务端：从缓冲解析一条 Command。 */
  ParseResult tryReadCommand(Command* out);
  /** 客户端：从缓冲解析一条 Response（RespValue）。 */
  ParseResult tryReadResponse(CommandResult* out);

  static ssize_t writeBytes(int fd, const String& bytes,
                            int* saved_errno = nullptr);

  void clear();

 private:
  lemo::buffer::ChainBuffer read_chain_;
  ProtocolLimits limits_;
  RespReader reader_;
};

}  // namespace ledis
