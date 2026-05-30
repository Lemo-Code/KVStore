/**
 * @file bench_ring_buffer.cc
 * @brief RingBuffer readv 零拷贝 vs 常见 buffer 方案性能对比。
 */
#include "buffer/ring_buffer.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

namespace {

using Clock = std::chrono::high_resolution_clock;

struct BenchConfig {
  size_t total_bytes = 64 * 1024 * 1024;
  size_t chunk_size = 4096;
  size_t buf_capacity = 16 * 1024;
  int rounds = 5;
  bool quick = false;
};

struct BenchRow {
  const char* scene;
  const char* impl;
  double mb_per_s;
  double ns_per_io;
  double vs_net_pct;
};

BenchConfig ParseConfig(int argc, char** argv) {
  BenchConfig cfg;
  if (std::getenv("NET_BENCH_QUICK")) {
    cfg.quick = true;
  }
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--quick") == 0) {
      cfg.quick = true;
    } else if (std::strcmp(argv[i], "--bytes") == 0 && i + 1 < argc) {
      cfg.total_bytes = static_cast<size_t>(std::strtoull(argv[++i], nullptr, 10));
    } else if (std::strcmp(argv[i], "--chunk") == 0 && i + 1 < argc) {
      cfg.chunk_size = static_cast<size_t>(std::strtoull(argv[++i], nullptr, 10));
    } else if (std::strcmp(argv[i], "--buf") == 0 && i + 1 < argc) {
      cfg.buf_capacity = static_cast<size_t>(std::strtoull(argv[++i], nullptr, 10));
    }
  }
  if (cfg.quick) {
    cfg.total_bytes = 8 * 1024 * 1024;
    cfg.rounds = 3;
  }
  return cfg;
}

class ClassicRing {
 public:
  explicit ClassicRing(size_t cap) : cap_(cap), buf_(cap) {}

  size_t writable() const { return cap_ - used_; }
  size_t readable() const { return used_; }

  size_t tailPos() const { return (head_ + used_) % cap_; }

  size_t writableContiguous() const {
    if (used_ == 0) {
      return cap_ - head_;
    }
    const size_t tp = tailPos();
    return cap_ - tp;
  }

  void writeBytes(const void* src, size_t n) {
    const auto* p = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < n; ++i) {
      buf_[(tailPos() + i) % cap_] = p[i];
    }
    used_ += n;
  }

  void consume(size_t n) {
    head_ = (head_ + n) % cap_;
    used_ -= n;
    if (used_ == 0) {
      head_ = 0;
    }
  }

  void primeWrap(size_t primed_write, size_t to_consume) {
    std::vector<uint8_t> fill(primed_write, 'p');
    writeBytes(fill.data(), fill.size());
    consume(to_consume);
  }

 private:
  size_t cap_;
  size_t head_ = 0;
  size_t used_ = 0;
  std::vector<uint8_t> buf_;
};

class LinearBuffer {
 public:
  explicit LinearBuffer(size_t cap) : buf_(cap), rd_(0), wr_(0) {}

  size_t writable() const { return buf_.size() - wr_; }
  size_t readable() const { return wr_ - rd_; }
  char* writePtr() { return buf_.data() + wr_; }
  void hasWritten(size_t n) { wr_ += n; }
  void consume(size_t n) { rd_ += n; }

  void ensureWritable(size_t n) {
    if (writable() >= n) {
      return;
    }
    if (readable() + writable() >= n) {
      const size_t r = readable();
      std::memmove(buf_.data(), buf_.data() + rd_, r);
      rd_ = 0;
      wr_ = r;
      return;
    }
    buf_.resize(wr_ + n);
  }

  void append(const void* data, size_t n) {
    ensureWritable(n);
    std::memcpy(writePtr(), data, n);
    wr_ += n;
  }

 private:
  std::vector<char> buf_;
  size_t rd_;
  size_t wr_;
};

ssize_t WriteAll(int fd, const void* data, size_t len) {
  const auto* p = static_cast<const uint8_t*>(data);
  size_t off = 0;
  while (off < len) {
    const ssize_t n = ::write(fd, p + off, len - off);
    if (n <= 0) {
      return n;
    }
    off += static_cast<size_t>(n);
  }
  return static_cast<ssize_t>(len);
}

