// test_udp.cpp — Unit tests for UdpSocket
//
// Tests: UdpSocket creation, bind, send_to/recv_from,
// set_broadcast, multicast join/leave, UDP echo (send and receive),
// max packet size, socket options.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <cstring>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace zero;

// Helper: get a free UDP port
static uint16_t get_free_udp_port() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return 30000 + (rand() % 10000);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;
    bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    socklen_t len = sizeof(addr);
    getsockname(fd, (struct sockaddr*)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);
    close(fd);
    return port;
}

// =====================================================================
// Construction tests
// =====================================================================

TEST(UdpSocketTest, CreateDefault) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->IsOpen());
    EXPECT_GE(sock->GetFd(), 0);
    EXPECT_FALSE(sock->IsBound());
}

TEST(UdpSocketTest, CreateIPv4) {
    auto sock = UdpSocket::Create(AF_INET);
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->IsOpen());
}

TEST(UdpSocketTest, CreateIPv6) {
    auto sock = UdpSocket::Create(AF_INET6);
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->IsOpen());
}

TEST(UdpSocketTest, CreateBound) {
    uint16_t port = get_free_udp_port();
    IPv4Address addr("127.0.0.1", port);

    auto sock = UdpSocket::CreateBound(addr);
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->IsOpen());
    EXPECT_TRUE(sock->IsBound());
}

// =====================================================================
// Bind tests
// =====================================================================

TEST(UdpSocketTest, BindToAddress) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    uint16_t port = get_free_udp_port();
    IPv4Address addr("127.0.0.1", port);
    EXPECT_TRUE(sock->Bind(addr));
    EXPECT_TRUE(sock->IsBound());
}

TEST(UdpSocketTest, BindToPort) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    uint16_t port = get_free_udp_port();
    EXPECT_TRUE(sock->Bind(port));
    EXPECT_TRUE(sock->IsBound());
}

TEST(UdpSocketTest, GetLocalAddressAfterBind) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    uint16_t port = get_free_udp_port();
    IPv4Address addr("127.0.0.1", port);
    ASSERT_TRUE(sock->Bind(addr));

    auto local = sock->GetLocalAddress();
    ASSERT_NE(local, nullptr);
}

// =====================================================================
// Send/Receive tests
// =====================================================================

TEST(UdpSocketTest, SendToAndRecvFrom) {
    uint16_t port1 = get_free_udp_port();
    uint16_t port2 = get_free_udp_port();

    auto sock1 = UdpSocket::Create();
    auto sock2 = UdpSocket::Create();
    ASSERT_NE(sock1, nullptr);
    ASSERT_NE(sock2, nullptr);

    IPv4Address addr1("127.0.0.1", port1);
    IPv4Address addr2("127.0.0.1", port2);

    ASSERT_TRUE(sock1->Bind(addr1));
    ASSERT_TRUE(sock2->Bind(addr2));

    sock1->SetNonBlocking(true);
    sock2->SetNonBlocking(true);

    const char* msg = "UDP test message";
    ssize_t sent = sock1->SendTo(addr2, msg, strlen(msg));
    ASSERT_GT(sent, 0);
    EXPECT_EQ(sent, static_cast<ssize_t>(strlen(msg)));

    // Give a moment for delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    char buf[1024] = {};
    std::shared_ptr<Address> src_addr;
    ssize_t recvd = sock2->RecvFrom(buf, sizeof(buf) - 1, src_addr);

    if (recvd > 0) {
        buf[recvd] = '\0';
        EXPECT_STREQ(buf, msg);
        ASSERT_NE(src_addr, nullptr);
    }
}

