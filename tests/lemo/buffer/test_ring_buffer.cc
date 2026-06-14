/**
 * @file test_ring_buffer.cc
 * @brief RingBuffer：环回、scatter-gather、fd IO、协议搜索
 */
#include "../io/test_common.h"

#include "lemo/buffer/ring_buffer.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

void test_basic_rw() {
  lemo::buffer::RingBuffer buf(16);
  LEMO_CHECK(buf.capacity() == 16);
  LEMO_CHECK(buf.writable() == 16);

  const char* msg = "hello";
  LEMO_CHECK(buf.write(msg, 5) == 5);
  LEMO_CHECK(buf.readable() == 5);
  LEMO_CHECK(buf.writable() == 11);

  char out[8] = {};
  LEMO_CHECK(buf.read(out, 5) == 5);
  LEMO_CHECK(std::memcmp(out, "hello", 5) == 0);
  LEMO_CHECK(buf.empty());
}

void test_wrap_around() {
  lemo::buffer::RingBuffer buf(8);
  LEMO_CHECK(buf.write("01234567", 8) == 8);
  char drop[6];
  LEMO_CHECK(buf.read(drop, 6) == 6);

  LEMO_CHECK(buf.write("ABCD", 4) == 4);
  LEMO_CHECK(buf.readable() == 6);

  lemo::buffer::BufferRegion regions[2];
  const size_t n = buf.readable_regions(regions, 2, 0);
  LEMO_CHECK(n == 2);
  LEMO_CHECK(regions[0].len + regions[1].len == 6);
  LEMO_CHECK(regions[0].data[0] == '6' && regions[0].data[1] == '7');
  LEMO_CHECK(regions[1].data[0] == 'A');

  char out[8] = {};
  LEMO_CHECK(buf.read(out, 6) == 6);
  LEMO_CHECK(out[0] == '6' && out[1] == '7');
  LEMO_CHECK(out[2] == 'A' && out[3] == 'B' && out[4] == 'C' && out[5] == 'D');
}

void test_peek_and_consume() {
  lemo::buffer::RingBuffer buf(32);
  LEMO_CHECK(buf.write("ping", 4) == 4);
  char tmp[4];
  LEMO_CHECK(buf.peek(tmp, 4) == 4);
  LEMO_CHECK(buf.readable() == 4);
  LEMO_CHECK(tmp[0] == 'p');
  buf.consume(2);
  LEMO_CHECK(buf.readable() == 2);
  LEMO_CHECK(buf.read(tmp, 2) == 2);
  LEMO_CHECK(tmp[0] == 'n' && tmp[1] == 'g');
}

void test_find_across_wrap() {
  lemo::buffer::RingBuffer buf(8);
  LEMO_CHECK(buf.write("abcde", 5) == 5);
  char drop[2];
  LEMO_CHECK(buf.read(drop, 2) == 2);
  LEMO_CHECK(buf.write("fg\r\nhi", 6) == 6);

  const uint8_t* p = buf.find("\r\n", 2, 0);
  LEMO_CHECK(p != nullptr);
  LEMO_CHECK(buf.find_byte('h', 0) != nullptr);
}

void test_read_write_fd() {
  int fds[2];
  LEMO_CHECK(::pipe(fds) == 0);

  lemo::buffer::RingBuffer wbuf(64);
  LEMO_CHECK(wbuf.write("stream-data", 11) == 11);
  LEMO_CHECK(wbuf.writeFd(fds[1]) == 11);

  lemo::buffer::RingBuffer rbuf(64);
  LEMO_CHECK(rbuf.readFd(fds[0]) == 11);
  LEMO_CHECK(rbuf.readable() == 11);

  char out[16] = {};
  LEMO_CHECK(rbuf.read(out, 11) == 11);
  LEMO_CHECK(std::memcmp(out, "stream-data", 11) == 0);

  ::close(fds[0]);
  ::close(fds[1]);
}

void test_grow() {
  lemo::buffer::RingBuffer buf(8);
  std::string payload(20, 'x');
  LEMO_CHECK(buf.write(payload.data(), payload.size()) == payload.size());
  LEMO_CHECK(buf.capacity() >= 32);
  LEMO_CHECK(buf.readable() == 20);
}

}  // namespace

int main() {
  test_basic_rw();
  test_wrap_around();
  test_peek_and_consume();
  test_find_across_wrap();
  test_read_write_fd();
  test_grow();
  std::printf("PASS test_ring_buffer\n");
  return 0;
}
