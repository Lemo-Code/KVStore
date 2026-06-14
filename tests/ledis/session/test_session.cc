/**
 * @file test_session.cc
 */
#include "../test_common.h"

#include "ledis/command/command.h"
#include "ledis/command/command_result.h"
#include "ledis/config/ledis_settings.h"
#include "ledis/protocol/resp_writer.h"
#include "ledis/session/ledis_engine.h"
#include "ledis/session/session.h"
#include "ledis/stream/ledis_stream.h"

#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

void test_session_ping() {
  int sv[2];
  LEDIS_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  ledis::Command ping;
  ping.name = ledis::Sds("PING");
  const std::string cmd = ledis::RespWriter::encodeCommand(ping);
  LEDIS_CHECK(write(sv[1], cmd.data(), cmd.size()) ==
              static_cast<ssize_t>(cmd.size()));

  ledis::Session session;
  session.setHandler([](const ledis::SessionContext&,
                        const ledis::Command& cmd) {
    if (cmd.name.str() == "PING") {
      return ledis::CommandResult::pong();
    }
    return ledis::CommandResult::error("ERR unknown command");
  });

  LEDIS_CHECK(session.readAndPump(sv[0]));

  ledis::LedisStream client;
  LEDIS_CHECK(client.readMore(sv[1]) > 0);
  ledis::CommandResult resp;
  LEDIS_CHECK(client.tryReadResponse(&resp) == ledis::ParseResult::kOk);
  LEDIS_CHECK(resp.value.bulk.str() == "PONG");

  close(sv[0]);
  close(sv[1]);
}

ledis::CommandResult readOneResponse(ledis::LedisStream& stream, int fd) {
  for (int i = 0; i < 200; ++i) {
    if (stream.readMore(fd) > 0) {
      ledis::CommandResult resp;
      if (stream.tryReadResponse(&resp) == ledis::ParseResult::kOk) {
        return resp;
      }
    }
  }
  return ledis::CommandResult::error("ERR test timeout");
}

void sendCommand(int fd, const ledis::Command& cmd) {
  const std::string wire = ledis::RespWriter::encodeCommand(cmd);
  LEDIS_CHECK(write(fd, wire.data(), wire.size()) ==
              static_cast<ssize_t>(wire.size()));
}

void test_select_db_isolation() {
  int sv[2];
  LEDIS_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  ledis::LedisSettings settings;
  ledis::LedisEngine engine(settings);
  ledis::Session session;
  session.setHandler([&engine, &session](const ledis::SessionContext&,
                                           const ledis::Command& cmd) {
    return engine.dispatchSync(session.context(), cmd);
  });

  ledis::LedisStream client;
  const int server_fd = sv[0];
  const int client_fd = sv[1];

  ledis::Command select2;
  select2.name = ledis::Sds("SELECT");
  select2.args.push_back(ledis::Sds("2"));
  sendCommand(client_fd, select2);
  LEDIS_CHECK(session.readAndPump(server_fd));
  LEDIS_CHECK(readOneResponse(client, client_fd).value.bulk.str() == "OK");

  ledis::Command set;
  set.name = ledis::Sds("SET");
  set.args.push_back(ledis::Sds("a"));
  set.args.push_back(ledis::Sds("1"));
  sendCommand(client_fd, set);
  LEDIS_CHECK(session.readAndPump(server_fd));
  LEDIS_CHECK(readOneResponse(client, client_fd).value.bulk.str() == "OK");

  ledis::Command select1;
  select1.name = ledis::Sds("SELECT");
  select1.args.push_back(ledis::Sds("1"));
  sendCommand(client_fd, select1);
  LEDIS_CHECK(session.readAndPump(server_fd));
  LEDIS_CHECK(readOneResponse(client, client_fd).value.bulk.str() == "OK");

  ledis::Command get;
  get.name = ledis::Sds("GET");
  get.args.push_back(ledis::Sds("a"));
  sendCommand(client_fd, get);
  LEDIS_CHECK(session.readAndPump(server_fd));
  LEDIS_CHECK(readOneResponse(client, client_fd).value.type == ledis::RespType::kNull);

  close(server_fd);
  close(client_fd);
}

}  // namespace

int main() {
  test_session_ping();
  test_select_db_isolation();
  std::printf("test_session: OK\n");
  return 0;
}
