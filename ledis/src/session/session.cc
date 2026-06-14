#include "ledis/session/session.h"

#include "ledis/protocol/resp_writer.h"
#include "ledis/session/io_worker.h"
#include "ledis/session/ledis_engine.h"
#include "ledis/types.h"

#include <cerrno>

namespace ledis {

Session::Session(uint64_t conn_id, uint32_t io_thread_id, LedisEngine* engine,
                 size_t chunk_size)
    : conn_id_(conn_id),
      io_thread_id_(io_thread_id),
      engine_(engine),
      stream_(chunk_size) {}

Session::Session(size_t chunk_size) : stream_(chunk_size) {}

ssize_t Session::readMore(int fd, size_t max_bytes, int* saved_errno) {
  int local_errno = 0;
  int* err = saved_errno ? saved_errno : &local_errno;
  const ssize_t n = stream_.readMore(fd, max_bytes, err);
  if (n < 0 && *err == ENOSPC) {
    query_buffer_exceeded_ = true;
  }
  return n;
}

size_t Session::parseAndEnqueue(int fd) {
  size_t count = 0;
  if (blocked_) {
    return count;
  }
  for (;;) {
    Command cmd;
    const ParseResult pr = stream_.tryReadCommand(&cmd);
    if (pr == ParseResult::kNeedMore) {
      break;
    }
    if (pr == ParseResult::kProtocolError) {
      writeResult(fd, CommandResult::error("ERR protocol error"));
      break;
    }

    if (!asyncMode()) {
      CommandResult result = CommandResult::error("ERR no handler");
      if (sync_handler_) {
        result = sync_handler_(ctx_, cmd);
      } else if (engine_) {
        result = engine_->dispatchSync(ctx_, cmd);
      }
      if (result.blocked) {
        blocked_ = true;
        break;
      }
      if (!writeResults(fd, result)) {
        break;
      }
      ++count;
      continue;
    }

    CommandEnvelope env;
    env.conn_id = conn_id_;
    env.io_thread_id = io_thread_id_;
    env.seq = next_seq_++;
    env.ctx = ctx_;
    env.cmd = Move(cmd);
    if (!engine_->inbound().tryPush(Move(env))) {
      writeResult(fd, CommandResult::error("ERR command queue full"));
      break;
    }
    ++outstanding_commands_;
    ++count;
  }
  return count;
}

void Session::deliverReply(ReplyEnvelope reply) {
  if (reply.conn_id != conn_id_) {
    return;
  }
  if (!reply.unsolicited) {
    ctx_ = Move(reply.ctx);
    if (outstanding_commands_ > 0) {
      --outstanding_commands_;
    }
    blocked_ = false;
  }
  pending_replies_.push_back(Move(reply));
}

size_t Session::pollReplies(int fd) {
  size_t count = 0;
  while (!pending_replies_.empty()) {
    const CommandResult& result = pending_replies_.front().result;
    if (!writeResults(fd, result)) {
      break;
    }
    pending_replies_.pop_front();
    ++count;
  }
  return count;
}

size_t Session::pump(int fd) {
  return parseAndEnqueue(fd);
}

bool Session::readAndPump(int fd, IoWorker* worker) {
  const ssize_t n = stream_.readMore(fd);
  if (n <= 0) {
    return false;
  }
  parseAndEnqueue(fd);
  if (worker) {
    worker->pollReplies();
  }
  pollReplies(fd);
  return true;
}

bool Session::writeResult(int fd, const CommandResult& result) {
  return writeResults(fd, result);
}

bool Session::writeResults(int fd, const CommandResult& result) {
  const String bytes = RespWriter::encode(result.value);
  if (LedisStream::writeBytes(fd, bytes) != static_cast<ssize_t>(bytes.size())) {
    return false;
  }
  for (size_t i = 0; i < result.trailing.size(); ++i) {
    const String extra = RespWriter::encode(result.trailing[i]);
    if (LedisStream::writeBytes(fd, extra) != static_cast<ssize_t>(extra.size())) {
      return false;
    }
  }
  return true;
}

}  // namespace ledis
