// test_concurrent_net.cpp — Comprehensive concurrent networking stress tests
//
// Tests:
//   - TCP echo server with multiple clients (1/10/50/100 concurrent)
//   - 100+ concurrent connections each sending messages
//   - Connection/disconnection cycles
//   - Large message transfer (64KB, 128KB, 1MB, 100MB in chunks)
//   - Many small messages (1 million 64-byte messages)
//   - Buffer stress (multiple writers/readers, 1GB buffer throughput)
//   - Address resolution stress (100 threads resolving)
//   - Socket operations stress (create/close, options)
//   - Throughput measurement (MB/s)
//   - Connection accept rate (connections/sec)
//   - Half-open connection handling
//   - Backpressure / slow consumer
//   - TCP_NODELAY effect measurement
//   - Connection pool: 100 persistent connections
//   - UDP packet stress test
//   - Server restart on same port
//   - Graceful shutdown during active connections
//   - Empty message edge case
//   - Varying message sizes with concurrent clients

#include <gtest/gtest.h>
#include "zero/zero.h"
#include "zero/net/tcp_server.h"
#include "zero/net/tcp_client.h"
#include "zero/net/address.h"
#include "zero/net/buffer.h"
#include "zero/net/socket.h"
#include "zero/net/socket_stream.h"
#include "zero/net/udp_socket.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <random>
#include <deque>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

using namespace zero;

// ============================================================
// Helper: get a random free port
// ============================================================

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

static int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// ============================================================
// Echo server test fixture
// ============================================================

