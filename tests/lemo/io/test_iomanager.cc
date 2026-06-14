/**
 * @file test_iomanager.cc
 * @brief IOManager 单元测试：每例单一断言目标，主/协程同步用 IoPhase。
 */
#include "test_common.h"
#include "test_io_common.h"

#include "lemo/fiber/fiber.h"
#include "lemo/io/iomanager.h"
#include "lemo/io/iomanager.h"

#include <fcntl.h>
#include <cstring>
#include <sys/socket.h>

namespace {

using lemo_io_test::fill_pipe;
using lemo_io_test::IoPhase;
using lemo_io_test::kIoFinished;
using lemo_io_test::kIoRegistered;
using lemo_io_test::kIoWaiting;
using lemo_io_test::wait_eq;
using lemo_io_test::wait_ge;

void test_timer_via_epoll() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(2, false, "test_io_timer"));
  std::atomic<int> fired{0};
  iom->addTimer(50, [&fired]() { fired.store(1); });
  LEMO_CHECK(wait_eq(fired, 1, 3000));
  iom->stop();
}

void test_timer_front_inserts_tickle() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(2, false, "test_timer_front"));
  std::atomic<int> order{0};
  iom->addTimer(500, [&order]() { order.store(2); });
  iom->addTimer(30, [&order]() {
    if (order.load() == 0) {
      order.store(1);
    }
  });
  LEMO_CHECK(wait_eq(order, 1, 3000));
  iom->stop();
}

/** READ：addEvent + epoll 唤醒 + 读数据 */
void test_pipe_read_event() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(2, false, "test_pipe_read"));
  int pipefd[2] = {-1, -1};
  LEMO_CHECK(::pipe(pipefd) == 0);

  std::atomic<int> phase{0};
  iom->schedule([iom, pipefd, &phase]() {
    LEMO_CHECK(iom->addEvent(pipefd[0], lemo::io::Reactor::READ) == 0);
    phase.store(kIoWaiting);
    lemo::fiber::Fiber::YieldToHold();
    char buf[4] = {};
    LEMO_CHECK(::read(pipefd[0], buf, 1) == 1);
    LEMO_CHECK(buf[0] == 'x');
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoWaiting, 3000));
  LEMO_CHECK(::write(pipefd[1], "x", 1) == 1);
  LEMO_CHECK(wait_eq(phase, kIoFinished, 3000));

  ::close(pipefd[0]);
  ::close(pipefd[1]);
  iom->stop();
}

/** WRITE + cancelEvent：pipe 写满后等待，cancel 唤醒（不依赖 EPOLLET 边沿时序） */
void test_write_cancel_wakeup() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(2, false, "test_write_cancel"));
  int pipefd[2] = {-1, -1};
  LEMO_CHECK(::pipe(pipefd) == 0);
  fill_pipe(pipefd);

  std::atomic<int> phase{0};
  iom->schedule([iom, pipefd, &phase]() {
    LEMO_CHECK(iom->addEvent(pipefd[1], lemo::io::Reactor::WRITE) == 0);
    phase.store(kIoWaiting);
    lemo::fiber::Fiber::YieldToHold();
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoWaiting, 3000));
  LEMO_CHECK(iom->cancelEvent(pipefd[1], lemo::io::Reactor::WRITE));
  LEMO_CHECK(wait_eq(phase, kIoFinished, 3000));

  ::close(pipefd[0]);
  ::close(pipefd[1]);
  iom->stop();
}

/** addEvent 带 callback：回调执行即成功，协程不得再 yield */
void test_add_event_with_callback() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(2, false, "test_cb_event"));
  int pipefd[2] = {-1, -1};
  LEMO_CHECK(::pipe(pipefd) == 0);

  std::atomic<int> cb_fired{0};
  iom->schedule([iom, pipefd, &cb_fired]() {
    LEMO_CHECK(iom->addEvent(pipefd[0], lemo::io::Reactor::READ,
                            [&cb_fired]() { cb_fired.store(1); }) == 0);
  });

  LEMO_CHECK(wait_eq(cb_fired, 0, 500));
  LEMO_CHECK(::write(pipefd[1], "c", 1) == 1);
  LEMO_CHECK(wait_eq(cb_fired, 1, 3000));

  ::close(pipefd[0]);
  ::close(pipefd[1]);
  iom->stop();
}

void test_del_event() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(1, false, "test_del"));
  int pipefd[2] = {-1, -1};
  LEMO_CHECK(::pipe(pipefd) == 0);

  std::atomic<int> phase{0};
  iom->schedule([iom, pipefd, &phase]() {
    LEMO_CHECK(iom->addEvent(pipefd[0], lemo::io::Reactor::READ) == 0);
    LEMO_CHECK(iom->delEvent(pipefd[0], lemo::io::Reactor::READ));
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoFinished, 3000));
  ::close(pipefd[0]);
  ::close(pipefd[1]);
  iom->stop();
}

