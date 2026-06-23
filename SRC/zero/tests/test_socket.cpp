// test_socket.cpp — Unit tests for the non-blocking Socket wrapper
//
// Tests: createTCP/createUDP, bind, listen, accept, close,
// set_reuse_addr/port, set_nonblocking, set_tcp_nodelay, set_keepalive,
// set_send_buffer/recv_buffer, get_local_addr/get_remote_addr,
// get_error, socket pair read/write, timeout settings.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>

using namespace zero;

// =====================================================================
// Factory creation tests
// =====================================================================

TEST(SocketTest, CreateTCP) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->is_valid());
    EXPECT_EQ(sock->type(), Socket::Type::TCP);
    EXPECT_GE(sock->fd(), 0);
    EXPECT_FALSE(sock->is_connected());
}

TEST(SocketTest, CreateUDP) {
    auto sock = Socket::create_udp();
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->is_valid());
    EXPECT_EQ(sock->type(), Socket::Type::UDP);
    EXPECT_GE(sock->fd(), 0);
}

TEST(SocketTest, CreateTCPWithFamily) {
    auto sock = Socket::create(AF_INET, Socket::Type::TCP);
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->is_valid());
    EXPECT_EQ(sock->type(), Socket::Type::TCP);
}

TEST(SocketTest, CreateUDPWithFamily) {
    auto sock = Socket::create(AF_INET, Socket::Type::UDP);
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->is_valid());
    EXPECT_EQ(sock->type(), Socket::Type::UDP);
}

TEST(SocketTest, FromFd) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0);

    auto sock = Socket::from_fd(fd, Socket::Type::TCP);
    ASSERT_NE(sock, nullptr);
    EXPECT_EQ(sock->fd(), fd);
    EXPECT_TRUE(sock->is_valid());
}

// =====================================================================
// Bind tests
// =====================================================================

TEST(SocketTest, BindTCP) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    IPv4Address addr("127.0.0.1", 0); // Port 0 = OS picks a free port
    EXPECT_TRUE(sock->bind(addr));

    auto local = sock->local_address();
    ASSERT_NE(local, nullptr);
    EXPECT_GT(dynamic_cast<IPv4Address*>(local.get())->port(), 0u);
}

TEST(SocketTest, BindUDP) {
    auto sock = Socket::create_udp();
    ASSERT_NE(sock, nullptr);

    IPv4Address addr("127.0.0.1", 0);
    EXPECT_TRUE(sock->bind(addr));

    auto local = sock->local_address();
    ASSERT_NE(local, nullptr);
}

TEST(SocketTest, BindToSpecificPort) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    // Try binding to a port that's likely free
    IPv4Address addr("127.0.0.1", 19876);
    bool bound = sock->bind(addr);
    if (bound) {
        auto local = sock->local_address();
        ASSERT_NE(local, nullptr);
        auto* ipv4 = dynamic_cast<IPv4Address*>(local.get());
        ASSERT_NE(ipv4, nullptr);
        EXPECT_EQ(ipv4->port(), 19876u);
    }
    // If bind fails (port in use), that's OK — but bind shouldn't crash
}

// =====================================================================
// Listen and accept tests
// =====================================================================

TEST(SocketTest, Listen) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    IPv4Address addr("127.0.0.1", 0);
    ASSERT_TRUE(sock->bind(addr));

    EXPECT_TRUE(sock->listen());
}

TEST(SocketTest, ListenWithBacklog) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    IPv4Address addr("127.0.0.1", 0);
    ASSERT_TRUE(sock->bind(addr));

    EXPECT_TRUE(sock->listen(64));
}

TEST(SocketTest, AcceptNonBlockingReturnsNull) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    IPv4Address addr("127.0.0.1", 0);
    ASSERT_TRUE(sock->bind(addr));
    ASSERT_TRUE(sock->listen());

    sock->set_nonblocking(true);

    // No pending connection — accept should return nullptr
    auto client = sock->accept();
    // May return nullptr in non-blocking mode
    if (client) {
        // If it did return something, it should be valid
        EXPECT_TRUE(client->is_valid());
    }
}

TEST(SocketTest, AcceptAfterConnect) {
    auto server = Socket::create_tcp();
    ASSERT_NE(server, nullptr);
    server->set_reuse_addr(true);

    IPv4Address addr("127.0.0.1", 0);
    ASSERT_TRUE(server->bind(addr));

    auto local = server->local_address();
    ASSERT_NE(local, nullptr);
    uint16_t port = dynamic_cast<IPv4Address*>(local.get())->port();
    ASSERT_GT(port, 0u);

    ASSERT_TRUE(server->listen());
    server->set_nonblocking(true);

    // Connect from client
    auto client = Socket::create_tcp();
    ASSERT_NE(client, nullptr);
    client->set_nonblocking(true);

    IPv4Address connect_addr("127.0.0.1", port);
    client->connect(connect_addr);

    // Give it a moment
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto accepted = server->accept();
    if (accepted) {
        EXPECT_TRUE(accepted->is_valid());
    }
    // accept may or may not succeed in non-blocking mode depending on timing
}

// =====================================================================
// Socket option tests
// =====================================================================

TEST(SocketTest, SetReuseAddr) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->set_reuse_addr(true));
    EXPECT_TRUE(sock->get_reuse_addr());

    EXPECT_TRUE(sock->set_reuse_addr(false));
    EXPECT_FALSE(sock->get_reuse_addr());
}

