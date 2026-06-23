// test_bench_net.cpp — Network performance benchmarks
//
// Measures: TCP echo QPS, TCP throughput (MB/s), TCP latency (us RTT),
// connection rate (conn/s), buffer throughput (GB/s), address resolution
// QPS, socket creation/destruction rate, multi-client QPS at various
// concurrency levels, large-message throughput, small-message RTT latency,
// and buffer varint encode/decode speed.
//
// All timing uses std::chrono::high_resolution_clock.
// Results printed with std::cout using [BENCH] prefix.

#include <gtest/gtest.h>
#include "zero/zero.h"
#include "zero/net/tcp_server.h"
#include "zero/net/address.h"
#include "zero/net/buffer.h"
#include "zero/net/socket.h"
#include "zero/net/socket_stream.h"

#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace zero;
using namespace std::chrono;

// ============================================================
// Helper: format large numbers with commas
// ============================================================

static std::string fmt_num(long long n) {
    if (n == 0) return "0";
    bool neg = n < 0;
    if (neg) n = -n;
    std::string s;
    int cnt = 0;
    while (n > 0) {
        if (cnt && cnt % 3 == 0) s = "," + s;
        s = static_cast<char>('0' + (n % 10)) + s;
        n /= 10;
        ++cnt;
    }
    return neg ? "-" + s : s;
}

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

// ============================================================
// Echo server benchmark fixture
// ============================================================

