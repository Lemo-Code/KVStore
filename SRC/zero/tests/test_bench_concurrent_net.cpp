// test_bench_concurrent_net.cpp — Combined concurrent networking benchmarks
//
// Measures: throughput scaling with thread count (1, 2, 4, 8, 16, 32),
// many concurrent connections (up to 500), connection lifecycle rate,
// message size scaling, pipeline depth impact, mixed-size workloads,
// connect/disconnect storm, and resource limits.
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
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace zero;
using namespace std::chrono;

// ============================================================
// Helpers
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
// Scalable echo server fixture (configurable worker count)
// ============================================================

class ScalableEchoServer {
public:
    explicit ScalableEchoServer(int workers = 0)
        : port_(get_random_port()), addr_("127.0.0.1", port_) {
        if (workers > 0) {
            server_.set_worker_count(workers);
        }
        server_.set_backlog(1024);
        server_.set_reuse_port(true);
        server_.set_connection_callback([this](SocketStream::Ptr conn) {
            handle_connection(conn);
        });
        EXPECT_TRUE(server_.bind(addr_));
        EXPECT_TRUE(server_.start());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ~ScalableEchoServer() { server_.stop(); }

    const IPv4Address& addr() const { return addr_; }
    uint16_t port() const { return port_; }
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
// Raw socket client for benchmarks
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

    void close() {
        if (sock_) sock_->close();
        connected_ = false;
    }

private:
    Socket::Ptr sock_;
    bool connected_;
};

// ============================================================
// Scalability Test: QPS vs thread count (1, 2, 4, 8, 16, 32)
// ============================================================

TEST(BenchConcurrentNet, ScalabilityTest) {
    ScalableEchoServer server(32);

    std::vector<int> thread_counts = {1, 2, 4, 8, 16, 32};
    const int kMsgsPerClient = 500;
    const size_t kMsgSize = 128;

    std::cout << "[BENCH] ConcurrentNet Scalability Test:" << std::endl;
    std::cout << "  threads |   total msgs |   time(ms) |     QPS |"
              << " throughput(MB/s) | efficiency" << std::endl;
    std::cout << "  --------+--------------+------------+-----------+"
              << "------------------+------------" << std::endl;

    double baseline_qps = 0;

    for (size_t ti = 0; ti < thread_counts.size(); ++ti) {
        int num_threads = thread_counts[ti];
        int total_msgs = num_threads * kMsgsPerClient;
        std::atomic<int> success{0};
        std::atomic<int> failures{0};
        std::vector<std::thread> threads;

        auto start = high_resolution_clock::now();

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                BenchClient client(server.addr());
                if (!client.connected()) {
                    failures.fetch_add(kMsgsPerClient);
                    return;
                }
                std::string msg(kMsgSize, static_cast<char>('A' + t));
                for (int i = 0; i < kMsgsPerClient; ++i) {
                    if (!client.send_all(msg.data(), kMsgSize)) {
                        failures.fetch_add(1);
                        continue;
                    }
                    std::string response(kMsgSize, '\0');
                    if (!client.recv_all(&response[0], kMsgSize)) {
                        failures.fetch_add(1);
                        continue;
                    }
                    if (response == msg)
                        success.fetch_add(1);
                    else
                        failures.fetch_add(1);
                }
            });
        }

        for (auto& t : threads) t.join();
        auto end = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(end - start).count();

        int ok = success.load();
        double qps = ok / (us / 1000000.0);
        size_t data_bytes = (size_t)ok * kMsgSize * 2; // send + recv
        double mbps = (data_bytes / (1024.0 * 1024.0)) / (us / 1000000.0);

        if (ti == 0) baseline_qps = qps;
        double efficiency = (baseline_qps > 0)
            ? (qps / (baseline_qps * num_threads) * 100.0) : 100.0;

        std::cout << "  " << std::setw(6) << num_threads << " | "
                  << std::setw(12) << fmt_num(ok) << " | "
                  << std::setw(10) << us / 1000 << " | "
                  << std::setw(9) << std::fixed << std::setprecision(0)
                  << qps << " | "
                  << std::setw(15) << std::setprecision(2) << mbps
                  << "  | "
                  << std::setw(7) << std::setprecision(1) << efficiency
                  << "%" << std::endl;

        EXPECT_GT(ok, 0) << "No successful echoes with " << num_threads
                          << " threads";
    }
}

