/**
 * @file test_hook.cc
 * @brief Hook 单元测试：socket 在协程内注册；主线程用 ::syscall 驱动数据。
 */
#include "test_io_common.h"

#include "lemo/fiber/fiber.h"
#include "lemo/io/fd_context.h"
#include "lemo/io/hook.h"
#include "lemo/io/iomanager.h"
#include "lemo/utils/time_util.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

extern "C" int connect_with_timeout(int fd, const struct sockaddr* addr,
                                    socklen_t addrlen, uint64_t timeout_ms);

namespace {

using lemo_io_test::IoPhase;
using lemo_io_test::kIoFinished;
using lemo_io_test::kIoWaiting;
using lemo_io_test::prepare_hook_socketpair;
using lemo_io_test::wait_eq;
using lemo_io_test::wait_ge;

void test_hook_sleep() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(2, false, "test_hook_sleep"));
  std::atomic<int> done{0};
  const uint64_t start = lemo::utils::NowMs();
  iom->schedule([&done, start]() {
    LEMO_CHECK(sleep(1) == 0);
    LEMO_CHECK(lemo::utils::NowMs() - start >= 800);
    done.store(1);
  });
  LEMO_CHECK(wait_eq(done, 1, 5000));
  iom->stop();
}

void test_hook_usleep() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(2, false, "test_hook_usleep"));
  std::atomic<int> done{0};
  const uint64_t start = lemo::utils::NowMs();
  iom->schedule([&done, start]() {
    LEMO_CHECK(usleep(200000) == 0);
    LEMO_CHECK(lemo::utils::NowMs() - start >= 150);
    done.store(1);
  });
  LEMO_CHECK(wait_eq(done, 1, 3000));
  iom->stop();
}

void test_hook_nanosleep() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(2, false, "test_hook_nano"));
  std::atomic<int> done{0};
  iom->schedule([&done]() {
    timespec req = {0, 200000000};
    timespec rem = {};
    LEMO_CHECK(nanosleep(&req, &rem) == 0);
    LEMO_CHECK(rem.tv_sec == 0 && rem.tv_nsec == 0);
    done.store(1);
  });
  LEMO_CHECK(wait_eq(done, 1, 3000));
  iom->stop();
}

void test_hook_nanosleep_invalid() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(1, false, "test_hook_nano_inv"));
  std::atomic<int> done{0};
  iom->schedule([&done]() {
    errno = 0;
    LEMO_CHECK(nanosleep(nullptr, nullptr) == -1);
    LEMO_CHECK(errno == EINVAL);
    done.store(1);
  });
  LEMO_CHECK(wait_eq(done, 1, 3000));
  iom->stop();
}

void test_hook_socket_read_write() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(2, false, "test_hook_sock_rw"));
  int sv[2] = {-1, -1};
  prepare_hook_socketpair(sv);

  std::atomic<int> phase{0};
  iom->schedule([&sv, &phase]() {
    phase.store(kIoWaiting);
    char buf[4] = {};
    LEMO_CHECK(read(sv[0], buf, 1) == 1);
    LEMO_CHECK(buf[0] == 'h');
    LEMO_CHECK(write(sv[0], "p", 1) == 1);
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoWaiting, 3000));
  LEMO_CHECK(::write(sv[1], "h", 1) == 1);
  LEMO_CHECK(wait_eq(phase, kIoFinished, 3000));

  char c = 0;
  LEMO_CHECK(::read(sv[1], &c, 1) == 1);
  LEMO_CHECK(c == 'p');

  ::close(sv[0]);
  ::close(sv[1]);
  iom->stop();
}

void test_hook_recv_send() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(2, false, "test_hook_recv"));
  int sv[2] = {-1, -1};
  prepare_hook_socketpair(sv);

  std::atomic<int> phase{0};
  iom->schedule([&sv, &phase]() {
    phase.store(kIoWaiting);
    char buf[8] = {};
    LEMO_CHECK(recv(sv[0], buf, 4, 0) == 4);
    LEMO_CHECK(buf[0] == 'd' && buf[3] == 'a');
    LEMO_CHECK(send(sv[0], "OK", 2, 0) == 2);
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoWaiting, 3000));
  LEMO_CHECK(::send(sv[1], "data", 4, 0) == 4);
  LEMO_CHECK(wait_eq(phase, kIoFinished, 3000));

  char out[4] = {};
  LEMO_CHECK(::recv(sv[1], out, 2, 0) == 2);
  LEMO_CHECK(out[0] == 'O' && out[1] == 'K');

  ::close(sv[0]);
  ::close(sv[1]);
  iom->stop();
}