template <typename ReaderFn>
size_t RunIoBench(const BenchConfig& cfg, ReaderFn reader) {
  int fds[2];
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
    return 0;
  }

  std::vector<uint8_t> chunk(cfg.chunk_size, 'x');
  size_t io_ops = 0;
  size_t received = 0;

  std::thread writer([&]() {
    const int wfd = fds[1];
    size_t sent = 0;
    while (sent < cfg.total_bytes) {
      const size_t n = std::min(cfg.chunk_size, cfg.total_bytes - sent);
      if (WriteAll(wfd, chunk.data(), n) <= 0) {
        break;
      }
      sent += n;
    }
    ::close(wfd);
  });

  reader(fds[0], io_ops, received);
  ::close(fds[0]);
  writer.join();

  return (received == cfg.total_bytes) ? io_ops : 0;
}

size_t BenchNetReadv(const BenchConfig& cfg, bool wrap) {
  return RunIoBench(cfg, [&](int fd, size_t& io_ops, size_t& received) {
    net::RingBuffer buf(cfg.buf_capacity);
    if (wrap && cfg.buf_capacity > cfg.chunk_size) {
      std::vector<uint8_t> prime(cfg.buf_capacity - cfg.chunk_size, 'p');
      buf.write(prime.data(), prime.size());
      buf.consume(prime.size() / 2);
    }
    while (received < cfg.total_bytes) {
      const ssize_t n = buf.readFd(fd, cfg.chunk_size);
      if (n <= 0) {
        break;
      }
      received += static_cast<size_t>(n);
      ++io_ops;
      buf.consume(static_cast<size_t>(n));
    }
  });
}

size_t BenchClassicMemcpy(const BenchConfig& cfg, bool wrap) {
  return RunIoBench(cfg, [&](int fd, size_t& io_ops, size_t& received) {
    ClassicRing ring(cfg.buf_capacity);
    if (wrap && cfg.buf_capacity > cfg.chunk_size) {
      const size_t prime = cfg.buf_capacity - cfg.chunk_size;
      ring.primeWrap(prime, prime / 2);
    }
    std::vector<uint8_t> stack(cfg.chunk_size);
    while (received < cfg.total_bytes) {
      if (ring.writable() == 0) {
        ring.consume(std::min(ring.readable(), cfg.chunk_size));
        continue;
      }
      const size_t want = std::min(cfg.chunk_size,
                                   std::min(ring.writable(), ring.writableContiguous()));
      const ssize_t n = ::read(fd, stack.data(), want);
      if (n <= 0) {
        break;
      }
      ring.writeBytes(stack.data(), static_cast<size_t>(n));
      ring.consume(static_cast<size_t>(n));
      received += static_cast<size_t>(n);
      ++io_ops;
    }
  });
}

size_t BenchLinearReadv(const BenchConfig& cfg, bool /*wrap*/) {
  return RunIoBench(cfg, [&](int fd, size_t& io_ops, size_t& received) {
    LinearBuffer buf(cfg.buf_capacity);
    while (received < cfg.total_bytes) {
      buf.ensureWritable(cfg.chunk_size);
      const size_t w = std::min(cfg.chunk_size, buf.writable());
      struct iovec iov = {buf.writePtr(), w};
      const ssize_t n = ::readv(fd, &iov, 1);
      if (n <= 0) {
        break;
      }
      buf.hasWritten(static_cast<size_t>(n));
      buf.consume(static_cast<size_t>(n));
      received += static_cast<size_t>(n);
      ++io_ops;
    }
  });
}

size_t BenchLinearRead(const BenchConfig& cfg, bool /*wrap*/) {
  return RunIoBench(cfg, [&](int fd, size_t& io_ops, size_t& received) {
    LinearBuffer buf(cfg.buf_capacity);
    std::vector<uint8_t> stack(cfg.chunk_size);
    while (received < cfg.total_bytes) {
      const ssize_t n = ::read(fd, stack.data(), cfg.chunk_size);
      if (n <= 0) {
        break;
      }
      buf.append(stack.data(), static_cast<size_t>(n));
      buf.consume(static_cast<size_t>(n));
      received += static_cast<size_t>(n);
      ++io_ops;
    }
  });
}

