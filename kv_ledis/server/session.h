#pragma once

#include <memory>
#include <string>
#include <lstl/container/vector.h>
#include <lstl/container/unordered_set.h>

#include "zero/net/socket.h"
#include "zero/net/socket_stream.h"
#include "kv_ledis/protocol/resp_parser.h"

namespace ledis {

struct Session {
    zero::Socket::ptr  sock;
    zero::SocketStream stream;
    std::string        remote_addr;
    RespParser         parser;

    bool authenticated = true;
    bool closed = false;

    // 事务
    bool in_multi = false;
    lstl::vector<lstl::vector<std::string>> multi_queue;
    lstl::vector<std::string> watched_keys;

    // Pub/Sub
    lstl::unordered_set<std::string> channels;
    lstl::unordered_set<std::string> patterns;
    std::string pubsub_buf;  // Pub/Sub 消息缓冲

    // Client 管理
    std::string name;  // CLIENT SETNAME/GETNAME

    // Monitor
    bool is_monitor = false;

    explicit Session(zero::Socket::ptr s)
        : sock(std::move(s)), stream(this->sock) {
        if (auto addr = this->sock->getRemoteAddress())
            remote_addr = addr->toString();
    }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
};

} // namespace ledis
