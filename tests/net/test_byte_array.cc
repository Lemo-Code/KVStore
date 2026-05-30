/**
 * @file test_byte_array.cc
 * @brief ByteArray：链式节点、序列化、scatter-gather、Stream 对接
 */
#include "test_common.h"

#include "buffer/byte_array.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

void test_fixed_types_roundtrip() {
  auto ba = std::make_shared<net::ByteArray>();
  ba->writeFint32(42);
  ba->writeFuint16(1000);
  ba->writeStringF32("rpc-payload");
  ba->writeFloat(3.14f);
  ba->writeInt32(-7);

  ba->setPosition(0);
  NET_CHECK(ba->readFint32() == 42);
  NET_CHECK(ba->readFuint16() == 1000);
  NET_CHECK(ba->readStringF32() == "rpc-payload");
  NET_CHECK(std::fabs(ba->readFloat() - 3.14f) < 1e-5f);
  NET_CHECK(ba->readInt32() == -7);
}

void test_chain_across_nodes() {
  net::ByteArray ba(8);
  std::string payload(20, 'z');
  ba.write(payload.data(), payload.size());
  NET_CHECK(ba.getSize() == 20);

  ba.setPosition(0);
  std::string out = ba.toString();
  NET_CHECK(out == payload);
}

void test_read_buffers_span_nodes() {
  net::ByteArray ba(8);
  ba.write("abcdefgh", 8);
  ba.write("ijklmnop", 8);

  std::vector<iovec> iovs;
  const uint64_t n = ba.getReadBuffers(iovs, 16, 0);
  NET_CHECK(n == 16);
  NET_CHECK(iovs.size() == 2);
  NET_CHECK(iovs[0].iov_len == 8);
  NET_CHECK(iovs[1].iov_len == 8);
  NET_CHECK(std::memcmp(iovs[0].iov_base, "abcdefgh", 8) == 0);
  NET_CHECK(std::memcmp(iovs[1].iov_base, "ijklmnop", 8) == 0);
}

void test_write_buffers_fill() {
  net::ByteArray dst(8);
  std::vector<iovec> wiovs;
  const uint64_t want = 12;
  NET_CHECK(dst.getWriteBuffers(wiovs, want) == want);
  NET_CHECK(wiovs.size() == 2);

  const char src[] = "hello-byte-a";
  size_t off = 0;
  for (const iovec& iov : wiovs) {
    std::memcpy(iov.iov_base, src + off, iov.iov_len);
    off += iov.iov_len;
  }
  dst.setPosition(static_cast<size_t>(want));

  dst.setPosition(0);
  NET_CHECK(dst.toString() == "hello-byte-a");
}

void test_file_roundtrip() {
  const std::string path = net_test::LogPath("byte_array.bin");
  net::ByteArray ba;
  ba.writeStringVint("persist-me");
  ba.setPosition(0);
  NET_CHECK(ba.writeToFile(path));

  net::ByteArray loaded;
  NET_CHECK(loaded.readFromFile(path));
  loaded.setPosition(0);
  NET_CHECK(loaded.readStringVint() == "persist-me");
}

}  // namespace

int main() {
  test_fixed_types_roundtrip();
  test_chain_across_nodes();
  test_read_buffers_span_nodes();
  test_write_buffers_fill();
  test_file_roundtrip();
  std::printf("PASS test_byte_array\n");
  return 0;
}
