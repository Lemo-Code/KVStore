/**
 * @file bench_echo_server.cc
 * @brief Echo 服务器性能压测（IOManager + 协程 + hook）。
 *
 * 协议：客户端发送 payload 字节，服务端原样回显。
 *
 * 运行：
 *   bin/lemo/socket/bench_echo_server --mode local
 *   bin/net/bench_echo_server --mode server --port 9000 --threads 4
 *   bin/net/bench_echo_server --mode client --host 127.0.0.1 --port 9000
 *   bin/net/bench_echo_server --quick
 */
#include "lemo/io/iomanager.h"
#include "lemo/socket/address.h"
#include "lemo/socket/socket.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Us = std::chrono::microseconds;

enum class Mode { kLocal, kServer, kClient };

struct Config {
  Mode mode = Mode::kLocal;
  std::string host = "127.0.0.1";
  uint16_t port = 0;
  int threads = 4;
  int connections = 64;
  int messages = 1000;
  int payload = 128;
  bool quick = false;
};

struct Result {
  uint64_t roundtrips = 0;
  int payload = 0;
  double wall_ms = 0;
  double req_per_s = 0;
  double mb_per_s = 0;
  double us_per_req = 0;
};

std::atomic<bool> g_stop{false};

void OnSignal(int) { g_stop.store(true, std::memory_order_release); }

Config ParseConfig(int argc, char** argv) {
  Config cfg;
  if (std::getenv("LEMO_BENCH_QUICK") != nullptr) {
    cfg.quick = true;
  }
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--quick") == 0) {
      cfg.quick = true;
    } else if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
      const char* m = argv[++i];
      if (std::strcmp(m, "server") == 0) {
        cfg.mode = Mode::kServer;
      } else if (std::strcmp(m, "client") == 0) {
        cfg.mode = Mode::kClient;
      } else {
        cfg.mode = Mode::kLocal;
      }
    } else if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
      cfg.host = argv[++i];
    } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      cfg.port = static_cast<uint16_t>(std::atoi(argv[++i]));
    } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
      cfg.threads = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--connections") == 0 && i + 1 < argc) {
      cfg.connections = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--messages") == 0 && i + 1 < argc) {
      cfg.messages = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--payload") == 0 && i + 1 < argc) {
      cfg.payload = std::atoi(argv[++i]);
    }
  }
  if (cfg.quick) {
    cfg.threads = 2;
    cfg.connections = 16;
    cfg.messages = 200;
    cfg.payload = 64;
  }
  if (cfg.threads < 1) {
    cfg.threads = 1;
  }
  if (cfg.connections < 1) {
    cfg.connections = 1;
  }
  if (cfg.messages < 1) {
    cfg.messages = 1;
  }
  if (cfg.payload < 1) {
    cfg.payload = 1;
  }
  return cfg;
}

void WaitUntil(const std::atomic<uint64_t>& v, uint64_t target) {
  while (v.load(std::memory_order_acquire) < target) {
    std::this_thread::yield();
  }
}

uint16_t GetBoundPort(const lemo::socket::Socket::ptr& listen) {
  lemo::socket::Address::ptr addr = listen->getLocalAddress();
  lemo::socket::IPv4Address::ptr v4 =
      std::dynamic_pointer_cast<lemo::socket::IPv4Address>(addr);
  if (v4) {
    return v4->getPort();
  }
  lemo::socket::IPv6Address::ptr v6 =
      std::dynamic_pointer_cast<lemo::socket::IPv6Address>(addr);
  if (v6) {
    return v6->getPort();
  }
  return 0;
}

bool EchoSession(const lemo::socket::Socket::ptr& sock, size_t buf_size) {
  std::vector<char> buf(buf_size);
  char* const data = buf.data();
  while (true) {
    const int n = sock->recv(data, buf_size);
    if (n <= 0) {
      break;
    }
    int sent = 0;
    while (sent < n) {
      const int w = sock->send(data + static_cast<size_t>(sent),
                               static_cast<size_t>(n - sent));
      if (w <= 0) {
        sock->close();
        return false;
      }
      sent += w;
    }
  }
  sock->close();
  return true;
}

lemo::socket::Socket::ptr MakeListenSocket(const std::string& host, uint16_t port) {
  lemo::socket::Socket::ptr listen = lemo::socket::Socket::CreateTCPSocket();
  lemo::socket::IPv4Address::ptr addr =
      lemo::socket::IPv4Address::Create(host.c_str(), port);
  if (!addr || !listen->bind(addr) || !listen->listen(4096)) {
    return nullptr;
  }
  return listen;
}

void StartAcceptLoop(lemo::io::IOManager* iom,
                     const lemo::socket::Socket::ptr& listen,
                     const std::atomic<bool>& stop, size_t buf_size) {
  while (!stop.load(std::memory_order_acquire)) {
    lemo::socket::Socket::ptr client = listen->accept();
    if (!client) {
      if (stop.load(std::memory_order_acquire)) {
        break;
      }
      continue;
    }
    iom->schedule([client, buf_size]() { EchoSession(client, buf_size); });
  }
}