void test_cancel_event() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(1, false, "test_cancel_io"));
  int pipefd[2] = {-1, -1};
  LEMO_CHECK(::pipe(pipefd) == 0);

  std::atomic<int> phase{0};
  iom->schedule([iom, pipefd, &phase]() {
    LEMO_CHECK(iom->addEvent(pipefd[0], lemo::io::Reactor::READ) == 0);
    LEMO_CHECK(iom->cancelEvent(pipefd[0], lemo::io::Reactor::READ));
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoFinished, 3000));
  ::close(pipefd[0]);
  ::close(pipefd[1]);
  iom->stop();
}

void test_cancel_all() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(2, false, "test_cancel_all"));
  int pipefd[2] = {-1, -1};
  LEMO_CHECK(::pipe(pipefd) == 0);

  std::atomic<int> phase{0};
  iom->schedule([iom, pipefd, &phase]() {
    LEMO_CHECK(iom->addEvent(pipefd[0], lemo::io::Reactor::READ) == 0);
    phase.store(kIoWaiting);
    lemo::fiber::Fiber::YieldToHold();
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoWaiting, 3000));
  iom->schedule([iom, pipefd, &phase]() {
    LEMO_CHECK(iom->cancelAll(pipefd[0]));
  });
  LEMO_CHECK(wait_eq(phase, kIoFinished, 3000));

  ::close(pipefd[0]);
  ::close(pipefd[1]);
  iom->stop();
}

/** 同 fd 注册 READ+WRITE 后删除 WRITE，仅 READ 路径生效 */
void test_read_write_both_events() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(2, false, "test_rw_both"));
  int sv[2] = {-1, -1};
  LEMO_CHECK(lemo_io_test::socket_pair(sv));

  std::atomic<int> phase{0};
  iom->schedule([iom, sv, &phase]() {
    LEMO_CHECK(iom->addEvent(sv[0], lemo::io::Reactor::READ) == 0);
    LEMO_CHECK(iom->addEvent(sv[0], lemo::io::Reactor::WRITE) == 0);
    LEMO_CHECK(iom->delEvent(sv[0], lemo::io::Reactor::WRITE));
    phase.store(kIoWaiting);
    lemo::fiber::Fiber::YieldToHold();
    char c = 0;
    LEMO_CHECK(::read(sv[0], &c, 1) == 1);
    LEMO_CHECK(c == 'b');
    LEMO_CHECK(::write(sv[0], "B", 1) == 1);
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoWaiting, 3000));
  LEMO_CHECK(::write(sv[1], "b", 1) == 1);
  LEMO_CHECK(wait_eq(phase, kIoFinished, 3000));

  char out = 0;
  LEMO_CHECK(::read(sv[1], &out, 1) == 1);
  LEMO_CHECK(out == 'B');

  ::close(sv[0]);
  ::close(sv[1]);
  iom->stop();
}

void test_epoll_hup() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(2, false, "test_hup"));
  int pipefd[2] = {-1, -1};
  LEMO_CHECK(::pipe(pipefd) == 0);

  std::atomic<int> phase{0};
  iom->schedule([iom, pipefd, &phase]() {
    LEMO_CHECK(iom->addEvent(pipefd[0], lemo::io::Reactor::READ) == 0);
    phase.store(kIoWaiting);
    lemo::fiber::Fiber::YieldToHold();
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoWaiting, 3000));
  ::close(pipefd[1]);
  LEMO_CHECK(wait_eq(phase, kIoFinished, 3000));

  ::close(pipefd[0]);
  iom->stop();
}

void test_invalid_fd_ops() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(1, false, "test_invalid_fd"));
  LEMO_CHECK(!iom->delEvent(99999, lemo::io::Reactor::READ));
  LEMO_CHECK(!iom->cancelEvent(99999, lemo::io::Reactor::READ));
  LEMO_CHECK(!iom->cancelAll(99999));
  iom->stop();
}

void test_fd_context_growth() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(1, false, "test_fd_grow"));
  int pipefd[2] = {-1, -1};
  LEMO_CHECK(lemo_io_test::pipe_with_min_fd(48, pipefd));

  std::atomic<int> phase{0};
  iom->schedule([iom, pipefd, &phase]() {
    LEMO_CHECK(pipefd[0] >= 48);
    LEMO_CHECK(iom->addEvent(pipefd[0], lemo::io::Reactor::READ) == 0);
    phase.store(kIoWaiting);
    lemo::fiber::Fiber::YieldToHold();
    char c = 0;
    LEMO_CHECK(::read(pipefd[0], &c, 1) == 1);
    LEMO_CHECK(c == 'g');
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoWaiting, 3000));
  LEMO_CHECK(::write(pipefd[1], "g", 1) == 1);
  LEMO_CHECK(wait_eq(phase, kIoFinished, 3000));

  ::close(pipefd[0]);
  ::close(pipefd[1]);
  iom->stop();
}

