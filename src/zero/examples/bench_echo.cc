/**
 * @file    bench_echo.cpp
 * @brief   Echo client load generator — benchmarks any echo server.
 *
 * Usage:
 *   1. Start echo server:  ./echo_server 9999
 *   2. Run benchmark:      ./bench_echo 127.0.0.1 9999 50 10
 *
 * Measures: QPS, latency (avg/p50/p99), bandwidth
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <thread>
#include <algorithm>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std::chrono;

struct Stats {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> total_bytes{0};
    std::atomic<uint64_t> total_errors{0};
    std::vector<uint64_t> latencies;
    std::mutex lat_mutex;

    void recordLatency(uint64_t us) {
        std::lock_guard<std::mutex> lock(lat_mutex);
        if (latencies.size() < 200000) latencies.push_back(us);
    }

    void report(double sec) {
        uint64_t reqs = total_requests.load();
        uint64_t bytes = total_bytes.load();
        uint64_t errs = total_errors.load();

        printf("\n╔══════════════════════════════╗\n");
        printf("║     Echo Benchmark Results   ║\n");
        printf("╠══════════════════════════════╣\n");
        printf("║ Duration:    %8.2f s      ║\n", sec);
        printf("║ Requests:    %10lu       ║\n", reqs);
        printf("║ Throughput:  %8.0f req/s  ║\n", reqs / sec);
        printf("║ Bandwidth:   %8.2f MB/s   ║\n", bytes / sec / 1048576);
        printf("║ Errors:      %10lu       ║\n", errs);
        printf("╠══════════════════════════════╣\n");

        if (!latencies.empty()) {
            std::sort(latencies.begin(), latencies.end());
            size_t n = latencies.size();
            uint64_t sum = 0;
            for (auto l : latencies) sum += l;
            printf("║ Samples:     %10zu       ║\n", n);
            printf("║ Avg latency: %7.0f us      ║\n", (double)sum / n);
            printf("║ P50 latency: %7lu us      ║\n", latencies[n/2]);
            printf("║ P90 latency: %7lu us      ║\n", latencies[n*9/10]);
            printf("║ P99 latency: %7lu us      ║\n", latencies[n*99/100]);
        }
        printf("╚══════════════════════════════╝\n");
    }
};

// Single-connection worker: sends echo requests, measures latency
void clientWorker(int fd, int duration_sec, Stats& stats) {
    char buf[256], rbuf[256];
    auto deadline = steady_clock::now() + seconds(duration_sec);
    int msg_id = 0;

    while (steady_clock::now() < deadline) {
        int len = snprintf(buf, sizeof(buf), "ECHO-%d-%06d", fd, msg_id++);
        auto t0 = steady_clock::now();

        // Write with full retry
        ssize_t nw;
        do { nw = write(fd, buf, len); } while (nw < 0 && errno == EAGAIN);
        if (nw < 0) { stats.total_errors++; break; }

        // Read with full retry
        ssize_t nr;
        do { nr = read(fd, rbuf, sizeof(rbuf)); } while (nr < 0 && errno == EAGAIN);
        if (nr <= 0) { if (nr < 0) stats.total_errors++; break; }

        auto t1 = steady_clock::now();
        stats.recordLatency(duration_cast<microseconds>(t1 - t0).count());
        stats.total_requests++;
        stats.total_bytes += len + nr;
    }
}

int main(int argc, char** argv) {
    const char* host   = (argc > 1) ? argv[1] : "127.0.0.1";
    int port           = (argc > 2) ? atoi(argv[2]) : 9999;
    int connections    = (argc > 3) ? atoi(argv[3]) : 50;
    int duration       = (argc > 4) ? atoi(argv[4]) : 10;

    printf("=== Zero Echo Benchmark ===\n");
    printf("Target:  %s:%d\n", host, port);
    printf("Conns:   %d\n", connections);
    printf("Time:    %d s\n\n", duration);

    // Establish connections
    printf("Connecting %d clients...\n", connections);
    std::vector<int> fds;
    for (int i = 0; i < connections; ++i) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host, &addr.sin_addr);

        if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(fd);
            continue;
        }
        fds.push_back(fd);
    }
    printf("Connected: %zu/%d\n\n", fds.size(), connections);

    if (fds.empty()) {
        fprintf(stderr, "No connections established. Is the server running?\n");
        return 1;
    }

    // Start benchmark
    Stats stats;
    auto t0 = steady_clock::now();

    std::vector<std::thread> threads;
    for (int fd : fds) {
        threads.emplace_back(clientWorker, fd, duration, std::ref(stats));
    }

    printf("Benchmark running for %d seconds...\n", duration);

    for (auto& t : threads) t.join();
    auto t1 = steady_clock::now();

    // Cleanup
    for (int fd : fds) close(fd);

    double elapsed = duration_cast<microseconds>(t1 - t0).count() / 1e6;
    stats.report(elapsed);

    return 0;
}
