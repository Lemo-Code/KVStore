/**
 * @file test_query_buffer.cc
 */
#include "../test_common.h"

#include "ledis/session/session.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

void setNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  LEDIS_CHECK(flags >= 0);
  LEDIS_CHECK(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
}

void test_query_buffer_limit() {
  int sv[2];
  LEDIS_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  setNonBlocking(sv[0]);
  setNonBlocking(sv[1]);

  ledis::Session session;
  session.stream().setQueryBufferLimit(64);

  std::string payload(128, 'x');
  LEDIS_CHECK(write(sv[1], payload.data(), payload.size()) ==
              static_cast<ssize_t>(payload.size()));

  const ssize_t n1 = session.readMore(sv[0]);
  LEDIS_CHECK(n1 == 64);
  LEDIS_CHECK(session.stream().readChain().readable() == 64);

  int err = 0;
  const ssize_t n2 = session.readMore(sv[0], 65536, &err);
  LEDIS_CHECK(n2 < 0);
  LEDIS_CHECK(err == ENOSPC);
  LEDIS_CHECK(session.queryBufferExceeded());

  close(sv[0]);
  close(sv[1]);
}

}  // namespace

int main() {
  test_query_buffer_limit();
  std::printf("test_query_buffer: OK\n");
  return 0;
}