class BenchEchoServer {
public:
    BenchEchoServer() : port_(get_random_port()), addr_("127.0.0.1", port_) {
        server_.set_connection_callback([this](SocketStream::Ptr conn) {
            handle_connection(conn);
        });
        EXPECT_TRUE(server_.bind(addr_));
        EXPECT_TRUE(server_.start());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    explicit BenchEchoServer(int workers)
        : port_(get_random_port()), addr_("127.0.0.1", port_) {
        server_.set_worker_count(workers);
        server_.set_connection_callback([this](SocketStream::Ptr conn) {
            handle_connection(conn);
        });
        EXPECT_TRUE(server_.bind(addr_));
        EXPECT_TRUE(server_.start());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ~BenchEchoServer() { server_.stop(); }

    uint16_t port() const { return port_; }
    const IPv4Address& addr() const { return addr_; }
    size_t total_bytes() const { return total_bytes_.load(); }

private:
    void handle_connection(SocketStream::Ptr conn) {
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
    std::atomic<size_t> total_bytes_{0};
};

// ============================================================
// Raw TCP client helper for benchmarks
// ============================================================

class BenchClient {
public:
    explicit BenchClient(const IPv4Address& addr)
        : connected_(false) {
        sock_ = Socket::create_tcp();
        if (sock_) {
            connected_ = sock_->connect(addr);
        }
    }

    ~BenchClient() {
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

    ssize_t send_raw(const void* data, size_t len) {
        return sock_->send(data, len);
    }

    ssize_t recv_raw(void* buf, size_t len) {
        return sock_->recv(buf, len);
    }

    void close() {
        if (sock_) sock_->close();
        connected_ = false;
    }

private:
    Socket::Ptr sock_;
    bool connected_;
};

// ============================================================
// TCP Echo QPS — single client echo throughput
// ============================================================

TEST(BenchNet, TCPEchoQPS) {
    BenchEchoServer server;
    BenchClient client(server.addr());
    ASSERT_TRUE(client.connected());

    const int N = 50000;
    const char* msg = "echo benchmark message for QPS measurement";
    size_t msg_len = strlen(msg);

    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        ASSERT_TRUE(client.send_all(msg, msg_len));
        std::string response(msg_len, '\0');
        ASSERT_TRUE(client.recv_all(&response[0], msg_len));
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);
    size_t total_bytes = msg_len * N * 2; // send + recv
    double mbps = (total_bytes / (1024.0 * 1024.0)) / (us / 1000000.0);

    std::cout << "[BENCH] Net TCP Echo QPS: " << std::fixed << std::setprecision(0)
              << qps << " req/sec, " << std::setprecision(2)
              << mbps << " MB/s (" << fmt_num(N) << " echoes in "
              << us / 1000 << " ms, " << fmt_num(total_bytes)
              << " bytes total)" << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================
// TCP Throughput — raw TCP throughput in MB/s
// ============================================================

TEST(BenchNet, TCPThroughput) {
    BenchEchoServer server;
    BenchClient client(server.addr());
    ASSERT_TRUE(client.connected());

    const size_t kChunkSize = 65536; // 64KB
    const size_t kTotalBytes = 100ULL * 1024 * 1024; // 100MB
    const size_t kChunks = kTotalBytes / kChunkSize;

    std::string chunk(kChunkSize, 'X');

    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < kChunks; ++i) {
        ASSERT_TRUE(client.send_all(chunk.data(), kChunkSize));
        std::string response(kChunkSize, '\0');
        ASSERT_TRUE(client.recv_all(&response[0], kChunkSize));
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();

    size_t total_on_wire = kTotalBytes * 2; // send + recv
    double mbps = (total_on_wire / (1024.0 * 1024.0)) / (us / 1000000.0);
    double gbps = mbps * 8.0 / 1024.0;

    std::cout << "[BENCH] Net TCP Throughput: " << std::fixed << std::setprecision(2)
              << mbps << " MB/s (" << gbps << " Gbps) — "
              << kChunks << " x " << kChunkSize << "B chunks in "
              << us / 1000 << " ms" << std::endl;
    EXPECT_GT(mbps, 0);
}

// ============================================================
// TCP Latency — round-trip latency, 1-byte ping
// ============================================================

TEST(BenchNet, TCPLatency) {
    BenchEchoServer server;
    BenchClient client(server.addr());
    ASSERT_TRUE(client.connected());

    const int N = 10000;
    std::vector<long long> latencies;
    latencies.reserve(N);
    char ping = 'P';

    for (int i = 0; i < N; ++i) {
        auto t1 = high_resolution_clock::now();
        ASSERT_TRUE(client.send_all(&ping, 1));
        char pong;
        ASSERT_TRUE(client.recv_all(&pong, 1));
        auto t2 = high_resolution_clock::now();
        latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
    }

    std::sort(latencies.begin(), latencies.end());
    long long p50  = latencies[N * 50 / 100];
    long long p90  = latencies[N * 90 / 100];
    long long p99  = latencies[N * 99 / 100];
    long long p999 = latencies[N * 999 / 1000];
    long long p100 = latencies.back();
    double avg = (double)std::accumulate(latencies.begin(), latencies.end(), 0LL) / N;

    std::cout << "[BENCH] Net TCP Latency (1B RTT): avg=" << std::fixed
              << std::setprecision(2) << avg / 1000.0 << " us, p50="
              << p50 / 1000.0 << " us, p90=" << p90 / 1000.0
              << " us, p99=" << p99 / 1000.0 << " us, p999="
              << p999 / 1000.0 << " us, max=" << p100 / 1000.0
              << " us (" << N << " samples)" << std::endl;
    EXPECT_GT(N, 0);
}

// ============================================================
// Connection Rate — connections per second
// ============================================================

TEST(BenchNet, ConnectionRate) {
    uint16_t port = get_random_port();
    IPv4Address addr("127.0.0.1", port);

    // Minimal echo server
    TcpServer server;
    server.set_connection_callback([&](SocketStream::Ptr conn) {
        char buf[64];
        ssize_t n = conn->read(buf, sizeof(buf));
        if (n > 0) {
            conn->write(buf, n);
            conn->flush();
        }
    });
    ASSERT_TRUE(server.bind(addr));
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const int N = 5000;
    auto start = high_resolution_clock::now();

    for (int i = 0; i < N; ++i) {
        auto sock = Socket::create_tcp();
        if (sock && sock->connect(addr)) {
            const char* msg = "conn";
            sock->send(msg, 4);
            char buf[4];
            sock->recv(buf, 4);
            sock->close();
        }
    }

    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double conn_per_sec = (double)N / (us / 1000000.0);
    double us_per_conn = (double)us / N;

    std::cout << "[BENCH] Net Connection Rate: " << std::fixed
              << std::setprecision(0) << conn_per_sec << " conn/sec, "
              << std::setprecision(1) << us_per_conn
              << " us/conn (" << N << " connections in "
              << us / 1000 << " ms)" << std::endl;

    server.stop();
    EXPECT_GT(conn_per_sec, 0);
}

// ============================================================
// Buffer Throughput — read/write throughput in GB/s
// ============================================================

TEST(BenchNet, BufferThroughput) {
    const size_t kChunkSize = 4096;
    const size_t kTotalBytes = 100ULL * 1024 * 1024; // 100MB
    const size_t kChunks = kTotalBytes / kChunkSize;
    std::string chunk_data(kChunkSize, 'B');

    // Write benchmark
    Buffer buf;
    auto t1 = high_resolution_clock::now();
    for (size_t i = 0; i < kChunks; ++i) {
        buf.append(chunk_data.data(), kChunkSize);
    }
    auto t2 = high_resolution_clock::now();
    auto write_us = duration_cast<microseconds>(t2 - t1).count();
    double write_gbps = (kTotalBytes / (1024.0 * 1024.0 * 1024.0))
                        / (write_us / 1000000.0);

    EXPECT_EQ(buf.readable_size(), kTotalBytes);

    // Read benchmark
    char read_buf[kChunkSize];
    size_t total_read = 0;
    auto t3 = high_resolution_clock::now();
    while (!buf.empty()) {
        size_t to_read = std::min(kChunkSize, buf.readable_size());
        size_t n = buf.read(read_buf, to_read);
        total_read += n;
    }
    auto t4 = high_resolution_clock::now();
    auto read_us = duration_cast<microseconds>(t4 - t3).count();
    double read_gbps = (kTotalBytes / (1024.0 * 1024.0 * 1024.0))
                       / (read_us / 1000000.0);

    EXPECT_EQ(total_read, kTotalBytes);

    std::cout << "[BENCH] Net Buffer Throughput: write=" << std::fixed
              << std::setprecision(2) << write_gbps << " GB/s, read="
              << read_gbps << " GB/s ("
              << kTotalBytes / (1024 * 1024) << " MB in "
              << kChunkSize << "B chunks)" << std::endl;
}

TEST(BenchNet, BufferLargeThroughput) {
    const size_t kChunkSize = 65536; // 64KB
    const size_t kTotalBytes = 1024ULL * 1024 * 1024; // 1GB
    const size_t kChunks = kTotalBytes / kChunkSize;
    std::string chunk_data(kChunkSize, 'G');

    Buffer buf;
    auto t1 = high_resolution_clock::now();
    for (size_t i = 0; i < kChunks; ++i) {
        buf.append(chunk_data.data(), kChunkSize);
    }
    auto t2 = high_resolution_clock::now();
    auto write_us = duration_cast<microseconds>(t2 - t1).count();
    double write_gbps = (kTotalBytes / (1024.0 * 1024.0 * 1024.0))
                        / (write_us / 1000000.0);

    EXPECT_EQ(buf.readable_size(), kTotalBytes);

    // Read back
    char read_buf[kChunkSize];
    size_t total_read = 0;
    auto t3 = high_resolution_clock::now();
    while (!buf.empty()) {
        size_t to_read = std::min(kChunkSize, buf.readable_size());
        total_read += buf.read(read_buf, to_read);
    }
    auto t4 = high_resolution_clock::now();
    auto read_us = duration_cast<microseconds>(t4 - t3).count();
    double read_gbps = (kTotalBytes / (1024.0 * 1024.0 * 1024.0))
                       / (read_us / 1000000.0);

    EXPECT_EQ(total_read, kTotalBytes);

    std::cout << "[BENCH] Net Buffer Large Throughput (1GB): write="
              << std::fixed << std::setprecision(3) << write_gbps
              << " GB/s, read=" << read_gbps << " GB/s" << std::endl;
}

// ============================================================
// Address Resolution QPS
// ============================================================

TEST(BenchNet, AddressResolveQPS) {
    const int N = 10000;

    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        auto addrs = Address::lookup_all("127.0.0.1",
            static_cast<uint16_t>(8080 + (i % 1000)));
        ASSERT_FALSE(addrs.empty());
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);
    double ns_per_op = (double)us * 1000.0 / N;

    std::cout << "[BENCH] Net Address Resolve QPS: " << std::fixed
              << std::setprecision(0) << qps << " resolves/sec, "
              << std::setprecision(1) << ns_per_op << " ns/resolve ("
              << fmt_num(N) << " resolves in " << us / 1000 << " ms)"
              << std::endl;
    EXPECT_GT(qps, 0);
}

TEST(BenchNet, IPv4AddressCreateQPS) {
    const int N = 100000;

    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        uint16_t port = static_cast<uint16_t>(10000 + (i % 50000));
        IPv4Address addr("127.0.0.1", port);
        (void)addr.ip();
        (void)addr.port();
        (void)addr.family();
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);
    double ns_per_op = (double)us * 1000.0 / N;

    std::cout << "[BENCH] Net IPv4Address Create QPS: " << std::fixed
              << std::setprecision(0) << qps << " ops/sec, "
              << std::setprecision(1) << ns_per_op << " ns/op ("
              << fmt_num(N) << " ops in " << us / 1000 << " ms)"
              << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================
// Socket Create QPS
// ============================================================

TEST(BenchNet, SocketCreateQPS) {
    const int N = 50000;
    int success = 0;

    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        auto sock = Socket::create_tcp();
        if (sock && sock->is_valid()) {
            ++success;
            sock->close();
        }
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)success / (us / 1000000.0);
    double ns_per_op = (double)us * 1000.0 / success;

    std::cout << "[BENCH] Net Socket Create/Close QPS: " << std::fixed
              << std::setprecision(0) << qps << " ops/sec, "
              << std::setprecision(1) << ns_per_op << " ns/op ("
              << fmt_num(success) << " sockets in "
              << us / 1000 << " ms)" << std::endl;
    EXPECT_EQ(success, N);
}

// ============================================================
// Multi-client QPS with various concurrency levels
// ============================================================

static void run_multi_client_qps(int num_clients, int msgs_per_client,
                                 const IPv4Address& addr) {
    std::atomic<int> total_success{0};
    std::atomic<int> total_failures{0};
    std::vector<std::thread> threads;

    auto start = high_resolution_clock::now();

    for (int c = 0; c < num_clients; ++c) {
        threads.emplace_back([&, c, msgs_per_client]() {
            BenchClient client(addr);
            if (!client.connected()) {
                total_failures.fetch_add(msgs_per_client);
                return;
            }
            for (int i = 0; i < msgs_per_client; ++i) {
                std::string msg = "c" + std::to_string(c) + "_m" +
                                  std::to_string(i) + "_pad";
                if (!client.send_all(msg.data(), msg.size())) {
                    total_failures.fetch_add(1);
                    continue;
                }
                std::string response(msg.size(), '\0');
                if (!client.recv_all(&response[0], msg.size())) {
                    total_failures.fetch_add(1);
                    continue;
                }
                if (response == msg)
                    total_success.fetch_add(1);
                else
                    total_failures.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();
    auto end = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(end - start).count();
    int total = total_success.load() + total_failures.load();
    double qps = (double)total_success.load() / (us / 1000000.0);

    std::cout << "  clients=" << num_clients << ": " << std::fixed
              << std::setprecision(0) << qps << " echo/sec ("
              << total_success.load() << "/" << total
              << " ok, " << us / 1000 << " ms)" << std::endl;
}

TEST(BenchNet, MultiClientQPS) {
    BenchEchoServer server(16); // 16 worker threads
    std::cout << "[BENCH] Net Multi-Client Echo QPS:" << std::endl;

    // Test with 1, 10, 50, 100, 200 concurrent clients
    run_multi_client_qps(1,   10000, server.addr());
    run_multi_client_qps(10,  5000,  server.addr());
    run_multi_client_qps(50,  2000,  server.addr());
    run_multi_client_qps(100, 1000,  server.addr());
    run_multi_client_qps(200, 500,   server.addr());

    SUCCEED();
}

// ============================================================
// Large Message Throughput
// ============================================================

TEST(BenchNet, LargeMessageThroughput) {
    BenchEchoServer server;
    BenchClient client(server.addr());
    ASSERT_TRUE(client.connected());

    const size_t kMsgSize = 65536; // 64KB
    const int kNumMsgs = 1000;
    std::string msg(kMsgSize, 'L');

    auto start = high_resolution_clock::now();
    for (int i = 0; i < kNumMsgs; ++i) {
        ASSERT_TRUE(client.send_all(msg.data(), kMsgSize));
        std::string response(kMsgSize, '\0');
        ASSERT_TRUE(client.recv_all(&response[0], kMsgSize));
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();

    size_t total_bytes = kMsgSize * kNumMsgs * 2; // send + recv
    double mbps = (total_bytes / (1024.0 * 1024.0)) / (us / 1000000.0);

    std::cout << "[BENCH] Net Large Message Throughput: " << std::fixed
              << std::setprecision(2) << mbps << " MB/s ("
              << kNumMsgs << " x " << kMsgSize << "B echoes, "
              << fmt_num((long long)total_bytes) << " bytes in "
              << us / 1000 << " ms)" << std::endl;
    EXPECT_GT(mbps, 0);
}

// ============================================================
// Small Message Latency — 1-byte echo ping at scale
// ============================================================

TEST(BenchNet, SmallMessageLatency) {
    BenchEchoServer server;
    BenchClient client(server.addr());
    ASSERT_TRUE(client.connected());

    const int N = 20000;
    std::vector<long long> latencies;
    latencies.reserve(N);
    char byte = 'A';

    for (int i = 0; i < N; ++i) {
        auto t1 = high_resolution_clock::now();
        ASSERT_TRUE(client.send_all(&byte, 1));
        char echo;
        ASSERT_TRUE(client.recv_all(&echo, 1));
        auto t2 = high_resolution_clock::now();
        latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
    }

    std::sort(latencies.begin(), latencies.end());
    double avg_us = (double)std::accumulate(latencies.begin(),
        latencies.end(), 0LL) / N / 1000.0;

    std::cout << "[BENCH] Net Small Message Latency: avg=" << std::fixed
              << std::setprecision(2) << avg_us << " us, p50="
              << latencies[N * 50 / 100] / 1000.0 << " us, p99="
              << latencies[N * 99 / 100] / 1000.0 << " us ("
              << N << " samples)" << std::endl;
}

// ============================================================
// Buffer Varint Throughput
// ============================================================

TEST(BenchNet, BufferVarintThroughput) {
    Buffer buf;

    // Write benchmark
    const int N = 500000;
    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        buf.write_varint(static_cast<uint64_t>(i) * 7 + 1);
    }
    auto t2 = high_resolution_clock::now();
    auto write_us = duration_cast<microseconds>(t2 - t1).count();

    // Read benchmark
    auto t3 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        uint64_t v = 0;
        bool ok = buf.read_varint(v);
        EXPECT_TRUE(ok);
        EXPECT_EQ(v, static_cast<uint64_t>(i) * 7 + 1);
    }
    auto t4 = high_resolution_clock::now();
    auto read_us = duration_cast<microseconds>(t4 - t3).count();

    double write_qps = (double)N / (write_us / 1000000.0);
    double read_qps  = (double)N / (read_us / 1000000.0);

    std::cout << "[BENCH] Net Buffer Varint: write=" << std::fixed
              << std::setprecision(0) << write_qps << " ops/sec ("
              << write_us / 1000 << " ms), read=" << read_qps
              << " ops/sec (" << read_us / 1000 << " ms) — "
              << fmt_num(N) << " varints" << std::endl;
}

TEST(BenchNet, BufferFixedIntThroughput) {
    Buffer buf;

    const int N = 2000000; // 2M

    // Write int32
    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        buf.write_int32(i);
    }
    auto t2 = high_resolution_clock::now();
    auto write_us = duration_cast<microseconds>(t2 - t1).count();
    size_t write_bytes = (size_t)N * 4;
    double write_mbps = (write_bytes / (1024.0 * 1024.0))
                        / (write_us / 1000000.0);

    // Read int32
    auto t3 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        int32_t v = buf.read_int32();
        EXPECT_EQ(v, i);
    }
    auto t4 = high_resolution_clock::now();
    auto read_us = duration_cast<microseconds>(t4 - t3).count();
    double read_mbps = (write_bytes / (1024.0 * 1024.0))
                       / (read_us / 1000000.0);

    std::cout << "[BENCH] Net Buffer int32: write=" << std::fixed
              << std::setprecision(2) << write_mbps << " MB/s, read="
              << read_mbps << " MB/s ("
              << fmt_num(N) << " int32s, "
              << write_bytes / (1024 * 1024) << " MB)" << std::endl;
}

// ============================================================
// Buffer Prepend/Append mixed throughput
// ============================================================

TEST(BenchNet, BufferMixedOpsThroughput) {
    const int kIterations = 100000;
    const int kChunkSize = 256;
    std::string data(kChunkSize, 'M');

    Buffer buf;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        buf.append(data.data(), kChunkSize);
        if (i % 3 == 0) {
            char tmp[128];
            buf.read(tmp, 128);
        }
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();

    size_t total_written = (size_t)kIterations * kChunkSize;
    double mbps = (total_written / (1024.0 * 1024.0)) / (us / 1000000.0);

    std::cout << "[BENCH] Net Buffer Mixed Ops: " << std::fixed
              << std::setprecision(2) << mbps << " MB/s ("
              << fmt_num(kIterations) << " writes + partial reads, "
              << total_written / 1024 << " KB in " << us / 1000
              << " ms)" << std::endl;
    EXPECT_GT(mbps, 0);
}

// ============================================================
// Socket options overhead
// ============================================================

TEST(BenchNet, SocketOptionsQPS) {
    const int N = 100000;

    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);
    ASSERT_TRUE(sock->is_valid());

    auto start = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        sock->set_tcp_nodelay(true);
    }
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double qps = (double)N / (us / 1000000.0);
    double ns_per_op = (double)us * 1000.0 / N;

    std::cout << "[BENCH] Net Socket SetOption QPS: " << std::fixed
              << std::setprecision(0) << qps << " ops/sec, "
              << std::setprecision(1) << ns_per_op << " ns/op ("
              << fmt_num(N) << " ops in " << us / 1000 << " ms)"
              << std::endl;

    sock->close();
    EXPECT_GT(qps, 0);
}

// ============================================================
// Concurrent echo + throughput
// ============================================================

TEST(BenchNet, ConcurrentEchoThroughput) {
    BenchEchoServer server(8);

    const int kNumClients = 8;
    const int kMsgsPerClient = 2000;
    const size_t kMsgSize = 1024;
    std::atomic<int> success{0};
    std::atomic<unsigned long long> total_data{0};
    std::vector<std::thread> threads;

    auto start = high_resolution_clock::now();

    for (int c = 0; c < kNumClients; ++c) {
        threads.emplace_back([&, c]() {
            BenchClient client(server.addr());
            if (!client.connected()) return;

            std::string msg(kMsgSize, static_cast<char>('A' + c));

            for (int i = 0; i < kMsgsPerClient; ++i) {
                if (!client.send_all(msg.data(), kMsgSize)) continue;
                std::string response(kMsgSize, '\0');
                if (!client.recv_all(&response[0], kMsgSize)) continue;
                if (response == msg) {
                    success.fetch_add(1);
                    total_data.fetch_add(kMsgSize * 2);
                }
            }
        });
    }

    for (auto& t : threads) t.join();
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();

    double qps = (double)success.load() / (us / 1000000.0);
    double mbps = (total_data.load() / (1024.0 * 1024.0))
                  / (us / 1000000.0);

    std::cout << "[BENCH] Net Concurrent Echo: " << std::fixed
              << std::setprecision(0) << qps << " echo/sec, "
              << std::setprecision(2) << mbps << " MB/s ("
              << kNumClients << " clients x "
              << kMsgsPerClient << " msgs x "
              << kMsgSize << "B in " << us / 1000 << " ms)"
              << std::endl;
    EXPECT_GT(success.load(), 0);
}
