/**
 * @file test_ring_buffer.cc
 * @brief RingBuffer：环回、scatter-gather、fd IO、协议搜索
 */
#include "test_common.h"

#include "buffer/ring_buffer.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

void test_basic_rw() {
  net::RingBuffer buf(16);
  NET_CHECK(buf.capacity() == 16);
  NET_CHECK(buf.writable() == 16);

  const char* msg = "hello";
  NET_CHECK(buf.write(msg, 5) == 5);
  NET_CHECK(buf.readable() == 5);
  NET_CHECK(buf.writable() == 11);

  char out[8] = {};
  NET_CHECK(buf.read(out, 5) == 5);
  NET_CHECK(std::memcmp(out, "hello", 5) == 0);
  NET_CHECK(buf.empty());
}

void test_wrap_around() {
  net::RingBuffer buf(8);
  NET_CHECK(buf.write("01234567", 8) == 8);
  char drop[6];
  NET_CHECK(buf.read(drop, 6) == 6);

  NET_CHECK(buf.write("ABCD", 4) == 4);
  NET_CHECK(buf.readable() == 6);

  net::BufferRegion regions[2];
  const size_t n = buf.readable_regions(regions, 2, 0);
  NET_CHECK(n == 2);
  NET_CHECK(regions[0].len + regions[1].len == 6);
  NET_CHECK(regions[0].data[0] == '6' && regions[0].data[1] == '7');
  NET_CHECK(regions[1].data[0] == 'A');

  char out[8] = {};
  NET_CHECK(buf.read(out, 6) == 6);
  NET_CHECK(out[0] == '6' && out[1] == '7');
  NET_CHECK(out[2] == 'A' && out[3] == 'B' && out[4] == 'C' && out[5] == 'D');
}

void test_peek_and_consume() {
  net::RingBuffer buf(32);
  NET_CHECK(buf.write("ping", 4) == 4);
  char tmp[4];
  NET_CHECK(buf.peek(tmp, 4) == 4);
  NET_CHECK(buf.readable() == 4);
  NET_CHECK(tmp[0] == 'p');
  buf.consume(2);
  NET_CHECK(buf.readable() == 2);
  NET_CHECK(buf.read(tmp, 2) == 2);
  NET_CHECK(tmp[0] == 'n' && tmp[1] == 'g');
}

void test_find_across_wrap() {
  net::RingBuffer buf(8);
  NET_CHECK(buf.write("abcde", 5) == 5);
  char drop[2];
  NET_CHECK(buf.read(drop, 2) == 2);
  NET_CHECK(buf.write("fg\r\nhi", 6) == 6);

  const uint8_t* p = buf.find("\r\n", 2, 0);
  NET_CHECK(p != nullptr);
  NET_CHECK(buf.find_byte('h', 0) != nullptr);
}

void test_read_write_fd() {
  int fds[2];
  NET_CHECK(::pipe(fds) == 0);

  net::RingBuffer wbuf(64);
  NET_CHECK(wbuf.write("stream-data", 11) == 11);
  NET_CHECK(wbuf.writeFd(fds[1]) == 11);

  net::RingBuffer rbuf(64);
  NET_CHECK(rbuf.readFd(fds[0]) == 11);
  NET_CHECK(rbuf.readable() == 11);

  char out[16] = {};
  NET_CHECK(rbuf.read(out, 11) == 11);
  NET_CHECK(std::memcmp(out, "stream-data", 11) == 0);

  ::close(fds[0]);
  ::close(fds[1]);
}

void test_grow() {
  net::RingBuffer buf(8);
  std::string payload(20, 'x');
  NET_CHECK(buf.write(payload.data(), payload.size()) == payload.size());
  NET_CHECK(buf.capacity() >= 32);
  NET_CHECK(buf.readable() == 20);
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
