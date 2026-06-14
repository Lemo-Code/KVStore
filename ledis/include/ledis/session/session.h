#pragma once

#include "ledis/command/command.h"
#include "ledis/command/command_result.h"
#include "ledis/session/command_envelope.h"
#include "ledis/session/session_context.h"
#include "ledis/stream/ledis_stream.h"
#include "ledis/types.h"

#include <cstdint>

namespace ledis {

class LedisEngine;

using CommandHandler = Function<CommandResult(const SessionContext& ctx, const Command& cmd)>;

/**
 * @brief 单连接 IO 会话：读缓冲 + 解析入队 + 写回响应。
 *
 * 异步模式：parse → InboundQueue → DB Worker → ReplyRouter → deliverReply
 * 同步模式：setHandler() 且 engine=nullptr，本地直接 dispatch
 */
class Session {
 public:
  Session(uint64_t conn_id, uint32_t io_thread_id, LedisEngine* engine,
          size_t chunk_size = lemo::buffer::ChainBuffer::kDefaultChunkSize);

  /** 兼容旧测试：无 engine 的同步 Session。 */
  explicit Session(size_t chunk_size = lemo::buffer::ChainBuffer::kDefaultChunkSize);

  uint64_t connId() const { return conn_id_; }
  uint32_t ioThreadId() const { return io_thread_id_; }

  LedisStream& stream() { return stream_; }
  const LedisStream& stream() const { return stream_; }
  SessionContext& context() { return ctx_; }
  const SessionContext& context() const { return ctx_; }

  void setHandler(CommandHandler handler) { sync_handler_ = Move(handler); }
  bool asyncMode() const { return engine_ != nullptr && !sync_handler_; }

  ssize_t readMore(int fd, size_t max_bytes = 65536, int* saved_errno = nullptr);
  size_t parseAndEnqueue(int fd);
  void deliverReply(ReplyEnvelope reply);
  size_t pollReplies(int fd);

  size_t pendingReplyCount() const { return pending_replies_.size(); }
  size_t outstandingCommands() const { return outstanding_commands_; }

  size_t pump(int fd);
  bool readAndPump(int fd, class IoWorker* worker = nullptr);
  bool writeResult(int fd, const CommandResult& result);
  bool writeResults(int fd, const CommandResult& result);

  bool queryBufferExceeded() const { return query_buffer_exceeded_; }
  void clearQueryBufferExceeded() { query_buffer_exceeded_ = false; }

  bool blocked() const { return blocked_; }
  void setBlocked(bool blocked) { blocked_ = blocked; }

 private:
  uint64_t conn_id_ = 0;
  uint32_t io_thread_id_ = 0;
  uint64_t next_seq_ = 1;
  LedisEngine* engine_ = nullptr;
  LedisStream stream_;
  SessionContext ctx_;
  CommandHandler sync_handler_;
  StdDeque<ReplyEnvelope> pending_replies_;
  size_t outstanding_commands_ = 0;
  bool query_buffer_exceeded_ = false;
  bool blocked_ = false;
};

}  // namespace ledis