size_t BenchSylarStyle(const BenchConfig& cfg, bool /*wrap*/) {
  return RunIoBench(cfg, [&](int fd, size_t& io_ops, size_t& received) {
    LinearBuffer buf(cfg.buf_capacity);
    std::vector<char> extra(cfg.chunk_size);
    while (received < cfg.total_bytes) {
      buf.ensureWritable(1);
      const size_t w = std::min(buf.writable(), cfg.chunk_size);
      const size_t extra_len = cfg.chunk_size - w;
      struct iovec iov[2];
      int cnt = 1;
      iov[0].iov_base = buf.writePtr();
      iov[0].iov_len = w;
      if (extra_len > 0) {
        iov[1].iov_base = extra.data();
        iov[1].iov_len = extra_len;
        cnt = 2;
      }
      const ssize_t n = ::readv(fd, iov, cnt);
      if (n <= 0) {
        break;
      }
      if (static_cast<size_t>(n) <= w) {
        buf.hasWritten(static_cast<size_t>(n));
      } else {
        buf.hasWritten(w);
        buf.append(extra.data(), static_cast<size_t>(n) - w);
      }
      const size_t drain = static_cast<size_t>(n);
      buf.consume(drain);
      received += drain;
      ++io_ops;
    }
  });
}

struct Impl {
  const char* name;
  size_t (*fn)(const BenchConfig&, bool);
};

const Impl kImpls[] = {
    {"net_readv", BenchNetReadv},
    {"classic_memcpy", BenchClassicMemcpy},
    {"linear_readv", BenchLinearReadv},
    {"linear_read", BenchLinearRead},
    {"sylar_style", BenchSylarStyle},
};

BenchRow Measure(const BenchConfig& cfg, const char* scene, bool wrap,
                 const Impl& impl, double net_mb) {
  BenchRow row = {scene, impl.name, 0, 0, 0};
  double best_ms = 1e100;
  size_t best_ops = 0;

  for (int i = 0; i < cfg.rounds; ++i) {
    const auto t0 = Clock::now();
    const size_t ops = impl.fn(cfg, wrap);
    const auto t1 = Clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    if (ops > 0 && ms < best_ms) {
      best_ms = ms;
      best_ops = ops;
    }
  }

  if (best_ops == 0) {
    return row;
  }
  const double sec = best_ms / 1000.0;
  row.mb_per_s = (cfg.total_bytes / (1024.0 * 1024.0)) / sec;
  row.ns_per_io = (sec * 1e9) / static_cast<double>(best_ops);
  row.vs_net_pct = (net_mb > 0) ? row.mb_per_s / net_mb * 100.0 : 100.0;
  return row;
}

void PrintHeader(const BenchConfig& cfg) {
  std::printf(
      "RingBuffer 零拷贝对比 | total=%zuMB chunk=%zu buf=%zuKB rounds=%d\n",
      cfg.total_bytes / (1024 * 1024), cfg.chunk_size, cfg.buf_capacity / 1024,
      cfg.rounds);
  std::printf("%-6s %-16s %10s %12s %10s\n", "scene", "impl", "MB/s",
              "ns/io", "vs_net%");
  std::printf("%-6s %-16s %10s %12s %10s\n", "-----", "----------------",
              "----------", "------------", "----------");
}

void PrintRow(const BenchRow& r) {
  std::printf("%-6s %-16s %10.1f %12.0f %9.1f%%\n", r.scene, r.impl, r.mb_per_s,
              r.ns_per_io, r.vs_net_pct);
}

}  // namespace

int main(int argc, char** argv) {
  const BenchConfig cfg = ParseConfig(argc, argv);
  PrintHeader(cfg);

  for (const char* scene : {"flat", "wrap"}) {
    const bool wrap = (std::strcmp(scene, "wrap") == 0);
    double net_mb = 0;

    for (const Impl& impl : kImpls) {
      if (std::strcmp(impl.name, "net_readv") != 0) {
        continue;
      }
      const BenchRow net = Measure(cfg, scene, wrap, impl, 0);
      net_mb = net.mb_per_s;
      PrintRow(net);
    }

    for (const Impl& impl : kImpls) {
      if (std::strcmp(impl.name, "net_readv") == 0) {
        continue;
      }
      if (wrap && (std::strcmp(impl.name, "linear_readv") == 0 ||
                   std::strcmp(impl.name, "linear_read") == 0 ||
                   std::strcmp(impl.name, "sylar_style") == 0)) {
        continue;
      }
      PrintRow(Measure(cfg, scene, wrap, impl, net_mb));
    }
    std::printf("\n");
  }

  std::printf(
      "解读:\n"
      "  flat  — 连续可写区，linear_readv 与 net_readv 均接近 1 段 readv\n"
      "  wrap  — 环回后 net_readv 单次 readv 写 2 段；classic 需分段 read+memcpy\n"
      "  vs_net%% — 相对 net_readv 吞吐（越高越快）\n");
  return 0;
}
