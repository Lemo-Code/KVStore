#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <lstl/container/unordered_set.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "zero/net/socket.h"
#include "zero/net/socket_stream.h"
#include "ledis/protocol/resp_parser.h"

namespace ledis {

class LedisServer;

// ============================================================
// ClientContext — 每个客户端连接的上下文
// ============================================================
struct ClientContext {
    // 网络层
    zero::Socket::ptr  sock;
    zero::SocketStream stream;

    ClientContext(zero::Socket::ptr s)
        : sock(std::move(s))
        , stream(this->sock)
    {
        response_event_fd_ = eventfd(0, EFD_NONBLOCK);
        if (auto addr = this->sock->getRemoteAddress()) {
            remote_addr = addr->toString();
        }
    }

    ~ClientContext() {
        if (response_event_fd_ >= 0) {
            ::close(response_event_fd_);
            response_event_fd_ = -1;
        }
    }

    // 禁止拷贝
    ClientContext(const ClientContext&) = delete;
    ClientContext& operator=(const ClientContext&) = delete;
    // 允许移动
    ClientContext(ClientContext&& other) noexcept
        : sock(std::move(other.sock))
        , stream(std::move(other.stream))
        , remote_addr(std::move(other.remote_addr))
        , write_buf(std::move(other.write_buf))
        , parser(std::move(other.parser))
        , response_event_fd_(other.response_event_fd_)
        , io_thread_id(other.io_thread_id)
        , db_index(other.db_index)
        , closed(other.closed)
        , total_commands(other.total_commands)
    {
        other.response_event_fd_ = -1;
    }

    std::string remote_addr;

    // 响应缓冲 (存储线程写入 → IO fiber 刷新)
    std::string write_buf;

    // 协议解析
    RespParser parser;

    // 响应通知 eventfd
    //   IO fiber: read() 等待 → hook 注册到 reactor → fiber yield
    //   存储线程: write() 通知 → reactor 唤醒 fiber
    int response_event_fd_ = -1;

    // 响应就绪标志 (存储线程写入 write_buf 后设置)
    std::atomic<bool> response_ready{false};

    // 所属 IO 线程
    int io_thread_id = 0;

    // 当前选中数据库
    int db_index = 0;

    // 事务状态
    bool in_multi = false;
    lstl::vector<lstl::vector<std::string>> multi_queue;  // QUEUED 命令 (所有字符串)

    // Pub/Sub 订阅
    lstl::unordered_set<std::string> channels;
    lstl::unordered_set<std::string> patterns;
    std::atomic<bool> pubsub_msg{false};   // 有 Pub/Sub 消息待发送

    // 标志
    bool closed = false;
    bool authenticated = false;  // requirepass 认证

    // WATCH 乐观锁
    lstl::vector<std::string> watched_keys;

    // 统计
    uint64_t total_commands = 0;
};

} // namespace ledis