// ============================================================
// Many Connections: 500 concurrent connections
// ============================================================

TEST(BenchConcurrentNet, ManyConnections) {
    ScalableEchoServer server(32);

    const int kNumClients = 500;
    const int kMsgsPerClient = 10;
    const size_t kMsgSize = 64;

    std::atomic<int> success{0};
    std::atomic<int> failures{0};
    std::atomic<int> connected{0};
    std::vector<std::thread> threads;

    auto start = high_resolution_clock::now();

    for (int c = 0; c < kNumClients; ++c) {
        threads.emplace_back([&, c]() {
            BenchClient client(server.addr());
            if (!client.connected()) {
                failures.fetch_add(kMsgsPerClient);
                return;
            }
            connected.fetch_add(1);

            std::string msg(kMsgSize, static_cast<char>('a' + (c % 26)));
            for (int i = 0; i < kMsgsPerClient; ++i) {
                if (!client.send_all(msg.data(), kMsgSize)) {
                    failures.fetch_add(1);
                    continue;
                }
                std::string response(kMsgSize, '\0');
                if (!client.recv_all(&response[0], kMsgSize)) {
                    failures.fetch_add(1);
                    continue;
                }
                if (response == msg)
                    success.fetch_add(1);
                else
                    failures.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();

    int tot_ok = success.load();
    int tot_conn = connected.load();
    int total_expected = kNumClients * kMsgsPerClient;

    double qps = (tot_ok > 0) ? (tot_ok / (us / 1000000.0)) : 0;
    double conn_per_sec = (tot_conn > 0) ? (tot_conn / (us / 1000000.0)) : 0;
    size_t total_data = (size_t)tot_ok * kMsgSize * 2;
    double mbps = (total_data / (1024.0 * 1024.0)) / (us / 1000000.0);

    std::cout << "[BENCH] ConcurrentNet Many Connections:" << std::endl
              << "  target clients: " << kNumClients << std::endl
              << "  connected:      " << tot_conn << std::endl
              << "  msgs expected:  " << total_expected << std::endl
              << "  msgs ok:        " << tot_ok << std::endl
              << "  msgs failed:    " << failures.load() << std::endl
              << "  total time:     " << us / 1000 << " ms" << std::endl
              << "  connections/s:  " << std::fixed << std::setprecision(0)
              << conn_per_sec << std::endl
              << "  echoes/sec:     " << qps << std::endl
              << "  throughput:     " << std::setprecision(2) << mbps
              << " MB/s" << std::endl;
}

// ============================================================
// Many connections at higher scale (push limits)
// ============================================================

TEST(BenchConcurrentNet, ManyConnections_200) {
    ScalableEchoServer server(16);

    const int kNumClients = 200;
    const int kMsgsPerClient = 20;
    const size_t kMsgSize = 128;

    std::atomic<int> success{0};
    std::atomic<int> connected{0};
    std::vector<std::thread> threads;

    auto start = high_resolution_clock::now();

    for (int c = 0; c < kNumClients; ++c) {
        threads.emplace_back([&, c]() {
            BenchClient client(server.addr());
            if (!client.connected()) return;
            connected.fetch_add(1);

            std::string msg(kMsgSize, static_cast<char>('A' + (c % 26)));
            for (int i = 0; i < kMsgsPerClient; ++i) {
                if (!client.send_all(msg.data(), kMsgSize)) continue;
                std::string response(kMsgSize, '\0');
                if (!client.recv_all(&response[0], kMsgSize)) continue;
                if (response == msg) success.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();

    int ok = success.load();
    double qps = ok / (us / 1000000.0);
    double conn_rate = connected.load() / (us / 1000000.0);

    std::cout << "[BENCH] ConcurrentNet " << kNumClients << " clients: "
              << connected.load() << " connected, " << ok << " echoes, "
              << std::fixed << std::setprecision(0) << qps << " msg/sec, "
              << conn_rate << " conn/sec (" << us / 1000 << " ms)"
              << std::endl;
}

// ============================================================
// High-Frequency Connect/Disconnect Storm
// ============================================================

TEST(BenchConcurrentNet, ConnectDisconnectStorm) {
    uint16_t port = get_random_port();
    IPv4Address addr("127.0.0.1", port);

    TcpServer server;
    server.set_worker_count(16);
    server.set_backlog(1024);
    server.set_connection_callback([](SocketStream::Ptr conn) {
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

    const int kRounds = 2000;
    std::atomic<int> ok{0};

    auto start = high_resolution_clock::now();

    for (int r = 0; r < kRounds; ++r) {
        auto sock = Socket::create_tcp();
        if (!sock) continue;
        if (!sock->connect(addr)) {
            sock->close();
            continue;
        }
        const char* msg = "ping";
        sock->send(msg, 4);
        char buf[4];
        sock->recv(buf, 4);
        sock->close();
        ok.fetch_add(1);
    }

    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    double rate = ok.load() / (us / 1000000.0);

    std::cout << "[BENCH] ConcurrentNet Connect/Disconnect Storm: "
              << std::fixed << std::setprecision(0) << rate
              << " cycles/sec (" << ok.load() << " cycles in "
              << us / 1000 << " ms)" << std::endl;

    server.stop();
    EXPECT_GT(ok.load(), 0);
}

// ============================================================
// Throughput Scaling vs Connection Count
// ============================================================

TEST(BenchConcurrentNet, ThroughputScalingVsConnections) {
    ScalableEchoServer server(16);

    std::vector<int> conn_levels = {1, 5, 10, 25, 50, 100};
    const int kMsgsPerClient = 500;
    const size_t kMsgSize = 256;

    std::cout << "[BENCH] ConcurrentNet Throughput vs Connections:" << std::endl;
    std::cout << "  conns |    total data |   time(ms) |"
              << " throughput(MB/s) | msg/sec" << std::endl;
    std::cout << "  ------+--------------+------------+"
              << "------------------+---------" << std::endl;

    for (int num_conn : conn_levels) {
        int total_expected = num_conn * kMsgsPerClient;
        std::atomic<int> success{0};
        std::vector<std::thread> threads;

        auto start = high_resolution_clock::now();

        for (int c = 0; c < num_conn; ++c) {
            threads.emplace_back([&, c]() {
                BenchClient client(server.addr());
                if (!client.connected()) return;

                std::string msg(kMsgSize, static_cast<char>('A' + c));
                for (int i = 0; i < kMsgsPerClient; ++i) {
                    if (!client.send_all(msg.data(), kMsgSize)) continue;
                    std::string response(kMsgSize, '\0');
                    if (!client.recv_all(&response[0], kMsgSize)) continue;
                    if (response == msg) success.fetch_add(1);
                }
            });
        }

        for (auto& t : threads) t.join();
        auto end = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(end - start).count();

        int ok = success.load();
        double qps = ok / (us / 1000000.0);
        size_t data_bytes = (size_t)ok * kMsgSize * 2;
        double mbps = (data_bytes / (1024.0 * 1024.0)) / (us / 1000000.0);

        std::cout << "  " << std::setw(5) << num_conn << " | "
                  << std::setw(12) << fmt_num((long long)data_bytes) << " | "
                  << std::setw(10) << us / 1000 << " | "
                  << std::setw(15) << std::fixed << std::setprecision(2)
                  << mbps << "  | "
                  << std::setw(7) << std::setprecision(0) << qps
                  << std::endl;
    }
}

// ============================================================
// Pipeline Depth Test: effect of sending multiple messages
// without waiting for responses
// ============================================================

TEST(BenchConcurrentNet, PipelineDepth) {
    ScalableEchoServer server(8);

    const int kPipelines = 1000;
    const size_t kMsgSize = 64;

    std::vector<int> depths = {1, 4, 8, 16, 32};
    std::string msg(kMsgSize, 'P');

    std::cout << "[BENCH] ConcurrentNet Pipeline Depth:" << std::endl;
    std::cout << "  depth |   time(ms) | throughput(MB/s) | speedup" << std::endl;
    std::cout << "  ------+------------+------------------+--------" << std::endl;

    double baseline_tput = 0;

    for (size_t di = 0; di < depths.size(); ++di) {
        int depth = depths[di];

        auto start = high_resolution_clock::now();

        BenchClient client(server.addr());
        ASSERT_TRUE(client.connected());

        for (int p = 0; p < kPipelines; ++p) {
            // Send depth messages
            for (int d = 0; d < depth; ++d) {
                ASSERT_TRUE(client.send_all(msg.data(), kMsgSize));
            }
            // Receive depth responses
            std::string response(kMsgSize, '\0');
            for (int d = 0; d < depth; ++d) {
                ASSERT_TRUE(client.recv_all(&response[0], kMsgSize));
            }
        }

        auto end = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(end - start).count();

        size_t total_bytes = (size_t)kPipelines * depth * kMsgSize * 2;
        double mbps = (total_bytes / (1024.0 * 1024.0)) / (us / 1000000.0);

        if (di == 0) baseline_tput = mbps;
        double speedup = (baseline_tput > 0) ? (mbps / baseline_tput) : 1.0;

        std::cout << "  " << std::setw(5) << depth << " | "
                  << std::setw(10) << us / 1000 << " | "
                  << std::setw(15) << std::fixed << std::setprecision(2)
                  << mbps << "  | "
                  << std::setprecision(2) << speedup << "x" << std::endl;
    }
}

// ============================================================
// Message Size Scaling with Many Connections
// ============================================================

TEST(BenchConcurrentNet, MessageSizeScaling) {
    ScalableEchoServer server(8);

    const int kNumClients = 8;
    const int kMsgsPerClient = 500;

    std::vector<size_t> sizes = {16, 64, 256, 1024, 4096, 16384, 65536};

    std::cout << "[BENCH] ConcurrentNet Message Size Scaling ("
              << kNumClients << " clients x " << kMsgsPerClient
              << " msgs):" << std::endl;
    std::cout << "  size(B) |  time(ms) | throughput(MB/s) |"
              << " msg/sec" << std::endl;
    std::cout << "  --------+------------+------------------+"
              << "---------" << std::endl;

    for (size_t sz : sizes) {
        std::atomic<int> success{0};
        std::vector<std::thread> threads;

        auto start = high_resolution_clock::now();

        for (int c = 0; c < kNumClients; ++c) {
            threads.emplace_back([&, c]() {
                BenchClient client(server.addr());
                if (!client.connected()) return;

                std::string msg(sz, static_cast<char>('A' + c));
                for (int i = 0; i < kMsgsPerClient; ++i) {
                    if (!client.send_all(msg.data(), sz)) continue;
                    std::string response(sz, '\0');
                    if (!client.recv_all(&response[0], sz)) continue;
                    if (response == msg) success.fetch_add(1);
                }
            });
        }

        for (auto& t : threads) t.join();
        auto end = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(end - start).count();

        int ok = success.load();
        double qps = ok / (us / 1000000.0);
        size_t data_bytes = (size_t)ok * sz * 2;
        double mbps = (data_bytes / (1024.0 * 1024.0)) / (us / 1000000.0);

        std::cout << "  " << std::setw(7) << sz << " | "
                  << std::setw(10) << us / 1000 << " | "
                  << std::setw(15) << std::fixed << std::setprecision(2)
                  << mbps << "  | "
                  << std::setw(7) << std::setprecision(0) << qps
                  << std::endl;
    }
}

// ============================================================
// Latency distribution under load
// ============================================================

TEST(BenchConcurrentNet, LatencyUnderLoad) {
    ScalableEchoServer server(8);

    const int kNumClients = 8;
    const int kSamplesPerClient = 500;
    const size_t kMsgSize = 32;

    std::mutex lat_mtx;
    std::vector<long long> all_latencies;
    all_latencies.reserve(kNumClients * kSamplesPerClient);

    std::vector<std::thread> threads;

    auto start = high_resolution_clock::now();

    for (int c = 0; c < kNumClients; ++c) {
        threads.emplace_back([&, c]() {
            BenchClient client(server.addr());
            if (!client.connected()) return;

            std::string msg(kMsgSize, static_cast<char>('A' + c));
            std::string response(kMsgSize, '\0');
            std::vector<long long> local_lat;
            local_lat.reserve(kSamplesPerClient);

            for (int i = 0; i < kSamplesPerClient; ++i) {
                auto t1 = high_resolution_clock::now();
                if (!client.send_all(msg.data(), kMsgSize)) continue;
                if (!client.recv_all(&response[0], kMsgSize)) continue;
                auto t2 = high_resolution_clock::now();
                local_lat.push_back(
                    duration_cast<nanoseconds>(t2 - t1).count());
            }

            std::lock_guard<std::mutex> lk(lat_mtx);
            all_latencies.insert(all_latencies.end(),
                                 local_lat.begin(), local_lat.end());
        });
    }

    for (auto& t : threads) t.join();
    auto end = high_resolution_clock::now();
    auto total_us = duration_cast<microseconds>(end - start).count();

    std::sort(all_latencies.begin(), all_latencies.end());
    size_t N = all_latencies.size();

    if (N < 10) {
        std::cout << "[BENCH] ConcurrentNet Latency Under Load: not enough samples"
                  << std::endl;
        return;
    }

    double avg = (double)std::accumulate(all_latencies.begin(),
        all_latencies.end(), 0LL) / N;

    std::cout << "[BENCH] ConcurrentNet Latency Under Load (" << N
              << " samples, " << kNumClients << " clients):" << std::endl
              << "  avg=" << std::fixed << std::setprecision(2) << avg / 1000.0
              << " us, p50=" << all_latencies[N * 50 / 100] / 1000.0
              << " us, p90=" << all_latencies[N * 90 / 100] / 1000.0
              << " us, p99=" << all_latencies[N * 99 / 100] / 1000.0
              << " us, max=" << all_latencies.back() / 1000.0
              << " us" << std::endl
              << "  total test time: " << total_us / 1000 << " ms"
              << std::endl;
}

// ============================================================
// Extreme: 1000 connections with 1 message each
// ============================================================

TEST(BenchConcurrentNet, ThousandConnectionsSingleMsg) {
    ScalableEchoServer server(32);

    const int kNumClients = 1000;
    const size_t kMsgSize = 32;

    std::atomic<int> connected{0};
    std::atomic<int> success{0};
    std::vector<std::thread> threads;

    auto start = high_resolution_clock::now();

    for (int c = 0; c < kNumClients; ++c) {
        threads.emplace_back([&, c]() {
            BenchClient client(server.addr());
            if (!client.connected()) return;
            connected.fetch_add(1);

            std::string msg(kMsgSize, static_cast<char>('a' + (c % 26)));
            if (!client.send_all(msg.data(), kMsgSize)) return;
            std::string response(kMsgSize, '\0');
            if (!client.recv_all(&response[0], kMsgSize)) return;
            if (response == msg) success.fetch_add(1);
        });
    }

    for (auto& t : threads) t.join();
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();

    int conn = connected.load();
    int ok = success.load();
    double conn_rate = conn / (us / 1000000.0);
    double msg_rate = ok / (us / 1000000.0);

    std::cout << "[BENCH] ConcurrentNet 1000 Connections (1 msg each):" << std::endl
              << "  connected: " << conn << "/" << kNumClients << std::endl
              << "  succeeded: " << ok << std::endl
              << "  time:      " << us / 1000 << " ms" << std::endl
              << "  conn/sec:  " << std::fixed << std::setprecision(0)
              << conn_rate << std::endl
              << "  msg/sec:   " << msg_rate << std::endl;
}