void test_hook_readv_writev() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(2, false, "test_hook_iov"));
  int sv[2] = {-1, -1};
  prepare_hook_socketpair(sv);

  std::atomic<int> phase{0};
  iom->schedule([&sv, &phase]() {
    phase.store(kIoWaiting);
    iovec riov[2];
    char a[2] = {};
    char b[2] = {};
    riov[0].iov_base = a;
    riov[0].iov_len = 1;
    riov[1].iov_base = b;
    riov[1].iov_len = 1;
    LEMO_CHECK(readv(sv[0], riov, 2) == 2);
    LEMO_CHECK(a[0] == '1' && b[0] == '2');

    iovec wiov[1];
    char w = 'v';
    wiov[0].iov_base = &w;
    wiov[0].iov_len = 1;
    LEMO_CHECK(writev(sv[0], wiov, 1) == 1);
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoWaiting, 3000));
  LEMO_CHECK(::write(sv[1], "12", 2) == 2);
  LEMO_CHECK(wait_eq(phase, kIoFinished, 3000));

  char c = 0;
  LEMO_CHECK(::read(sv[1], &c, 1) == 1);
  LEMO_CHECK(c == 'v');

  ::close(sv[0]);
  ::close(sv[1]);
  iom->stop();
}

void test_hook_accept() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(2, false, "test_hook_accept"));
  int listen_fd = -1;
  uint16_t port = 0;
  LEMO_CHECK(lemo_io_test::listen_tcp(&listen_fd, &port));

  std::atomic<int> client_fd{-1};
  std::atomic<int> accepted{-1};
  std::atomic<int> accept_phase{0};

  iom->schedule([&accept_phase, &accepted, listen_fd]() {
    accept_phase.store(kIoWaiting);
    sockaddr_in peer;
    socklen_t len = sizeof(peer);
    const int fd =
        accept(listen_fd, reinterpret_cast<sockaddr*>(&peer), &len);
    LEMO_CHECK(fd >= 0);
    LEMO_CHECK(lemo::io::FdManager::Instance().get(fd, false) != nullptr);
    accepted.store(fd);
    accept_phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(accept_phase, kIoWaiting, 3000));
  iom->schedule([&client_fd, port]() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    LEMO_CHECK(fd >= 0);
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    LEMO_CHECK(connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) ==
               0);
    client_fd.store(fd);
  });

  LEMO_CHECK(wait_ge(accepted, 0, 5000));
  LEMO_CHECK(client_fd.load() >= 0);
  LEMO_CHECK(wait_eq(accept_phase, kIoFinished, 3000));

  ::close(client_fd.load());
  ::close(accepted.load());
  ::close(listen_fd);
  iom->stop();
}

void test_hook_connect_timeout() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(2, false, "test_hook_conn_to"));

  std::atomic<int> done{0};
  iom->schedule([&done]() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    LEMO_CHECK(fd >= 0);
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(65535);
    inet_pton(AF_INET, "192.0.2.1", &addr.sin_addr);
    lemo::io::set_connect_timeout(200);
    errno = 0;
    const int r = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (r == -1 && errno != ETIMEDOUT) {
      std::fprintf(stderr, "connect_timeout errno=%d (%s)\n", errno,
                   strerror(errno));
    }
    LEMO_CHECK(r == -1);
    LEMO_CHECK(errno == ETIMEDOUT);
    ::close(fd);
    done.store(1);
  });

  LEMO_CHECK(wait_eq(done, 1, 5000));
  iom->stop();
}

void test_hook_read_timeout() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(2, false, "test_hook_read_to"));
  int sv[2] = {-1, -1};
  prepare_hook_socketpair(sv);

  std::atomic<int> done{0};
  iom->schedule([&sv, &done]() {
    timeval tv = {0, 200000};
    LEMO_CHECK(setsockopt(sv[0], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0);
    char c = 0;
    errno = 0;
    LEMO_CHECK(read(sv[0], &c, 1) == -1);
    LEMO_CHECK(errno == ETIMEDOUT);
    done.store(1);
  });

  LEMO_CHECK(wait_eq(done, 1, 5000));
  ::close(sv[0]);
  ::close(sv[1]);
  iom->stop();
}

