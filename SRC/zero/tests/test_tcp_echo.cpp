// test_tcp_echo.cpp — Full echo server stress tests
//
// Tests: start server on random port, spawn multiple clients (10+),
// each sends many messages (100+), verify all echoes correct,
// concurrent clients, large messages (64KB), measure throughput.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <numeric>

using namespace zero;

// Helper: get a random free port
static uint16_t get_random_port() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 20000 + (rand() % 10000);
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
// Echo server stress test infrastructure
// =====================================================================

class EchoServerFixture {
public:
    EchoServerFixture() : port_(get_random_port()), addr_("127.0.0.1", port_) {
        server_.set_connection_callback([this](SocketStream::Ptr conn) {
            handle_connection(conn);
        });
        ASSERT_TRUE(server_.bind(addr_));
        ASSERT_TRUE(server_.start());
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ~EchoServerFixture() {
        server_.stop();
    }

    uint16_t port() const { return port_; }
    const IPv4Address& addr() const { return addr_; }
    size_t total_handled() const { return total_handled_.load(); }
    size_t total_bytes() const { return total_bytes_.load(); }

private:
    void handle_connection(SocketStream::Ptr conn) {
        total_handled_.fetch_add(1);
        // Read all data and echo back
        char buf[65536];
        while (true) {
            ssize_t n = conn->read(buf, sizeof(buf));
            if (n <= 0) break;

            total_bytes_.fetch_add(n);

            ssize_t written = conn->write(buf, n);
            if (written < 0) break;

            conn->flush();
        }
    }

    uint16_t port_;
    IPv4Address addr_;
    TcpServer server_;
    std::atomic<size_t> total_handled_{0};
    std::atomic<size_t> total_bytes_{0};
};

// Helper: connect raw socket, send and receive exact data
class RawClient {
public:
    RawClient(const IPv4Address& addr) {
        sock_ = Socket::create_tcp();
        connected_ = sock_ && sock_->connect(addr);
    }

    ~RawClient() {
        if (sock_) sock_->close();
    }

    bool connected() const { return connected_; }

    bool send_all(const void* data, size_t len) {
        size_t total = 0;
        const char* p = static_cast<const char*>(data);
        while (total < len) {
            ssize_t n = sock_->send(p + total, len - total);
            if (n <= 0) return false;
            total += n;
        }
        return true;
    }

    bool recv_all(void* buf, size_t len) {
        size_t total = 0;
        char* p = static_cast<char*>(buf);
        while (total < len) {
            ssize_t n = sock_->recv(p + total, len - total);
            if (n <= 0) return false;
            total += n;
        }
        return true;
    }

