#pragma once

#include "test_common.h"

#include "lemo/io/fd_context.h"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace lemo_io_test {

enum IoPhase {
  kIoIdle = 0,
  kIoRegistered = 1,
  kIoWaiting = 2,
  kIoFinished = 3,
};

inline void sleep_ms(int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline bool wait_eq(const std::atomic<int>& v, int expected,
                    int timeout_ms = 5000) {
  for (int elapsed = 0; elapsed < timeout_ms; elapsed += 5) {
    if (v.load(std::memory_order_acquire) == expected) {
      return true;
    }
    sleep_ms(5);
  }
  return v.load() == expected;
}

inline bool wait_ge(const std::atomic<int>& v, int threshold,
                    int timeout_ms = 5000) {
  for (int elapsed = 0; elapsed < timeout_ms; elapsed += 5) {
    if (v.load(std::memory_order_acquire) >= threshold) {
      return true;
    }
    sleep_ms(5);
  }
  return v.load() >= threshold;
}

inline bool socket_pair(int sv[2]) {
  return ::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0;
}

/** 绑定 127.0.0.1:0 并 listen */
inline bool listen_tcp(int* listen_fd, uint16_t* port) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return false;
  }
  int on = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return false;
  }
  socklen_t len = sizeof(addr);
  if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
    ::close(fd);
    return false;
  }
  if (::listen(fd, 8) != 0) {
    ::close(fd);
    return false;
  }
  *listen_fd = fd;
  *port = ntohs(addr.sin_port);
  return true;
}

/** 主线程预建 socketpair 并注册 FdContext（hook 要求 ctx 存在） */
inline void prepare_hook_socketpair(int sv[2]) {
  LEMO_CHECK(socket_pair(sv));
  lemo::io::FdManager::Instance().del(sv[0]);
  lemo::io::FdManager::Instance().del(sv[1]);
  LEMO_CHECK(lemo::io::FdManager::Instance().get(sv[0], true) != nullptr);
  LEMO_CHECK(lemo::io::FdManager::Instance().get(sv[1], true) != nullptr);
}

inline int fill_pipe(int pipefd[2]) {
  const int wfd = pipefd[1];
  const int flags = ::fcntl(wfd, F_GETFL, 0);
  ::fcntl(wfd, F_SETFL, flags | O_NONBLOCK);
  char ch = 'F';
  int count = 0;
  while (::write(wfd, &ch, 1) > 0) {
    ++count;
  }
  LEMO_CHECK(errno == EAGAIN);
  LEMO_CHECK(count > 0);
  ::fcntl(wfd, F_SETFL, flags);
  return count;
}

inline bool pipe_with_min_fd(int min_fd, int pipefd[2]) {
  std::vector<int> trash;
  while (true) {
    if (::pipe(pipefd) != 0) {
      for (size_t i = 0; i < trash.size(); ++i) {
        ::close(trash[i]);
      }
      return false;
    }
    if (pipefd[0] >= min_fd) {
      for (size_t i = 0; i < trash.size(); ++i) {
        ::close(trash[i]);
      }
      return true;
    }
    trash.push_back(pipefd[0]);
    trash.push_back(pipefd[1]);
  }
}

}  // namespace lemo_io_test