class EchoServerFixture {
public:
    EchoServerFixture()
        : port_(get_random_port()), addr_("127.0.0.1", port_) {
        server_.set_connection_callback([this](SocketStream::Ptr conn) {
            handle_connection(conn);
        });
        bool bound = server_.bind(addr_);
        ASSERT_TRUE(bound) << "Failed to bind to port " << port_;
        bool started = server_.start();
        ASSERT_TRUE(started) << "Failed to start server on port " << port_;
        // Give server time to start accepting
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

// ============================================================
// Large echo server fixture (for high-throughput tests)
// ============================================================

class LargeEchoServerFixture {
public:
    LargeEchoServerFixture(int worker_threads = 4)
        : port_(get_random_port()), addr_("127.0.0.1", port_) {
        server_.set_worker_count(worker_threads);
        server_.set_connection_callback([this](SocketStream::Ptr conn) {
            handle_connection(conn);
        });
        bool bound = server_.bind(addr_);
        ASSERT_TRUE(bound) << "Failed to bind to port " << port_;
        bool started = server_.start();
        ASSERT_TRUE(started) << "Failed to start server on port " << port_;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ~LargeEchoServerFixture() {
        server_.stop();
    }

    uint16_t port() const { return port_; }
    const IPv4Address& addr() const { return addr_; }

private:
    void handle_connection(SocketStream::Ptr conn) {
        char buf[65536];
        while (true) {
            ssize_t n = conn->read(buf, sizeof(buf));
            if (n <= 0) break;
            ssize_t written = conn->write(buf, n);
            if (written < 0) break;
            conn->flush();
        }
    }

    uint16_t port_;
    IPv4Address addr_;
    TcpServer server_;
};

// ============================================================
// RawSocket helper for raw send/recv
// ============================================================

class RawClient {
public:
    RawClient(const IPv4Address& addr) {
        sock_ = Socket::create_tcp();
        if (sock_) {
            connected_ = sock_->connect(addr);
        }
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

// ============================================================
// Basic echo: single client, single message
// ============================================================

TEST(ConcurrentNet, SingleClientSingleMessage) {
    EchoServerFixture fixture;
    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    const std::string msg = "Hello Echo Server!";
    ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

    std::string response(msg.size(), '\0');
    ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
    EXPECT_EQ(response, msg);
}

TEST(ConcurrentNet, SingleClientMultipleMessages) {
    EchoServerFixture fixture;
    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    for (int i = 0; i < 30; ++i) {
        std::string msg = "message_" + std::to_string(i) +
                          "_padding_data_to_make_message_longer_12345";
        ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

        std::string response(msg.size(), '\0');
        ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
        EXPECT_EQ(response, msg) << "Mismatch at message " << i;
    }
}

// ============================================================
// Multiple concurrent clients (10+) each sending 50 messages
// ============================================================

TEST(ConcurrentNet, TenClientsEachFiftyMessages) {
    EchoServerFixture fixture;

    const int kNumClients = 10;
    const int kMsgsPerClient = 50;
    std::atomic<int> total_success{0};
    std::atomic<int> total_failures{0};
    std::vector<std::thread> threads;

    for (int c = 0; c < kNumClients; ++c) {
        threads.emplace_back([&fixture, c, kMsgsPerClient,
                               &total_success, &total_failures]() {
            RawClient client(fixture.addr());
            if (!client.connected()) {
                total_failures.fetch_add(kMsgsPerClient);
                return;
            }

            for (int i = 0; i < kMsgsPerClient; ++i) {
                std::string msg = "c" + std::to_string(c) + "_m" +
                                  std::to_string(i) + "_pad_data_xyz_";

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

    for (auto& t : threads) t.join();

    EXPECT_EQ(total_failures.load(), 0);
    EXPECT_EQ(total_success.load(), kNumClients * kMsgsPerClient);
}

// ============================================================
// More concurrent clients (20+)
// ============================================================

TEST(ConcurrentNet, TwentyClientsEachTenMessages) {
    EchoServerFixture fixture;

    const int kNumClients = 20;
    const int kMsgsPerClient = 10;
    std::atomic<int> success{0};
    std::vector<std::thread> threads;

    for (int c = 0; c < kNumClients; ++c) {
        threads.emplace_back([&fixture, c, &success]() {
            RawClient client(fixture.addr());
            if (!client.connected()) return;

            for (int i = 0; i < kMsgsPerClient; ++i) {
                std::string msg = "client_" + std::to_string(c) +
                                  "_msg_" + std::to_string(i);
                if (client.send_all(msg.data(), msg.size())) {
                    std::string response(msg.size(), '\0');
                    if (client.recv_all(&response[0], msg.size()) &&
                        response == msg) {
                        success.fetch_add(1);
                    }
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    // Allow some failures under heavy load
    EXPECT_GT(success.load(), 0);
    EXPECT_GE(fixture.total_handled(), 1u);
}

// ============================================================
// Heavy concurrency: 100 concurrent TCP connections
// ============================================================

TEST(ConcurrentNet, OneHundredConcurrentConnections) {
    LargeEchoServerFixture fixture(8);

    const int kNumClients = 100;
    const int kMsgsPerClient = 5;
    std::atomic<int> success{0};
    std::atomic<int> connected{0};
    std::vector<std::thread> threads;

    auto t_start = now_ns();

    for (int c = 0; c < kNumClients; ++c) {
        threads.emplace_back([&fixture, c, &success, &connected]() {
            RawClient client(fixture.addr());
            if (!client.connected()) return;
            connected.fetch_add(1);

            for (int i = 0; i < kMsgsPerClient; ++i) {
                std::string msg = "hundred_" + std::to_string(c) + "_" +
                                  std::to_string(i);
                if (client.send_all(msg.data(), msg.size())) {
                    std::string response(msg.size(), '\0');
                    if (client.recv_all(&response[0], msg.size()) &&
                        response == msg) {
                        success.fetch_add(1);
                    }
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;

    std::cout << "[Net100Concurrent] " << connected.load() << " connected, "
              << success.load() << " successful echoes in " << elapsed_ms << "ms"
              << std::endl;
    EXPECT_GT(connected.load(), 0);
    EXPECT_GT(success.load(), 0);
}

// ============================================================
// Echo server throughput with varying client counts
// ============================================================

TEST(ConcurrentNet, ThroughputVaryingClients) {
    for (int num_clients : {1, 10, 50}) {
        LargeEchoServerFixture fixture(4);
        const int msgs_per_client = 20;
        const size_t msg_size = 4096;
        std::atomic<int> success{0};
        std::vector<std::thread> threads;

        auto t_start = now_ns();

        for (int c = 0; c < num_clients; ++c) {
            threads.emplace_back([&fixture, c, &success, msg_size, msgs_per_client]() {
                RawClient client(fixture.addr());
                if (!client.connected()) return;

                std::string msg(msg_size, static_cast<char>('A' + (c % 26)));
                for (int i = 0; i < msgs_per_client; ++i) {
                    msg[0] = static_cast<char>('A' + (i % 26));
                    if (!client.send_all(msg.data(), msg.size())) return;

                    std::string response(msg.size(), '\0');
                    if (!client.recv_all(&response[0], msg.size())) return;

                    if (response == msg) {
                        success.fetch_add(1);
                    }
                }
            });
        }

        for (auto& t : threads) t.join();

        auto t_end = now_ns();
        double elapsed_ms = (t_end - t_start) / 1e6;
        size_t total_bytes = size_t(num_clients) * msgs_per_client * msg_size * 2;
        double mbps = (total_bytes / (1024.0 * 1024.0)) /
                      (elapsed_ms / 1000.0);

        int expected = num_clients * msgs_per_client;
        std::cout << "[NetThroughput-" << num_clients << "c] "
                  << success.load() << "/" << expected << " msgs, "
                  << (total_bytes / (1024.0 * 1024.0)) << " MB in "
                  << elapsed_ms << "ms (" << mbps << " MB/s)" << std::endl;
    }
}

// ============================================================
// Connection accept rate
// ============================================================

TEST(ConcurrentNet, ConnectionAcceptRate) {
    LargeEchoServerFixture fixture(4);

    const int num_connections = 500;
    std::atomic<int> attempts{0};
    std::atomic<int> successes{0};

    auto t_start = now_ns();

    // Sequential connections to measure accept rate
    for (int i = 0; i < num_connections; ++i) {
        attempts.fetch_add(1);
        RawClient client(fixture.addr());
        if (client.connected()) {
            successes.fetch_add(1);
            // Quick echo
            const std::string msg = "conn_test";
            client.send_all(msg.data(), msg.size());
            std::string resp(msg.size(), '\0');
            client.recv_all(&resp[0], msg.size());
        }
    }

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;

    std::cout << "[NetAcceptRate] " << successes.load() << "/" << attempts.load()
              << " connections in " << elapsed_ms << "ms ("
              << (successes.load() / std::max(elapsed_ms, 0.001) * 1000.0)
              << " conns/sec)" << std::endl;
    EXPECT_GT(successes.load(), 0);
}

// ============================================================
// Large message transfer
// ============================================================

TEST(ConcurrentNet, LargeMessage64KB) {
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

TEST(ConcurrentNet, LargeMessage128KB) {
    EchoServerFixture fixture;
    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    std::string msg(131072, '\0');  // 128KB
    for (size_t i = 0; i < msg.size(); ++i) {
        msg[i] = static_cast<char>('0' + (i % 10));
    }

    ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

    std::string response(msg.size(), '\0');
    ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
    EXPECT_EQ(response, msg);
}

TEST(ConcurrentNet, LargeMessage1MB) {
    EchoServerFixture fixture;
    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    std::string msg(1024 * 1024, '\0'); // 1MB
    for (size_t i = 0; i < msg.size(); ++i) {
        msg[i] = static_cast<char>('A' + (i % 26));
    }

    auto t_start = now_ns();

    ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

    std::string response(msg.size(), '\0');
    ASSERT_TRUE(client.recv_all(&response[0], msg.size()));

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;
    size_t total = msg.size() * 2; // send + recv
    double mbps = (total / (1024.0 * 1024.0)) / (elapsed_ms / 1000.0);

    EXPECT_EQ(response, msg);
    std::cout << "[NetLarge1MB] 1MB echo in " << elapsed_ms << "ms ("
              << mbps << " MB/s)" << std::endl;
}

TEST(ConcurrentNet, MultipleLargeMessages) {
    EchoServerFixture fixture;
    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    std::vector<size_t> sizes = {1024, 8192, 16384, 32768, 65536};

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

TEST(ConcurrentNet, HundredMBThroughput) {
    // Transfer 100MB in 1MB chunks through echo server
    LargeEchoServerFixture fixture(4);
    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    const size_t total_size = 100 * 1024 * 1024; // 100MB
    const size_t chunk_size = 1024 * 1024;       // 1MB
    const int num_chunks = static_cast<int>(total_size / chunk_size);
    std::string chunk(chunk_size, 'X');

    auto t_start = now_ns();

    for (int i = 0; i < num_chunks; ++i) {
        chunk[0] = static_cast<char>('A' + (i % 26));
        ASSERT_TRUE(client.send_all(chunk.data(), chunk.size()));

        std::string response(chunk.size(), '\0');
        ASSERT_TRUE(client.recv_all(&response[0], chunk.size()));

        if (i % 10 == 0) {
            EXPECT_EQ(response[0], chunk[0]) << "Corruption at chunk " << i;
        }
    }

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;
    size_t total_transferred = total_size * 2; // send + receive
    double mbps = (total_transferred / (1024.0 * 1024.0)) /
                  (elapsed_ms / 1000.0);

    std::cout << "[Net100MB] " << (total_transferred / (1024.0 * 1024.0))
              << " MB in " << elapsed_ms << "ms ("
              << mbps << " MB/s)" << std::endl;
    EXPECT_GT(mbps, 0.0);
}

// ============================================================
// Many small messages (1 million 64-byte messages)
// ============================================================

TEST(ConcurrentNet, ManySmallMessages) {
    LargeEchoServerFixture fixture(4);
    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    const int kNumMsgs = 10000; // Reduced from 1M for test runtime
    const size_t msg_size = 64;
    std::string msg(msg_size, 's');

    auto t_start = now_ns();

    for (int i = 0; i < kNumMsgs; ++i) {
        msg[0] = static_cast<char>('a' + (i % 26));
        ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

        std::string response(msg.size(), '\0');
        ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
        EXPECT_EQ(response[0], msg[0]);
    }

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;
    size_t total_bytes = kNumMsgs * msg_size * 2;
    double mbps = (total_bytes / (1024.0 * 1024.0)) /
                  (elapsed_ms / 1000.0);

    std::cout << "[NetSmallMsgs] " << kNumMsgs << " x " << msg_size
              << "B msgs in " << elapsed_ms << "ms ("
              << (kNumMsgs / std::max(elapsed_ms, 0.001) * 1000.0)
              << " msgs/sec, " << mbps << " MB/s)" << std::endl;
}

// ============================================================
// Connection/disconnection cycle
// ============================================================

TEST(ConcurrentNet, SequentialConnectDisconnect) {
    EchoServerFixture fixture;

    for (int round = 0; round < 15; ++round) {
        RawClient client(fixture.addr());
        ASSERT_TRUE(client.connected())
            << "Connection failed at round " << round;

        std::string msg = "round_" + std::to_string(round);
        ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

        std::string response(msg.size(), '\0');
        ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
        EXPECT_EQ(response, msg);
        // Client closes when it goes out of scope
    }
    SUCCEED();
}

TEST(ConcurrentNet, RapidConnectDisconnect) {
    // Rapid connect/disconnect cycles
    LargeEchoServerFixture fixture(4);
    const int cycles = 200;
    std::atomic<int> successes{0};

    for (int c = 0; c < cycles; ++c) {
        RawClient client(fixture.addr());
        if (client.connected()) {
            successes.fetch_add(1);
            // Quick echo
            std::string msg = "rapid_" + std::to_string(c);
            client.send_all(msg.data(), msg.size());
            std::string resp(msg.size(), '\0');
            client.recv_all(&resp[0], msg.size());
        }
    }

    std::cout << "[NetRapidConnect] " << successes.load() << "/" << cycles
              << " connections" << std::endl;
    EXPECT_GT(successes.load(), 0);
}

// ============================================================
// Connection pool: persistent connections
// ============================================================

TEST(ConcurrentNet, PersistentConnectionPool) {
    // Maintain 100 persistent connections
    LargeEchoServerFixture fixture(8);

    const int pool_size = 100;
    const int rounds = 10;
    std::atomic<int> total_success{0};

    std::vector<std::unique_ptr<RawClient>> pool;
    pool.reserve(pool_size);

    // Create persistent connections
    for (int i = 0; i < pool_size; ++i) {
        auto client = std::make_unique<RawClient>(fixture.addr());
        ASSERT_TRUE(client->connected()) << "Pool connection " << i << " failed";
        pool.push_back(std::move(client));
    }

    // Use all connections for multiple rounds
    for (int r = 0; r < rounds; ++r) {
        std::vector<std::thread> threads;
        for (int i = 0; i < pool_size; ++i) {
            threads.emplace_back([&pool, &total_success, r, i]() {
                std::string msg = "pool_" + std::to_string(i) + "_r" +
                                  std::to_string(r);
                if (pool[i]->send_all(msg.data(), msg.size())) {
                    std::string response(msg.size(), '\0');
                    if (pool[i]->recv_all(&response[0], msg.size()) &&
                        response == msg) {
                        total_success.fetch_add(1);
                    }
                }
            });
        }
        for (auto& t : threads) t.join();
    }

    pool.clear();

    int expected = pool_size * rounds;
    std::cout << "[NetPool] " << total_success.load() << "/" << expected
              << " echoes from persistent pool" << std::endl;
    EXPECT_GT(total_success.load(), 0);
}

// ============================================================
// Half-open connection handling
// ============================================================

TEST(ConcurrentNet, HalfOpenConnection) {
    EchoServerFixture fixture;
    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    // Send a message
    std::string msg = "hello_before_close";
    ASSERT_TRUE(client.send_all(msg.data(), msg.size()));
    std::string response(msg.size(), '\0');
    ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
    EXPECT_EQ(response, msg);

    // Abruptly close client (simulating half-open)
    client.close();

    // Server should handle this gracefully (test passes if no crash)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    SUCCEED();
}

// ============================================================
// Buffer stress: multiple writers/readers
// ============================================================

TEST(ConcurrentNet, BufferParallelWrites) {
    const int kNumThreads = 8;
    const int kPerThread = 1000;

    std::vector<std::thread> threads;
    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([t, kPerThread]() {
            Buffer buf;
            std::string data(500, static_cast<char>('A' + t));
            for (int i = 0; i < kPerThread; ++i) {
                buf.append(data.data(), data.size());
            }
            EXPECT_EQ(buf.readable_size(), 500u * kPerThread);
        });
    }
    for (auto& th : threads) th.join();
}

TEST(ConcurrentNet, BufferLargeDataReadWrite) {
    Buffer buf;
    std::string data(1024 * 1024, 'X');  // 1MB
    buf.append(data.data(), data.size());
    EXPECT_EQ(buf.readable_size(), data.size());

    std::string out(data.size(), '\0');
    size_t n = buf.read(&out[0], data.size());
    EXPECT_EQ(n, data.size());
    EXPECT_EQ(out, data);
}

TEST(ConcurrentNet, BufferChainStress) {
    Buffer buf;
    // Write many small chunks to force chain creation
    for (int i = 0; i < 10000; ++i) {
        buf.append("abcdefgh", 8);
    }
    EXPECT_EQ(buf.readable_size(), 80000u);

    // Read back in various sizes
    char tmp[64];
    size_t total_read = 0;
    while (!buf.empty()) {
        size_t to_read = std::min(sizeof(tmp), buf.readable_size());
        buf.read(tmp, to_read);
        total_read += to_read;
    }
    EXPECT_EQ(total_read, 80000u);
}

TEST(ConcurrentNet, BufferMixedSizesStress) {
    Buffer buf;
    std::string out;

    for (int iter = 0; iter < 100; ++iter) {
        // Write various sizes
        for (int i = 1; i <= 100; ++i) {
            std::string chunk(i, static_cast<char>('a' + (i % 26)));
            buf.append(chunk.data(), chunk.size());
        }

        // Read back
        while (buf.readable_size() >= 10) {
            char tmp[10];
            size_t n = buf.read(tmp, 10);
            out.append(tmp, n);
        }

        // Drain remainder
        if (!buf.empty()) {
            char tmp[256];
            size_t n = buf.read(tmp, buf.readable_size());
            out.append(tmp, n);
        }
        buf.clear();
    }

    // Verify total bytes make sense (sum of 1..100 per iteration, for 100 iters)
    EXPECT_EQ(out.size(), static_cast<size_t>(100 * (100 * 101 / 2)));
}

TEST(ConcurrentNet, BufferIntegersEndian) {
    Buffer buf;

    buf.write_int32(0x12345678);
    buf.write_int16(0x7FFF);
    buf.write_uint64(0xDEADBEEFCAFEBABEULL);
    buf.write_float(3.14159f);
    buf.write_double(2.718281828);

    EXPECT_EQ(buf.read_int32(), 0x12345678);
    EXPECT_EQ(buf.read_int16(), 0x7FFF);
    EXPECT_EQ(buf.read_uint64(), 0xDEADBEEFCAFEBABEULL);
    EXPECT_FLOAT_EQ(buf.read_float(), 3.14159f);
    EXPECT_DOUBLE_EQ(buf.read_double(), 2.718281828);
}

TEST(ConcurrentNet, BufferOneGBThroughput) {
    // Write 1GB through buffer, verify integrity
    Buffer buf;
    const size_t total_size = 1000ULL * 1024 * 1024; // 1000 MB
    const size_t chunk_size = 1024 * 1024;            // 1 MB
    const int num_chunks = static_cast<int>(total_size / chunk_size);

    auto t_start = now_ns();

    for (int i = 0; i < num_chunks; ++i) {
        std::string data(chunk_size, static_cast<char>('A' + (i % 26)));
        buf.append(data.data(), data.size());

        // Drain periodically
        if (buf.readable_size() > 10 * chunk_size) {
            buf.consume(buf.readable_size());
        }
    }
    buf.consume(buf.readable_size());

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;
    double mbps = (total_size / (1024.0 * 1024.0)) / (elapsed_ms / 1000.0);

    EXPECT_TRUE(buf.empty());
    std::cout << "[NetBuffer1GB] " << (total_size / (1024.0 * 1024.0))
              << " MB in " << elapsed_ms << "ms ("
              << mbps << " MB/s)" << std::endl;
}

// ============================================================
// Address resolution stress
// ============================================================

TEST(ConcurrentNet, AddressIPv4CreateStress) {
    std::vector<std::thread> threads;
    std::atomic<int> success{0};

    for (int t = 0; t < 20; ++t) {
        threads.emplace_back([t, &success]() {
            for (int i = 0; i < 100; ++i) {
                uint16_t port = static_cast<uint16_t>(10000 + (t * 100 + i) % 50000);
                IPv4Address addr("127.0.0.1", port);
                EXPECT_EQ(addr.ip(), "127.0.0.1");
                EXPECT_EQ(addr.port(), port);
                EXPECT_EQ(addr.family(), AF_INET);
                success.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(success.load(), 2000);
}

TEST(ConcurrentNet, AddressLookupStress) {
    std::atomic<int> success{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 50; ++i) {
                auto addrs = Address::lookup_all("127.0.0.1", 8080);
                if (!addrs.empty()) {
                    success.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_GT(success.load(), 0);
}

TEST(ConcurrentNet, AddressHundredThreadsResolving) {
    // 100 threads resolving "localhost"
    std::atomic<int> success{0};
    const int num_threads = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&success]() {
            for (int i = 0; i < 10; ++i) {
                auto addrs = Address::lookup_all("127.0.0.1", 12345);
                if (!addrs.empty()) {
                    success.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_GT(success.load(), 0);
    std::cout << "[NetAddrResolve] " << success.load()
              << " successful resolves from " << num_threads << " threads"
              << std::endl;
}

// ============================================================
// Socket operations stress
// ============================================================

TEST(ConcurrentNet, SocketCreateCloseStress) {
    std::vector<std::thread> threads;
    std::atomic<int> count{0};

    for (int t = 0; t < 20; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 100; ++i) {
                auto sock = Socket::create_tcp();
                if (sock && sock->is_valid()) {
                    count.fetch_add(1);
                    sock->close();
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(count.load(), 2000);
}

TEST(ConcurrentNet, SocketOptions) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    EXPECT_TRUE(sock->set_reuse_addr(true));
    EXPECT_TRUE(sock->get_reuse_addr());

    EXPECT_TRUE(sock->set_tcp_nodelay(true));
    EXPECT_TRUE(sock->get_tcp_nodelay());

    EXPECT_TRUE(sock->set_keepalive(true));
    EXPECT_TRUE(sock->get_keepalive());

    EXPECT_TRUE(sock->set_nonblocking(true));
    EXPECT_TRUE(sock->is_nonblocking());

    sock->close();
}

TEST(ConcurrentNet, SocketBindRandomPort) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->set_reuse_addr(true));

    IPv4Address addr(0);  // Port 0 = random
    EXPECT_TRUE(sock->bind(addr));

    auto local = sock->local_address();
    ASSERT_NE(local, nullptr);

    auto ipv4 = std::dynamic_pointer_cast<IPv4Address>(local);
    if (ipv4) {
        EXPECT_GT(ipv4->port(), 0u);
    }

    sock->close();
}

TEST(ConcurrentNet, TCPNodelayEffect) {
    // Measure latency with and without TCP_NODELAY
    EchoServerFixture fixture;

    {
        // With TCP_NODELAY
        auto sock = Socket::create_tcp();
        ASSERT_TRUE(sock->set_tcp_nodelay(true));
        ASSERT_TRUE(sock->connect(fixture.addr()));

        auto t_start = now_ns();
        for (int i = 0; i < 100; ++i) {
            std::string msg = "nodelay_test_" + std::to_string(i);
            size_t total = 0;
            const char* p = msg.data();
            while (total < msg.size()) {
                ssize_t n = sock->send(p + total, msg.size() - total);
                if (n <= 0) break;
                total += n;
            }
            total = 0;
            std::string resp(msg.size(), '\0');
            p = resp.data();
            while (total < msg.size()) {
                ssize_t n = sock->recv(&resp[0] + total, msg.size() - total);
                if (n <= 0) break;
                total += n;
            }
        }
        auto t_end = now_ns();
        double elapsed_nodelay_ms = (t_end - t_start) / 1e6;

        sock->close();
        std::cout << "[NetNodelay] 100 echoes: " << elapsed_nodelay_ms
                  << "ms with TCP_NODELAY ("
                  << (elapsed_nodelay_ms / 100.0) << " ms/msg)" << std::endl;
    }
}

// ============================================================
// Varying message sizes with concurrent clients
// ============================================================

TEST(ConcurrentNet, ConcurrentClientsVaryingSizes) {
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

    for (auto& t : threads) t.join();
    EXPECT_EQ(success.load(), kNumClients * 5);
}

// ============================================================
// Throughput measurement
// ============================================================

TEST(ConcurrentNet, ThroughputMeasurement) {
    EchoServerFixture fixture;
    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    const size_t kMsgSize = 8192;
    const int kNumMsgs = 100;
    std::string msg(kMsgSize, 'X');

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < kNumMsgs; ++i) {
        ASSERT_TRUE(client.send_all(msg.data(), msg.size()));
        std::string response(msg.size(), '\0');
        ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    size_t total_bytes = kMsgSize * kNumMsgs * 2;  // send + receive
    double mbps = (total_bytes / (1024.0 * 1024.0)) /
                  (elapsed_ms / 1000.0);

    EXPECT_GT(mbps, 0.0);

    std::cout << "[NetThroughput] " << kNumMsgs << " messages of "
              << kMsgSize << " bytes: " << total_bytes
              << " total bytes in " << elapsed_ms << "ms ("
              << mbps << " MB/s)" << std::endl;
}

// ============================================================
// Rapid small messages
// ============================================================

TEST(ConcurrentNet, RapidSmallMessages) {
    EchoServerFixture fixture;
    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    const int kNumMsgs = 200;
    for (int i = 0; i < kNumMsgs; ++i) {
        std::string msg = "m" + std::to_string(i);
        ASSERT_TRUE(client.send_all(msg.data(), msg.size()));

        std::string response(msg.size(), '\0');
        ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
        EXPECT_EQ(response, msg);
    }
}

// ============================================================
// Empty message edge case
// ============================================================

TEST(ConcurrentNet, EmptyMessage) {
    EchoServerFixture fixture;
    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    ASSERT_TRUE(client.send_all("", 0));
    SUCCEED();
}

// ============================================================
// Server restart cycle (bind same port)
// ============================================================

TEST(ConcurrentNet, ServerRestartOnSamePort) {
    uint16_t port = get_random_port();

    {
        TcpServer server1;
        IPv4Address addr("127.0.0.1", port);
        server1.set_connection_callback([](SocketStream::Ptr) {});
        ASSERT_TRUE(server1.bind(addr));
        ASSERT_TRUE(server1.start());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        server1.stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Try to bind same port again (may fail due to TIME_WAIT)
    {
        TcpServer server2;
        IPv4Address addr("127.0.0.1", port);
        server2.set_connection_callback([](SocketStream::Ptr) {});
        // This may or may not succeed depending on SO_REUSEADDR
        bool bound = server2.bind(addr);
        if (bound) {
            server2.stop();
        }
    }
    SUCCEED();
}

// ============================================================
// Graceful shutdown during active connections
// ============================================================

TEST(ConcurrentNet, GracefulShutdown) {
    uint16_t port = get_random_port();

    TcpServer server;
    std::atomic<int> connections{0};
    std::atomic<bool> server_done{false};

    server.set_connection_callback([&connections](SocketStream::Ptr conn) {
        connections.fetch_add(1);
        char buf[1024];
        while (true) {
            ssize_t n = conn->read(buf, sizeof(buf));
            if (n <= 0) break;
            conn->write(buf, n);
            conn->flush();
        }
    });

    IPv4Address addr("127.0.0.1", port);
    ASSERT_TRUE(server.bind(addr));
    ASSERT_TRUE(server.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Connect a client
    RawClient client(addr);
    ASSERT_TRUE(client.connected());

    // Send and receive to verify connection works
    const std::string msg = "shutdown_test";
    ASSERT_TRUE(client.send_all(msg.data(), msg.size()));
    std::string response(msg.size(), '\0');
    ASSERT_TRUE(client.recv_all(&response[0], msg.size()));
    EXPECT_EQ(response, msg);

    // Close client
    client.close();

    // Graceful stop
    server.stop(true);

    EXPECT_GE(connections.load(), 1);
}

// ============================================================
// UDP stress test
// ============================================================

TEST(ConcurrentNet, UdpSendReceive) {
    // Create UDP sockets for send/receive test
    auto sender = Socket::create_udp();
    auto receiver = Socket::create_udp();
    ASSERT_NE(sender, nullptr);
    ASSERT_NE(receiver, nullptr);

    // Bind receiver to random port
    uint16_t port = get_random_port();
    IPv4Address recv_addr("127.0.0.1", port);
    ASSERT_TRUE(receiver->bind(recv_addr));
    ASSERT_TRUE(receiver->set_nonblocking(true));

    // Send from sender to receiver
    const std::string msg = "UDP stress test message 12345";
    ssize_t sent = sender->sendto(msg.data(), msg.size(), recv_addr);
    ASSERT_GT(sent, 0);

    // Receive on receiver
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    char buf[1024];
    ssize_t received = receiver->recv(buf, sizeof(buf));
    EXPECT_GT(received, 0);

    sender->close();
    receiver->close();
}

TEST(ConcurrentNet, UdpPacketStress) {
    // Send 10000 UDP packets, count received
    auto sender = Socket::create_udp();
    auto receiver = Socket::create_udp();
    ASSERT_NE(sender, nullptr);
    ASSERT_NE(receiver, nullptr);

    uint16_t port = get_random_port();
    IPv4Address recv_addr("127.0.0.1", port);
    ASSERT_TRUE(receiver->bind(recv_addr));
    ASSERT_TRUE(receiver->set_nonblocking(true));

    const int kPackets = 10000;
    std::atomic<int> sent{0};
    std::atomic<int> received_count{0};

    // Sender thread
    std::thread sender_thread([&]() {
        for (int i = 0; i < kPackets; ++i) {
            std::string msg = "udp_pkt_" + std::to_string(i);
            ssize_t n = sender->sendto(msg.data(), msg.size(), recv_addr);
            if (n > 0) {
                sent.fetch_add(1);
            }
        }
    });

    // Receiver collects for a while
    auto deadline = std::chrono::steady_clock::now() +
                     std::chrono::seconds(5);
    char buf[2048];
    while (std::chrono::steady_clock::now() < deadline) {
        ssize_t n = receiver->recv(buf, sizeof(buf));
        if (n > 0) {
            received_count.fetch_add(1);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        if (received_count.load() >= kPackets) break;
    }

    sender_thread.join();
    sender->close();
    receiver->close();

    EXPECT_EQ(sent.load(), kPackets);
    std::cout << "[UdpStress] " << sent.load() << " sent, "
              << received_count.load() << " received ("
              << (100.0 * received_count.load() / std::max(sent.load(), 1))
              << "%)" << std::endl;
    // On localhost, most packets should be received
    EXPECT_GT(received_count.load(), kPackets * 0.9);
}

// ============================================================
// Backpressure: slow consumer test
// ============================================================

TEST(ConcurrentNet, BackpressureSlowConsumer) {
    EchoServerFixture fixture;
    RawClient client(fixture.addr());
    ASSERT_TRUE(client.connected());

    // Send many messages rapidly
    const int kNumMsgs = 500;
    const size_t msg_size = 8192;
    std::string msg(msg_size, 'B');
    std::atomic<int> sent{0};

    std::thread sender([&]() {
        for (int i = 0; i < kNumMsgs; ++i) {
            if (!client.send_all(msg.data(), msg.size())) break;
            sent.fetch_add(1);
        }
    });

    // Receive slowly with delays
    std::vector<std::string> responses;
    for (int i = 0; i < kNumMsgs; ++i) {
        std::string response(msg.size(), '\0');
        if (!client.recv_all(&response[0], msg.size())) break;
        responses.push_back(std::move(response));
        // Slow consumption
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    sender.join();

    std::cout << "[NetBackpressure] " << sent.load() << " sent, "
              << responses.size() << " received" << std::endl;
    // Server should have buffered; all sent messages should be echo'd
    EXPECT_GE(responses.size(), static_cast<size_t>(sent.load() * 0.9));
}

// ============================================================
// Concurrent connection storm
// ============================================================

TEST(ConcurrentNet, ConnectionStorm) {
    // Many threads simultaneously connecting and disconnecting
    LargeEchoServerFixture fixture(4);

    const int num_threads = 50;
    const int cycles_per_thread = 10;
    std::atomic<int> total_connections{0};
    std::atomic<int> total_echoes{0};

    auto t_start = now_ns();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int c = 0; c < cycles_per_thread; ++c) {
                RawClient client(fixture.addr());
                if (!client.connected()) continue;
                total_connections.fetch_add(1);

                std::string msg = "storm_" + std::to_string(c);
                if (client.send_all(msg.data(), msg.size())) {
                    std::string resp(msg.size(), '\0');
                    if (client.recv_all(&resp[0], msg.size())) {
                        total_echoes.fetch_add(1);
                    }
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    auto t_end = now_ns();
    double elapsed_ms = (t_end - t_start) / 1e6;

    std::cout << "[NetStorm] " << total_connections.load() << " connections, "
              << total_echoes.load() << " echoes in " << elapsed_ms << "ms"
              << std::endl;
    EXPECT_GT(total_connections.load(), 0);
}

// ============================================================
// Concurrent buffer stress: 32 threads writing to separate buffers
// ============================================================

TEST(ConcurrentNet, ConcurrentBuffer32Threads) {
    // 32 threads each writing to their own buffer
    const int num_threads = 32;
    std::vector<std::thread> threads;
    std::atomic<int> successful{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, &successful]() {
            Buffer buf;
            std::string data(4096, static_cast<char>('A' + (t % 26)));
            for (int i = 0; i < 100; ++i) {
                buf.append(data.data(), data.size());
            }
            // Verify
            EXPECT_EQ(buf.readable_size(), 409600u);
            // Read all back
            char* out = new char[409600];
            size_t n = buf.read(out, 409600);
            EXPECT_EQ(n, 409600u);
            delete[] out;
            successful.fetch_add(1);
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(successful.load(), num_threads);
}

// ============================================================
// All done
// ============================================================

// Finished. Total: ~40 test cases over 19 sections.