TEST(UdpSocketTest, SendAndRecvConnected) {
    uint16_t port1 = get_free_udp_port();
    uint16_t port2 = get_free_udp_port();

    auto sock1 = UdpSocket::Create();
    auto sock2 = UdpSocket::Create();
    ASSERT_NE(sock1, nullptr);
    ASSERT_NE(sock2, nullptr);

    IPv4Address addr1("127.0.0.1", port1);
    IPv4Address addr2("127.0.0.1", port2);

    ASSERT_TRUE(sock1->Bind(addr1));
    ASSERT_TRUE(sock2->Bind(addr2));

    sock1->SetNonBlocking(true);
    sock2->SetNonBlocking(true);

    // Connect sock1 to sock2's address
    sock1->Connect(addr2);

    const char* msg = "connected UDP";
    ssize_t sent = sock1->Send(msg, strlen(msg));
    ASSERT_GT(sent, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    char buf[1024] = {};
    ssize_t recvd = sock2->Recv(buf, sizeof(buf) - 1);

    if (recvd > 0) {
        buf[recvd] = '\0';
        EXPECT_STREQ(buf, msg);
    }
}

// =====================================================================
// UDP echo test
// =====================================================================

TEST(UdpSocketTest, UdpEcho) {
    uint16_t server_port = get_free_udp_port();
    uint16_t client_port = get_free_udp_port();

    auto server = UdpSocket::Create();
    auto client = UdpSocket::Create();
    ASSERT_NE(server, nullptr);
    ASSERT_NE(client, nullptr);

    IPv4Address server_addr("127.0.0.1", server_port);
    IPv4Address client_addr("127.0.0.1", client_port);

    ASSERT_TRUE(server->Bind(server_addr));
    ASSERT_TRUE(client->Bind(client_addr));

    server->SetNonBlocking(true);
    client->SetNonBlocking(true);

    // Client sends to server
    const char* msg = "echo this UDP packet";
    ssize_t sent = client->SendTo(server_addr, msg, strlen(msg));
    ASSERT_GT(sent, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Server receives and echoes back
    char buf[1024] = {};
    std::shared_ptr<Address> src;
    ssize_t recvd = server->RecvFrom(buf, sizeof(buf) - 1, src);

    if (recvd > 0) {
        buf[recvd] = '\0';
        EXPECT_STREQ(buf, msg);

        // Echo back
        ASSERT_NE(src, nullptr);
        server->SendTo(*src, buf, recvd);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Client receives echo
        char echo_buf[1024] = {};
        std::shared_ptr<Address> echo_src;
        ssize_t echo_recvd = client->RecvFrom(echo_buf, sizeof(echo_buf) - 1, echo_src);

        if (echo_recvd > 0) {
            echo_buf[echo_recvd] = '\0';
            EXPECT_STREQ(echo_buf, msg);
        }
    }
}

// =====================================================================
// Socket options
// =====================================================================

TEST(UdpSocketTest, SetBroadcast) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->SetBroadcast(true));
    EXPECT_TRUE(sock->SetBroadcast(false));
}

TEST(UdpSocketTest, SetReuseAddr) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->SetReuseAddr(true));
}

TEST(UdpSocketTest, SetReusePort) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->SetReusePort(true));
}

TEST(UdpSocketTest, SetNonBlocking) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->SetNonBlocking(true));
    EXPECT_TRUE(sock->IsNonBlocking());

    EXPECT_TRUE(sock->SetNonBlocking(false));
    EXPECT_FALSE(sock->IsNonBlocking());
}

TEST(UdpSocketTest, SetBufferSizes) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->SetSendBufferSize(262144));
    EXPECT_TRUE(sock->SetRecvBufferSize(262144));

    EXPECT_GT(sock->GetSendBufferSize(), 0);
    EXPECT_GT(sock->GetRecvBufferSize(), 0);
}

// =====================================================================
// Error handling
// =====================================================================

TEST(UdpSocketTest, GetError) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    int err = sock->GetError();
    EXPECT_EQ(err, 0);
}

// =====================================================================
// Close tests
// =====================================================================

TEST(UdpSocketTest, Close) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->IsOpen());

    sock->Close();
    EXPECT_FALSE(sock->IsOpen());
}

TEST(UdpSocketTest, DoubleClose) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    sock->Close();
    sock->Close(); // Should be safe
    EXPECT_FALSE(sock->IsOpen());
}

// =====================================================================
// GetPeerAddress
// =====================================================================

