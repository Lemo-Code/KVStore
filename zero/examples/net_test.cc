// Network layer test: Buffer + Address + Socket + Stream
#include "zero/net/buffer.h"
#include "zero/net/address.h"
#include "zero/net/socket.h"
#include "zero/net/socket_stream.h"

#include <cstdio>
#include <cassert>

void test_buffer() {
    printf("--- Buffer test ---\n");

    zero::ByteBuffer buf(256);

    // 写入定长整数
    buf.writeFInt32(42);
    buf.writeFInt64(1234567890123);
    buf.writeFUInt16(65535);

    assert(buf.getSize() == 14);  // 4 + 8 + 2
    assert(buf.getPosition() == 14);

    // 重置位置读取
    buf.setPosition(0);

    assert(buf.readFInt32() == 42);
    assert(buf.readFInt64() == 1234567890123);
    assert(buf.readFUInt16() == 65535);

    // 写入字符串
    buf.clear();
    buf.writeStringF16("hello world");
    buf.setPosition(0);
    assert(buf.readStringF16() == "hello world");

    // Varint
    buf.clear();
    buf.writeUInt32(300);
    buf.writeUInt64(1000000);
    buf.setPosition(0);
    assert(buf.readUInt32() == 300);
    assert(buf.readUInt64() == 1000000);

    // 浮点
    buf.clear();
    buf.writeDouble(3.14159);
    buf.setPosition(0);
    double v = buf.readDouble();
    assert(v > 3.14 && v < 3.15);

    // iovec (零拷贝)
    buf.clear();
    for (int i = 0; i < 10; ++i) buf.writeFUInt32(i);
    buf.setPosition(0);

    std::vector<iovec> iovs;
    uint64_t total = buf.getReadBuffers(iovs);
    assert(total == 40);

    printf("Buffer: PASSED\n");
}

void test_address() {
    printf("--- Address test ---\n");

    // IPv4
    auto ipv4 = zero::IPv4Address::Create("192.168.1.1", 8080);
    assert(ipv4);
    assert(ipv4->getPort() == 8080);
    assert(ipv4->getIPString() == "192.168.1.1");
    assert(ipv4->getFamily() == AF_INET);

    std::string s = ipv4->toString();
    assert(s.find("192.168.1.1") != std::string::npos);
    assert(s.find("8080") != std::string::npos);
    printf("  IPv4: %s\n", s.c_str());

    // IPv6
    auto ipv6 = zero::IPv6Address::Create("::1", 9090);
    assert(ipv6);
    assert(ipv6->getPort() == 9090);
    assert(ipv6->getFamily() == AF_INET6);
    printf("  IPv6: %s\n", ipv6->toString().c_str());

    // 子网
    auto net = ipv4->networkAddress(24);
    assert(net->getIPString() == "192.168.1.0");
    printf("  Network: %s\n", net->toString().c_str());

    auto bcast = ipv4->broadcastAddress(24);
    assert(bcast->getIPString() == "192.168.1.255");
    printf("  Broadcast: %s\n", bcast->toString().c_str());

    // Create from sockaddr
    auto addr2 = zero::Address::Create(ipv4->getAddr(), ipv4->getAddrLen());
    assert(addr2);
    assert(addr2->toString() == ipv4->toString());

    printf("Address: PASSED\n");
}

void test_socket() {
    printf("--- Socket test ---\n");

    // 创建 TCP socket
    auto sock = zero::Socket::CreateTCPSocket();
    assert(sock);
    assert(!sock->isClosed());
    assert(sock->isValid());
    printf("  Socket fd=%d\n", sock->getSocket());

    // 绑定
    auto addr = zero::IPv4Address::Create("0.0.0.0", 0);
    assert(sock->bind(addr));
    printf("  Bound\n");

    // 获取本地地址
    auto local = sock->getLocalAddress();
    assert(local);
    printf("  Local: %s\n", local->toString().c_str());

    sock->close();
    assert(sock->isClosed());
    printf("Socket: PASSED\n");
}

int main() {
    printf("=== Net Layer Test ===\n");
    test_buffer();
    test_address();
    test_socket();
    printf("=== All Net tests PASSED ===\n");
    return 0;
}
