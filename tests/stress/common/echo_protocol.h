/**
 * echo_protocol.h — TCP Echo 压测统一协议
 *
 * zero (echo_minimal) 与 libevent (bufferevent echo) 均须实现相同语义:
 *   1. TCP 长连接 keep-alive
 *   2. 每轮: 客户端 send 固定 kMsgSize 字节 → 服务端原样 echo 相同字节数
 *   3. 客户端 recv 满 kMsgSize 字节后才算完成一轮
 *   4. 客户端关闭连接时服务端结束会话
 */
#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <string>

namespace stress {

struct EchoProtocol {
    static constexpr int kMsgSize           = 64;
    static constexpr int kRoundsPerClient   = 1000;
    static constexpr int kConnectTimeoutSec = 5;
    static constexpr char kPayloadFill      = 'X';
};

/** 构造标准压测报文 (内容可校验) */
inline std::string makeEchoPayload(int size = EchoProtocol::kMsgSize) {
    std::string msg(size, EchoProtocol::kPayloadFill);
    for (int i = 0; i < size; ++i)
        msg[i] = static_cast<char>('A' + (i % 26));
    return msg;
}

inline bool connectEcho(int port, int& out_fd) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    int flag = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    struct timeval tv{EchoProtocol::kConnectTimeoutSec, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return false;
    }
    out_fd = fd;
    return true;
}

/** 发送/接收固定长度, 失败返回 false */
inline bool sendAll(int fd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

inline bool recvAll(int fd, char* data, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t n = ::recv(fd, data + got, len - got, 0);
        if (n <= 0) return false;
        got += static_cast<size_t>(n);
    }
    return true;
}

/** 单轮 echo 往返 + 内容校验 */
inline bool echoRoundTrip(int fd, const std::string& msg) {
    if (!sendAll(fd, msg.data(), msg.size())) return false;
    std::string buf(msg.size(), '\0');
    if (!recvAll(fd, &buf[0], buf.size())) return false;
    return std::memcmp(buf.data(), msg.data(), msg.size()) == 0;
}

/** 压测客户端: 同一连接上连续 kRoundsPerClient 轮, 返回成功轮数 */
inline uint64_t echoClientBench(int port, const std::string& msg, int rounds) {
    int fd = -1;
    if (!connectEcho(port, fd)) return 0;

    uint64_t ok = 0;
    for (int r = 0; r < rounds; ++r) {
        if (!echoRoundTrip(fd, msg)) break;
        ++ok;
    }
    ::close(fd);
    return ok;
}

inline void echoClientBenchAtomic(int port, const std::string& msg, int rounds,
                                  std::atomic<uint64_t>& ops) {
    ops.fetch_add(echoClientBench(port, msg, rounds), std::memory_order_relaxed);
}

/** 启动前探活: 1 轮 echo 且校验 payload */
inline bool probeEchoServer(int port, int timeout_ms = 3000) {
    for (int i = 0; i < timeout_ms; i += 50) {
        int fd = -1;
        if (!connectEcho(port, fd)) {
            usleep(50000);
            continue;
        }
        auto msg = makeEchoPayload();
        bool ok = echoRoundTrip(fd, msg);
        ::close(fd);
        if (ok) return true;
        usleep(50000);
    }
    return false;
}

} // namespace stress
