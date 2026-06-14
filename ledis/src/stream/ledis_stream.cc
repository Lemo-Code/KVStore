#include "ledis/stream/ledis_stream.h"
#include "ledis/types.h"

#include <algorithm>
#include <cerrno>
#include <unistd.h>

namespace ledis {

LedisStream::LedisStream(size_t chunk_size)
    : read_chain_(chunk_size), reader_(read_chain_, limits_) {}

ssize_t LedisStream::readMore(int fd, size_t max_bytes, int* saved_errno) {
  if (limits_.query_buffer_limit > 0 &&
      read_chain_.readable() >= limits_.query_buffer_limit) {
    if (saved_errno) {
      *saved_errno = ENOSPC;
    }
    return -1;
  }
  const size_t cap =
      limits_.query_buffer_limit > 0
          ? std::min(max_bytes, limits_.query_buffer_limit - read_chain_.readable())
          : max_bytes;
  if (cap == 0) {
    if (saved_errno) {
      *saved_errno = ENOSPC;
    }
    return -1;
  }
  return read_chain_.readFd(fd, cap, saved_errno);
}

ParseResult LedisStream::tryReadCommand(Command* out) {
  size_t consumed = 0;
  const ParseResult pr = reader_.tryParseCommand(out, &consumed);
  if (pr == ParseResult::kOk) {
    read_chain_.consume(consumed);
  }
  return pr;
}

ParseResult LedisStream::tryReadResponse(CommandResult* out) {
  if (!out) {
    return ParseResult::kProtocolError;
  }
  size_t consumed = 0;
  const ParseResult pr = reader_.tryParseOne(&out->value, &consumed);
  if (pr == ParseResult::kOk) {
    read_chain_.consume(consumed);
  }
  return pr;
}

ssize_t LedisStream::writeBytes(int fd, const String& bytes,
                                int* saved_errno) {
  if (bytes.empty()) {
    return 0;
  }
  size_t sent = 0;
  while (sent < bytes.size()) {
    const ssize_t n =
        ::write(fd, bytes.data() + sent, bytes.size() - sent);
    if (n < 0) {
      if (saved_errno) {
        *saved_errno = errno;
      }
      return n;
    }
    if (n == 0) {
      break;
    }
    sent += static_cast<size_t>(n);
  }
  return static_cast<ssize_t>(sent);
}

void LedisStream::clear() {
  read_chain_.clear();
}

}  // namespace ledis