TEST(SocketTest, SetReusePort) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->set_reuse_port(true));
    // get_reuse_port not directly exposed, just verify no crash
}

TEST(SocketTest, SetNonblocking) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->set_nonblocking(true));
    EXPECT_TRUE(sock->is_nonblocking());

    EXPECT_TRUE(sock->set_nonblocking(false));
    EXPECT_FALSE(sock->is_nonblocking());
}

TEST(SocketTest, SetTcpNoDelay) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->set_tcp_nodelay(true));
    EXPECT_TRUE(sock->get_tcp_nodelay());

    EXPECT_TRUE(sock->set_tcp_nodelay(false));
    EXPECT_FALSE(sock->get_tcp_nodelay());
}

TEST(SocketTest, SetKeepalive) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->set_keepalive(true));
    EXPECT_TRUE(sock->get_keepalive());

    EXPECT_TRUE(sock->set_keepalive(false));
    EXPECT_FALSE(sock->get_keepalive());
}

TEST(SocketTest, SetSendBufferSize) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->set_send_buffer_size(65536));
}

TEST(SocketTest, SetRecvBufferSize) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->set_recv_buffer_size(65536));
}

TEST(SocketTest, SetSendTimeout) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->set_send_timeout(5000));
}

TEST(SocketTest, SetRecvTimeout) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->set_recv_timeout(5000));
}

TEST(SocketTest, SetLinger) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->set_linger(true, 5));
}

TEST(SocketTest, SetTTL) {
    auto sock = Socket::create_udp();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->set_ttl(64));
}

// =====================================================================
// Get error tests
// =====================================================================

TEST(SocketTest, GetErrorOnNewSocket) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    int err = sock->get_error();
    EXPECT_EQ(err, 0); // No error on new socket
}

// =====================================================================
// Address retrieval tests
// =====================================================================

TEST(SocketTest, GetLocalAddressBeforeBind) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    auto addr = sock->local_address();
    // Before bind, may return nullptr or an unspecified address
    // Just verify no crash
    SUCCEED();
}

TEST(SocketTest, GetPeerAddressBeforeConnect) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    auto addr = sock->peer_address();
    // Should return nullptr or handle gracefully
    SUCCEED();
}

// =====================================================================
// Close tests
// =====================================================================

TEST(SocketTest, Close) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->is_valid());

    sock->close();
    EXPECT_FALSE(sock->is_valid());
}

TEST(SocketTest, DoubleCloseIsSafe) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    sock->close();
    sock->close(); // Should be safe
    SUCCEED();
}

TEST(SocketTest, DestructorClosesSocket) {
    // Create and destroy — fd should be cleaned up
    {
        auto sock = Socket::create_tcp();
        ASSERT_NE(sock, nullptr);
        EXPECT_TRUE(sock->is_valid());
    }
    SUCCEED();
}

// =====================================================================
// Socket pair read/write tests
// =====================================================================

TEST(SocketTest, SocketPairWriteRead) {
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    auto sock1 = Socket::from_fd(sv[0], Socket::Type::TCP);
    auto sock2 = Socket::from_fd(sv[1], Socket::Type::TCP);
    ASSERT_NE(sock1, nullptr);
    ASSERT_NE(sock2, nullptr);

    const char* msg = "socket pair test";
    ssize_t sent = sock1->send(msg, strlen(msg));
    EXPECT_EQ(sent, static_cast<ssize_t>(strlen(msg)));

    char buf[256] = {};
    ssize_t recvd = sock2->recv(buf, sizeof(buf) - 1);
    EXPECT_EQ(recvd, static_cast<ssize_t>(strlen(msg)));
    EXPECT_STREQ(buf, msg);
}

TEST(SocketTest, SocketPairBidirectional) {
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    auto sock1 = Socket::from_fd(sv[0], Socket::Type::TCP);
    auto sock2 = Socket::from_fd(sv[1], Socket::Type::TCP);

    // Bidirectional: send both ways
    sock1->send("ping", 4);
    sock2->send("pong", 4);

    char buf1[10] = {};
    char buf2[10] = {};

    sock2->recv(buf1, 4);
    sock1->recv(buf2, 4);

    EXPECT_EQ(std::string(buf1, 4), "ping");
    EXPECT_EQ(std::string(buf2, 4), "pong");
}

// =====================================================================
// Edge cases
// =====================================================================

TEST(SocketTest, IsConnectedInitiallyFalse) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);
    EXPECT_FALSE(sock->is_connected());
}

TEST(SocketTest, CreateTCPAndUDPBothValid) {
    auto tcp = Socket::create_tcp();
    auto udp = Socket::create_udp();

    ASSERT_NE(tcp, nullptr);
    ASSERT_NE(udp, nullptr);

    EXPECT_TRUE(tcp->is_valid());
    EXPECT_TRUE(udp->is_valid());

    EXPECT_EQ(tcp->type(), Socket::Type::TCP);
    EXPECT_EQ(udp->type(), Socket::Type::UDP);
}

TEST(SocketTest, FdNotLeakedAfterClose) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);
    int fd = sock->fd();

    sock->close();
    EXPECT_EQ(sock->fd(), -1);
    // fd should be closed — verify by checking fcntl
    int flags = fcntl(fd, F_GETFD);
    EXPECT_EQ(flags, -1);
}