void test_get_this() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(1, false, "test_get_this"));
  std::atomic<int> ok{0};
  iom->schedule([iom, &ok]() {
    LEMO_CHECK(lemo::io::IOManager::GetThis() == iom.get());
    ok.store(1);
  });
  LEMO_CHECK(wait_eq(ok, 1, 3000));
  iom->stop();
}

void test_use_caller_thread() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(2, true, "test_use_caller"));
  int pipefd[2] = {-1, -1};
  LEMO_CHECK(::pipe(pipefd) == 0);

  std::atomic<int> phase{0};
  iom->schedule([iom, pipefd, &phase]() {
    LEMO_CHECK(iom->addEvent(pipefd[0], lemo::io::Reactor::READ) == 0);
    phase.store(kIoWaiting);
    lemo::fiber::Fiber::YieldToHold();
    char c = 0;
    LEMO_CHECK(::read(pipefd[0], &c, 1) == 1);
    LEMO_CHECK(c == 'u');
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoWaiting, 3000));
  LEMO_CHECK(::write(pipefd[1], "u", 1) == 1);
  LEMO_CHECK(wait_eq(phase, kIoFinished, 3000));

  ::close(pipefd[0]);
  ::close(pipefd[1]);
  iom->stop();
}

void test_stop_cancels_pending() {
  lemo::io::IOManager::ptr iom(new lemo::io::IOManager(1, false, "test_stop_cancel"));
  int pipefd[2] = {-1, -1};
  LEMO_CHECK(::pipe(pipefd) == 0);

  std::atomic<int> phase{0};
  iom->schedule([iom, pipefd, &phase]() {
    LEMO_CHECK(iom->addEvent(pipefd[0], lemo::io::Reactor::READ) == 0);
    phase.store(kIoWaiting);
    lemo::fiber::Fiber::YieldToHold();
    phase.store(kIoFinished);
  });

  LEMO_CHECK(wait_eq(phase, kIoWaiting, 3000));
  iom->stop();
  LEMO_CHECK(phase.load() == kIoFinished);

  ::close(pipefd[0]);
  ::close(pipefd[1]);
}

void test_fdmanager() {
  lemo::io::IOManager iom(1, false, "test_fdm");
  int pipefd[2] = {-1, -1};
  LEMO_CHECK(::pipe(pipefd) == 0);
  lemo::io::Reactor::FdSlot* ctx = iom.getFd(pipefd[0], true);
  LEMO_CHECK(ctx != nullptr);
  LEMO_CHECK(ctx->isInit());
  LEMO_CHECK(!ctx->isSocket());
  ctx->setTimeout(SO_RCVTIMEO, 1000);
  LEMO_CHECK(ctx->getTimeout(SO_RCVTIMEO) == 1000);
  ctx->setTimeout(SO_SNDTIMEO, 2000);
  LEMO_CHECK(ctx->getTimeout(SO_SNDTIMEO) == 2000);
  iom.delFd(pipefd[0]);
  ::close(pipefd[0]);
  ::close(pipefd[1]);

  int sv[2] = {-1, -1};
  LEMO_CHECK(lemo_io_test::socket_pair(sv));
  lemo::io::Reactor::FdSlot* sctx = iom.getFd(sv[0], true);
  LEMO_CHECK(sctx != nullptr);
  LEMO_CHECK(sctx->isSocket());
  LEMO_CHECK(sctx->getSysNonBlock());
  iom.delFd(sv[0]);
  iom.delFd(sv[1]);
  ::close(sv[0]);
  ::close(sv[1]);
  iom.stop();
}

}  // namespace

typedef void (*IoTestFn)();

int main(int argc, char** argv) {
  struct NamedTest {
    const char* name;
    IoTestFn fn;
  };
  const NamedTest tests[] = {
      {"timer", test_timer_via_epoll},
      {"timer_front", test_timer_front_inserts_tickle},
      {"pipe_read", test_pipe_read_event},
      {"write_cancel", test_write_cancel_wakeup},
      {"cb_event", test_add_event_with_callback},
      {"del", test_del_event},
      {"cancel", test_cancel_event},
      {"cancel_all", test_cancel_all},
      {"rw_both", test_read_write_both_events},
      {"hup", test_epoll_hup},
      {"invalid_fd", test_invalid_fd_ops},
      {"fd_grow", test_fd_context_growth},
      {"get_this", test_get_this},
      {"use_caller", test_use_caller_thread},
      {"stop_cancel", test_stop_cancels_pending},
      {"fdmanager", test_fdmanager},
  };

  if (argc > 1) {
    for (const NamedTest& t : tests) {
      if (std::strcmp(argv[1], t.name) == 0) {
        t.fn();
        std::printf("PASS test_iomanager [%s]\n", t.name);
        return 0;
      }
    }
    std::fprintf(stderr, "unknown test: %s\n", argv[1]);
    return 1;
  }

  for (const NamedTest& t : tests) {
    t.fn();
  }
  std::printf("PASS test_iomanager\n");
  return 0;
}