Result RunClientBench(lemo::io::IOManager* iom, const std::string& host,
                      uint16_t port, const Config& cfg) {
  const uint64_t target =
      static_cast<uint64_t>(cfg.connections) *
      static_cast<uint64_t>(cfg.messages);
  std::atomic<uint64_t> done{0};

  const auto t0 = Clock::now();
  for (int i = 0; i < cfg.connections; ++i) {
    iom->schedule([host, port, cfg, &done]() {
      std::vector<char> payload(static_cast<size_t>(cfg.payload), 'E');
      std::vector<char> recvbuf(static_cast<size_t>(cfg.payload));
      lemo::socket::Socket::ptr sock = lemo::socket::Socket::CreateTCPSocket();
      lemo::socket::IPv4Address::ptr addr =
          lemo::socket::IPv4Address::Create(host.c_str(), port);
      if (!addr || !sock->connect(addr)) {
        return;
      }
      for (int m = 0; m < cfg.messages; ++m) {
        if (sock->send(payload.data(), payload.size()) !=
            static_cast<int>(payload.size())) {
          break;
        }
        if (sock->recv(&recvbuf[0], recvbuf.size()) !=
            static_cast<int>(recvbuf.size())) {
          break;
        }
        done.fetch_add(1, std::memory_order_relaxed);
      }
      sock->close();
    });
  }

  WaitUntil(done, target);
  const Us wall =
      std::chrono::duration_cast<Us>(Clock::now() - t0);

  Result r;
  r.roundtrips = done.load();
  r.payload = cfg.payload;
  r.wall_ms = wall.count() / 1000.0;
  if (wall.count() > 0) {
    r.req_per_s = r.roundtrips * 1e6 / wall.count();
    r.us_per_req = wall.count() * 1.0 / r.roundtrips;
    r.mb_per_s =
        r.roundtrips * cfg.payload * 2.0 / 1024.0 / 1024.0 * 1e6 / wall.count();
  }
  return r;
}

void PrintResult(const char* tag, const Config& cfg, const Result& r) {
  std::printf(
      "[%s] threads=%d connections=%d messages/conn=%d payload=%dB\n"
      "  roundtrips=%llu wall=%.2f ms throughput=%.0f req/s  bw=%.2f MiB/s  "
      "latency=%.2f us/req\n",
      tag, cfg.threads, cfg.connections, cfg.messages, cfg.payload,
      static_cast<unsigned long long>(r.roundtrips), r.wall_ms, r.req_per_s,
      r.mb_per_s, r.us_per_req);
  std::fflush(stdout);
}

Result RunLocalBench(const Config& cfg) {
  lemo::io::IOManager iom(static_cast<size_t>(cfg.threads), true, "echo_local");
  lemo::socket::Socket::ptr listen = MakeListenSocket("127.0.0.1", 0);
  if (!listen) {
    std::fprintf(stderr, "failed to bind listen socket\n");
    std::exit(1);
  }
  const uint16_t port = GetBoundPort(listen);

  std::atomic<bool> stop{false};
  const size_t buf_size =
      static_cast<size_t>(cfg.payload > 4096 ? cfg.payload : 4096);

  iom.schedule([&]() {
    StartAcceptLoop(&iom, listen, stop, buf_size);
  });

  Result r = RunClientBench(&iom, "127.0.0.1", port, cfg);

  stop.store(true, std::memory_order_release);
  const int fd = listen->getSocket();
  if (fd >= 0) {
    ::close(fd);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  iom.stop();
  return r;
}

void RunServer(const Config& cfg) {
  std::signal(SIGINT, OnSignal);
  std::signal(SIGTERM, OnSignal);

  lemo::io::IOManager iom(static_cast<size_t>(cfg.threads), false, "echo_server");
  lemo::socket::Socket::ptr listen = MakeListenSocket("0.0.0.0", cfg.port);
  if (!listen) {
    std::fprintf(stderr, "failed to bind 0.0.0.0:%u\n", cfg.port);
    std::exit(1);
  }
  const uint16_t port = GetBoundPort(listen);
  const size_t buf_size =
      static_cast<size_t>(cfg.payload > 4096 ? cfg.payload : 4096);

  std::printf("echo server listening on 0.0.0.0:%u threads=%d (Ctrl+C to stop)\n",
              port, cfg.threads);
  std::fflush(stdout);

  iom.schedule([&]() { StartAcceptLoop(&iom, listen, g_stop, buf_size); });

  while (!g_stop.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  const int fd = listen->getSocket();
  if (fd >= 0) {
    ::close(fd);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  iom.stop();
}

void RunClient(const Config& cfg) {
  lemo::io::IOManager iom(static_cast<size_t>(cfg.threads), true, "echo_client");
  if (cfg.port == 0) {
    std::fprintf(stderr, "client mode requires --port\n");
    std::exit(1);
  }
  Result r = RunClientBench(&iom, cfg.host, cfg.port, cfg);
  PrintResult("client", cfg, r);
  iom.stop();
}

}  // namespace

int main(int argc, char** argv) {
  const Config cfg = ParseConfig(argc, argv);

  std::printf("bench_echo_server mode=");
  switch (cfg.mode) {
    case Mode::kServer:
      std::printf("server");
      break;
    case Mode::kClient:
      std::printf("client");
      break;
    default:
      std::printf("local");
      break;
  }
  std::printf(" threads=%d connections=%d messages=%d payload=%dB%s\n",
              cfg.threads, cfg.connections, cfg.messages, cfg.payload,
              cfg.quick ? " (quick)" : "");
  std::fflush(stdout);

  switch (cfg.mode) {
    case Mode::kServer:
      RunServer(cfg);
      break;
    case Mode::kClient:
      RunClient(cfg);
      break;
    default: {
      const Result r = RunLocalBench(cfg);
      PrintResult("local", cfg, r);
      break;
    }
  }
  return 0;
}
