/**
 * @file test_byte_array.cc
 * @brief ByteArray：链式节点、序列化、scatter-gather。
 */
#include "../io/test_common.h"

#include "lemo/buffer/byte_array.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {

void test_fixed_types_roundtrip() {
  auto ba = std::make_shared<lemo::buffer::ByteArray>();
  ba->writeFint32(42);
  ba->writeFuint16(1000);
  ba->writeStringF32("rpc-payload");
  ba->writeFloat(3.14f);
  ba->writeInt32(-7);

  ba->setPosition(0);
  LEMO_CHECK(ba->readFint32() == 42);
  LEMO_CHECK(ba->readFuint16() == 1000);
  LEMO_CHECK(ba->readStringF32() == "rpc-payload");
  LEMO_CHECK(std::fabs(ba->readFloat() - 3.14f) < 1e-5f);
  LEMO_CHECK(ba->readInt32() == -7);
}

void test_chain_across_nodes() {
  lemo::buffer::ByteArray ba(8);
  std::string payload(20, 'z');
  ba.write(payload.data(), payload.size());
  LEMO_CHECK(ba.getSize() == 20);

  ba.setPosition(0);
  std::string out = ba.toString();
  LEMO_CHECK(out == payload);
}

void test_scatter_gather_buffers() {
  lemo::buffer::ByteArray ba(16);
  ba.write("hello", 5);
  ba.setPosition(0);
  std::vector<iovec> iovs;
  const uint64_t n = ba.getReadBuffers(iovs, 5);
  LEMO_CHECK(n == 5);
  LEMO_CHECK(!iovs.empty());
}

}  // namespace

int main() {
  test_fixed_types_roundtrip();
  test_chain_across_nodes();
  test_scatter_gather_buffers();
  std::printf("PASS test_byte_array\n");
  return 0;
}
