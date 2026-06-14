/**
 * @file test_io_db.cc
 * @brief IO/DB 分离：解析入队 → DB Worker → ReplyRouter → Session 写回
 */
#include "../test_common.h"

#include "ledis/command/command.h"
#include "ledis/config/ledis_settings.h"
#include "ledis/protocol/resp_writer.h"
#include "ledis/session/io_worker.h"
#include "ledis/session/ledis_engine.h"
#include "ledis/session/session.h"

#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {

void setNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  LEDIS_CHECK(flags >= 0);
  LEDIS_CHECK(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
}

bool pumpUntilPong(ledis::IoWorker& worker, ledis::Session& session, int server_fd,
                   int client_fd, int max_ms) {
  char buf[256];
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(max_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    worker.pollReplies();
    session.pollReplies(server_fd);

    const ssize_t n = read(client_fd, buf, sizeof(buf));
    if (n > 0) {
      const std::string wire(buf, static_cast<size_t>(n));
      return wire.find("PONG") != std::string::npos;
    }
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

void test_async_ping() {
  int sv[2];
  LEDIS_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  setNonBlocking(sv[0]);
  setNonBlocking(sv[1]);

  ledis::LedisSettings settings;
  settings.io_threads = 1;
  ledis::LedisEngine engine(settings);
  engine.startDbWorker();

  ledis::IoWorker worker(0, &engine);
  ledis::Session session(1, 0, &engine);
  worker.registerSession(&session);

  ledis::Command ping;
  ping.name = ledis::Sds("PING");
  const std::string cmd = ledis::RespWriter::encodeCommand(ping);
  LEDIS_CHECK(write(sv[1], cmd.data(), cmd.size()) ==
              static_cast<ssize_t>(cmd.size()));

  LEDIS_CHECK(session.readMore(sv[0]) > 0);
  LEDIS_CHECK(session.parseAndEnqueue(sv[0]) == 1);
  LEDIS_CHECK(pumpUntilPong(worker, session, sv[0], sv[1], 2000));

  engine.stopDbWorker();
  close(sv[0]);
  close(sv[1]);
}

bool readClientResponses(int client_fd, std::string* wire) {
  char buf[256];
  for (;;) {
    const ssize_t n = read(client_fd, buf, sizeof(buf));
    if (n > 0) {
      wire->append(buf, static_cast<size_t>(n));
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return true;
    }
    return n == 0;
  }
}

void test_async_set_get_pipeline() {
  int sv[2];
  LEDIS_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  setNonBlocking(sv[0]);
  setNonBlocking(sv[1]);

  ledis::LedisSettings settings;
  settings.io_threads = 1;
  ledis::LedisEngine engine(settings);
  engine.startDbWorker();
  ledis::IoWorker worker(0, &engine);
  ledis::Session session(2, 0, &engine);
  worker.registerSession(&session);

  std::string batch;
  {
    ledis::Command set;
    set.name = ledis::Sds("SET");
    set.args.push_back(ledis::Sds("k"));
    set.args.push_back(ledis::Sds("v"));
    batch += ledis::RespWriter::encodeCommand(set);
  }
  {
    ledis::Command get;
    get.name = ledis::Sds("GET");
    get.args.push_back(ledis::Sds("k"));
    batch += ledis::RespWriter::encodeCommand(get);
  }
  LEDIS_CHECK(write(sv[1], batch.data(), batch.size()) ==
              static_cast<ssize_t>(batch.size()));

  LEDIS_CHECK(session.readMore(sv[0]) > 0);
  LEDIS_CHECK(session.parseAndEnqueue(sv[0]) == 2);

  std::string wire;
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(2000);
  while (std::chrono::steady_clock::now() < deadline) {
    worker.pollReplies();
    session.pollReplies(sv[0]);
    readClientResponses(sv[1], &wire);
    if (wire.find("+OK") != std::string::npos &&
        wire.find("$1\r\nv\r\n") != std::string::npos) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  LEDIS_CHECK(wire.find("+OK") != std::string::npos);
  LEDIS_CHECK(wire.find("$1\r\nv\r\n") != std::string::npos);

  engine.stopDbWorker();
  close(sv[0]);
  close(sv[1]);
}

}  // namespace

int main() {
  test_async_ping();
  test_async_set_get_pipeline();
  std::printf("test_io_db: OK\n");
  return 0;
}