    void close() {
        if (sock_) sock_->close();
    }

private:
    Socket::Ptr sock_;
    bool connected_ = false;
};

// =====================================================================
// Basic echo tests
// =====================================================================

TEST(TcpEchoTest, SingleClientSingleMessage) {
    EchoServerFixture fixture;

    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    const std::string msg = "Hello Echo Server!";
    ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

    std::string response(msg.size(), '\0');
    ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
    EXPECT_EQ(response, msg);
}

TEST(TcpEchoTest, SingleClientMultipleMessages) {
    EchoServerFixture fixture;

    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    for (int i = 0; i < 20; ++i) {
        std::string msg = "message_" + std::to_string(i) + "_padding_to_make_it_longer";
        ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

        std::string response(msg.size(), '\0');
        ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
        EXPECT_EQ(response, msg);
    }
}

// =====================================================================
// Multiple concurrent clients
// =====================================================================

TEST(TcpEchoTest, TenClientsEachHundredMessages) {
    EchoServerFixture fixture;

    const int kNumClients = 10;
    const int kMsgsPerClient = 100;
    std::atomic<int> total_success{0};
    std::atomic<int> total_failures{0};
    std::vector<std::thread> threads;

    for (int c = 0; c < kNumClients; ++c) {
        threads.emplace_back([&fixture, c, kMsgsPerClient, &total_success, &total_failures]() {
            RawClient client(fixture.addr());
            if (!client.connected()) {
                total_failures.fetch_add(kMsgsPerClient);
                return;
            }

            for (int i = 0; i < kMsgsPerClient; ++i) {
                std::string msg = "c" + std::to_string(c) + "_m" +
                                  std::to_string(i) + "_pad1234567890";

                if (!client.send_all(msg.data(), msg.size())) {
                    total_failures.fetch_add(1);
                    continue;
                }

                std::string response(msg.size(), '\0');
                if (!client.recv_all(&response[0], msg.size())) {
                    total_failures.fetch_add(1);
                    continue;
                }

                if (response == msg) {
                    total_success.fetch_add(1);
                } else {
                    total_failures.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_failures.load(), 0);
    EXPECT_EQ(total_success.load(), kNumClients * kMsgsPerClient);
}

// =====================================================================
// Large messages (64KB)
// =====================================================================

TEST(TcpEchoTest, LargeMessage64KB) {
    EchoServerFixture fixture;

    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    std::string large_msg(65536, '\0');
    for (size_t i = 0; i < large_msg.size(); ++i) {
        large_msg[i] = static_cast<char>('A' + (i % 26));
    }

    ASSERT_TRUE(client.send_all(large_msg.data(), large_msg.size()));

    std::string response(large_msg.size(), '\0');
    ASSERT_TRUE(client.recv_all(&response[0], large_msg.size()));
    EXPECT_EQ(response, large_msg);
}

TEST(TcpEchoTest, LargeMessage32KB) {
    EchoServerFixture fixture;

    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    std::string msg(32768, '\0');
    for (size_t i = 0; i < msg.size(); ++i) {
        msg[i] = static_cast<char>('0' + (i % 10));
    }

    ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

    std::string response(msg.size(), '\0');
    ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
    EXPECT_EQ(response, msg);
}

// =====================================================================
// Varying message sizes
// =====================================================================

TEST(TcpEchoTest, VaryingMessageSizes) {
    EchoServerFixture fixture;

    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    std::vector<size_t> sizes = {1, 2, 4, 8, 16, 32, 64, 128, 256,
                                 512, 1024, 2048, 4096, 8192, 16384};

    for (size_t sz : sizes) {
        std::string msg(sz, '\0');
        for (size_t i = 0; i < sz; ++i) {
            msg[i] = static_cast<char>('A' + (i % 26));
        }

        ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

        std::string response(msg.size(), '\0');
        ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
        EXPECT_EQ(response, msg) << "Mismatch at size " << sz;
    }
}

// =====================================================================
// Concurrent clients with varying sizes
// =====================================================================

TEST(TcpEchoTest, ConcurrentClientsVaryingSizes) {
    EchoServerFixture fixture;

    const int kNumClients = 8;
    std::atomic<int> success{0};
    std::vector<std::thread> threads;

    for (int c = 0; c < kNumClients; ++c) {
        threads.emplace_back([&fixture, c, &success]() {
            RawClient client(fixture.addr());
            if (!client.connected()) return;

            std::vector<size_t> sizes = {10, 100, 1000, 5000, 10000};

            for (size_t sz : sizes) {
                std::string msg(sz, '\0');
                for (size_t i = 0; i < sz; ++i) {
                    msg[i] = static_cast<char>((c + i) % 256);
                }

                if (!client.send_all(msg.data(), msg.size())) continue;

                std::string response(msg.size(), '\0');
                if (!client.recv_all(&response[0], msg.size())) continue;

                if (response == msg) {
                    success.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Each client sends 5 messages
    EXPECT_EQ(success.load(), kNumClients * 5);
}

// =====================================================================
// Throughput measurement
// =====================================================================

TEST(TcpEchoTest, ThroughputMeasurement) {
    EchoServerFixture fixture;

    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    const size_t kMsgSize = 8192;
    const int kNumMsgs = 50;
    std::string msg(kMsgSize, 'X');

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < kNumMsgs; ++i) {
        ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

        std::string response(msg.size(), '\0');
        ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
        EXPECT_EQ(response.substr(0, 10), std::string(10, 'X'));
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    size_t total_bytes = kMsgSize * kNumMsgs * 2; // send + receive
    double mbps = (total_bytes / (1024.0 * 1024.0)) /
                  (elapsed_ms / 1000.0);

    // Just log the throughput — no strict assertion
    EXPECT_GT(mbps, 0.0);

    std::cout << "[Throughput] " << kNumMsgs << " messages of "
              << kMsgSize << " bytes each: "
              << total_bytes << " total bytes in " << elapsed_ms << "ms ("
              << mbps << " MB/s)" << std::endl;
}

// =====================================================================
// Sequential client tests (connect, echo, disconnect, repeat)
// =====================================================================

TEST(TcpEchoTest, SequentialClientsRepeatedConnect) {
    EchoServerFixture fixture;

    for (int round = 0; round < 10; ++round) {
        RawClient client(fixture.addr());
        ASSERT_TRUE(client.connected()) << "Connection failed at round " << round;

        std::string msg = "round_" + std::to_string(round);
        ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

        std::string response(msg.size(), '\0');
        ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
        EXPECT_EQ(response, msg);

        // Client disconnects (raw client destructor closes socket)
    }
}

// =====================================================================
// Edge case: empty message
// =====================================================================

TEST(TcpEchoTest, EmptyMessage) {
    EchoServerFixture fixture;

    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    // Sending empty data is essentially a no-op
    ASSERT_TRUE(client.send_all("", 0));
    // Just verify no crash
    SUCCEED();
}

// =====================================================================
// Edge case: many small messages rapidly
// =====================================================================

TEST(TcpEchoTest, RapidSmallMessages) {
    EchoServerFixture fixture;

    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    const int kNumMsgs = 500;
    for (int i = 0; i < kNumMsgs; ++i) {
        std::string msg = "m" + std::to_string(i);
        ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

        std::string response(msg.size(), '\0');
        ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
        EXPECT_EQ(response, msg);
    }
}