void test_hook_fcntl_user_nonblock() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(1, false, "test_hook_fcntl"));
  int sv[2] = {-1, -1};
  prepare_hook_socketpair(sv);

  std::atomic<int> done{0};
  iom->schedule([&sv, &done]() {
    int flags = fcntl(sv[0], F_GETFL, 0);
    LEMO_CHECK((flags & O_NONBLOCK) == 0);
    LEMO_CHECK(fcntl(sv[0], F_SETFL, flags | O_NONBLOCK) == 0);
    LEMO_CHECK(fcntl(sv[0], F_GETFL, 0) & O_NONBLOCK);

    char c = 0;
    errno = 0;
    LEMO_CHECK(read(sv[0], &c, 1) == -1);
    LEMO_CHECK(errno == EAGAIN);
    done.store(1);
  });

  LEMO_CHECK(wait_eq(done, 1, 3000));
  ::close(sv[0]);
  ::close(sv[1]);
  iom->stop();
}

void test_hook_ioctl_fionbio() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(1, false, "test_hook_ioctl"));
  int sv[2] = {-1, -1};
  prepare_hook_socketpair(sv);

  std::atomic<int> done{0};
  iom->schedule([&sv, &done]() {
    int nb = 1;
    LEMO_CHECK(ioctl(sv[0], FIONBIO, &nb) == 0);
    char c = 0;
    errno = 0;
    LEMO_CHECK(read(sv[0], &c, 1) == -1);
    LEMO_CHECK(errno == EAGAIN);
    done.store(1);
  });

  LEMO_CHECK(wait_eq(done, 1, 3000));
  ::close(sv[0]);
  ::close(sv[1]);
  iom->stop();
}

void test_hook_close_while_waiting() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(1, false, "test_hook_close"));
  int sv[2] = {-1, -1};
  prepare_hook_socketpair(sv);

  std::atomic<int> phase{0};
  iom->schedule([&sv, &phase]() {
    phase.store(kIoWaiting);
    char c = 0;
    LEMO_CHECK(read(sv[0], &c, 1) == -1);
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoWaiting, 3000));
  lemo_io_test::sleep_ms(100);
  iom->schedule([&sv]() { close(sv[0]); });
  LEMO_CHECK(wait_eq(phase, kIoFinished, 5000));

  ::close(sv[1]);
  iom->stop();
}

void test_hook_disabled() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(1, false, "test_hook_off"));
  std::atomic<int> done{0};
  iom->schedule([&done]() {
    lemo::io::set_hook_enable(false);
    LEMO_CHECK(!lemo::io::is_hook_enable());
    const uint64_t t0 = lemo::utils::NowMs();
    LEMO_CHECK(usleep(200000) == 0);
    LEMO_CHECK(lemo::utils::NowMs() - t0 >= 150);
    lemo::io::set_hook_enable(true);
    LEMO_CHECK(lemo::io::is_hook_enable());
    done.store(1);
  });
  LEMO_CHECK(wait_eq(done, 1, 5000));
  iom->stop();
}

void test_hook_connect_with_timeout_api() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(1, false, "test_cwt_api"));
  int listen_fd = -1;
  uint16_t port = 0;
  LEMO_CHECK(lemo_io_test::listen_tcp(&listen_fd, &port));
  ::close(listen_fd);

  std::atomic<int> done{0};
  iom->schedule([&done, port]() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    errno = 0;
    const int r = connect_with_timeout(fd, reinterpret_cast<sockaddr*>(&addr),
                                       sizeof(addr), 150);
    LEMO_CHECK(r == -1);
    LEMO_CHECK(errno == ETIMEDOUT);
    ::close(fd);
    done.store(1);
  });
  LEMO_CHECK(wait_eq(done, 1, 5000));
  iom->stop();
}

}  // namespace

int main() {
  test_hook_sleep();
  test_hook_usleep();
  test_hook_nanosleep();
  test_hook_nanosleep_invalid();
  test_hook_socket_read_write();
  test_hook_recv_send();
  test_hook_readv_writev();
  test_hook_accept();
  test_hook_connect_timeout();
  test_hook_read_timeout();
  test_hook_fcntl_user_nonblock();
  test_hook_ioctl_fionbio();
  test_hook_close_while_waiting();
  test_hook_disabled();
  test_hook_connect_with_timeout_api();
  std::printf("PASS test_hook\n");
  return 0;
}
