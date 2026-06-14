/**
 * @file test_chain_buffer.cc
 * @brief ChainBuffer：链式追加、跨 chunk 解析、长连接 consume、fd IO
 */
#include "../io/test_common.h"

#include "lemo/buffer/chain_buffer.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

void test_append_consume_across_chunks() {
  lemo::buffer::ChainBuffer buf(8);
  LEMO_CHECK(buf.append("12345678", 8) == 8);
  LEMO_CHECK(buf.append("AB", 2) == 2);
  LEMO_CHECK(buf.readable() == 10);

  char out[16] = {};
  LEMO_CHECK(buf.peek(out, 4) == 4);
  LEMO_CHECK(std::memcmp(out, "1234", 4) == 0);
  LEMO_CHECK(buf.readable() == 10);

  buf.consume(9);
  LEMO_CHECK(buf.readable() == 1);
  LEMO_CHECK(buf.read(out, 1) == 1);
  LEMO_CHECK(out[0] == 'B');
  LEMO_CHECK(buf.empty());
}

void test_find_crlf_across_chunks() {
  lemo::buffer::ChainBuffer buf(4);
  LEMO_CHECK(buf.append("ab", 2) == 2);
  LEMO_CHECK(buf.append("cd\r\n", 4) == 4);
  const uint8_t* p = buf.find("\r\n", 2, 0);
  LEMO_CHECK(p != nullptr);
  LEMO_CHECK(buf.find_byte('d', 0) != nullptr);
}

void test_read_write_fd() {
  int fds[2];
  LEMO_CHECK(pipe(fds) == 0);

  const char* msg = "redis-long-conn";
  LEMO_CHECK(write(fds[1], msg, std::strlen(msg)) == static_cast<ssize_t>(std::strlen(msg)));

  lemo::buffer::ChainBuffer buf(8);
  const ssize_t n = buf.readFd(fds[0], 64);
  LEMO_CHECK(n == static_cast<ssize_t>(std::strlen(msg)));
  LEMO_CHECK(buf.readable() == std::strlen(msg));

  char out[32] = {};
  LEMO_CHECK(buf.read(out, std::strlen(msg)) == std::strlen(msg));
  LEMO_CHECK(std::memcmp(out, msg, std::strlen(msg)) == 0);

  LEMO_CHECK(buf.append(msg, std::strlen(msg)) == std::strlen(msg));
  const ssize_t w = buf.writeFd(fds[1], buf.readable());
  LEMO_CHECK(w == static_cast<ssize_t>(std::strlen(msg)));

  close(fds[0]);
  close(fds[1]);
}

void test_long_connection_simulation() {
  lemo::buffer::ChainBuffer buf(16);
  const char* frame1 = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
  const char* frame2 = "*1\r\n$4\r\nPING\r\n";

  for (int i = 0; i < 100; ++i) {
    LEMO_CHECK(buf.append(frame1, std::strlen(frame1)) == std::strlen(frame1));
    buf.consume(std::strlen(frame1));
    LEMO_CHECK(buf.append(frame2, std::strlen(frame2)) == std::strlen(frame2));
    buf.consume(std::strlen(frame2));
  }
  LEMO_CHECK(buf.empty());
}

}  // namespace

int main() {
  test_append_consume_across_chunks();
  test_find_crlf_across_chunks();
  test_read_write_fd();
  test_long_connection_simulation();
  std::printf("test_chain_buffer: OK\n");
  return 0;
}