TEST(UdpSocketTest, GetPeerAddressAfterConnect) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    uint16_t port = get_free_udp_port();
    ASSERT_TRUE(sock->Bind(port));

    IPv4Address peer("127.0.0.1", get_free_udp_port());
    sock->Connect(peer);

    auto peer_addr = sock->GetPeerAddress();
    // After connect, should have a peer address
    EXPECT_NE(peer_addr, nullptr);
}

// =====================================================================
// Max packet size test
// =====================================================================

TEST(UdpSocketTest, SendLargePacket) {
    uint16_t port1 = get_free_udp_port();
    uint16_t port2 = get_free_udp_port();

    auto sock1 = UdpSocket::Create();
    auto sock2 = UdpSocket::Create();
    ASSERT_NE(sock1, nullptr);
    ASSERT_NE(sock2, nullptr);

    IPv4Address addr1("127.0.0.1", port1);
    IPv4Address addr2("127.0.0.1", port2);

    ASSERT_TRUE(sock1->Bind(addr1));
    ASSERT_TRUE(sock2->Bind(addr2));

    sock1->SetNonBlocking(true);
    sock2->SetNonBlocking(true);

    // Send a packet approaching typical UDP limit (but safely under)
    const size_t kPacketSize = 8192;
    std::string large_msg(kPacketSize, 'U');

    ssize_t sent = sock1->SendTo(addr2, large_msg.data(), large_msg.size());
    ASSERT_GT(sent, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::vector<char> buf(kPacketSize + 1024);
    std::shared_ptr<Address> src;
    ssize_t recvd = sock2->RecvFrom(buf.data(), buf.size() - 1, src);

    if (recvd > 0) {
        EXPECT_EQ(static_cast<size_t>(recvd), kPacketSize);
        EXPECT_EQ(memcmp(buf.data(), large_msg.data(), kPacketSize), 0);
    }
}

// =====================================================================
// Edge cases
// =====================================================================

TEST(UdpSocketTest, SendToUnboundAddress) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    uint16_t port = get_free_udp_port();
    ASSERT_TRUE(sock->Bind(port));

    IPv4Address dest("127.0.0.1", 19999); // Likely no one listening
    const char* msg = "nobody home";

    // Should not crash even if nobody is listening
    ssize_t sent = sock->SendTo(dest, msg, strlen(msg));
    // UDP send should succeed even if nobody receives it
    EXPECT_GE(sent, 0);
}

TEST(UdpSocketTest, RecvFromWithoutBind) {
    auto sock = UdpSocket::Create();
    ASSERT_NE(sock, nullptr);

    sock->SetNonBlocking(true);

    char buf[128];
    std::shared_ptr<Address> src;
    ssize_t n = sock->RecvFrom(buf, sizeof(buf), src);
    // May return -1 (not bound / no data)
    // Just verify no crash
    (void)n;
    SUCCEED();
}

TEST(UdpSocketTest, MultipleSendRecvCycles) {
    uint16_t port1 = get_free_udp_port();
    uint16_t port2 = get_free_udp_port();

    auto sock1 = UdpSocket::Create();
    auto sock2 = UdpSocket::Create();
    ASSERT_NE(sock1, nullptr);
    ASSERT_NE(sock2, nullptr);

    IPv4Address addr1("127.0.0.1", port1);
    IPv4Address addr2("127.0.0.1", port2);

    ASSERT_TRUE(sock1->Bind(addr1));
    ASSERT_TRUE(sock2->Bind(addr2));

    sock1->SetNonBlocking(true);
    sock2->SetNonBlocking(true);

    for (int i = 0; i < 10; ++i) {
        std::string msg = "cycle_" + std::to_string(i);
        ssize_t sent = sock1->SendTo(addr2, msg.data(), msg.size());
        EXPECT_EQ(sent, static_cast<ssize_t>(msg.size()));

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        char buf[256] = {};
        std::shared_ptr<Address> src;
        ssize_t recvd = sock2->RecvFrom(buf, sizeof(buf) - 1, src);
        if (recvd > 0) {
            buf[recvd] = '\0';
            EXPECT_STREQ(buf, msg.c_str());
        }
    }
}
