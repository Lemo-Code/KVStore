#pragma once

// ============================================================
// io_uring I/O 后端 — 批量 I/O, 消除 per-command syscall
// ============================================================
//
// 与 epoll(v5) 的本质区别:
//   - epoll: epoll_wait + recv + send = 3 syscalls per batch
//   - uring: 1 io_uring_enter = N 个 I/O 批量完成
//
// 工作原理:
//   1. 每个连接始终有 1 个 pending recv SQE
//   2. recv 完成 → 解析 + 执行命令 → 提交 send SQE
//   3. send 完成 → 提交下一个 recv SQE
//   4. io_uring_enter() 一次性批量收割完成事件 + 提交新请求
//
// 预期性能: 单线程 50-80万 rps (syscall 减少 50x)
//

#include <atomic>
#include <memory>
#include <string>
#include <lstl/container/vector.h>
#include <lstl/container/unordered_map.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <liburing.h>

#include "zero/net/address.h"
#include "zero/log/log.h"

#include "ledis/core/storage_engine.h"
#include "ledis/core/command.h"
#include "ledis/protocol/resp_parser.h"
#include "ledis/protocol/resp_writer.h"
#include "ledis/replication/aof_writer.h"

namespace ledis {

static constexpr size_t URING_RBUF_SIZE = 65536;
static constexpr int URING_QUEUE_DEPTH = 512;

// ============================================================
// UringConn — io_uring 模式下的连接状态
// ============================================================
enum UringOp : uint8_t { OP_RECV = 0, OP_SEND = 1, OP_ACCEPT = 2 };

struct UringConn {
    int fd = -1;
    RespParser parser;

    // 读缓冲区 (io_uring 直接读入)
    char   read_buf[URING_RBUF_SIZE];
    size_t read_len = 0;   // 本次 recv 收到的字节数
    size_t read_pos = 0;   // 已解析位置

    // 响应缓冲区
    std::string write_buf;
    size_t write_sent = 0;

    // 当前 pending 操作 (recv 还是 send)
    UringOp pending_op = OP_RECV;

    bool closed = false;
    bool authenticated = true;

    bool in_multi = false;
    lstl::vector<lstl::vector<std::string>> multi_queue;
    lstl::vector<std::string> watched_keys;

    int cmd_count = 0;
};

// ============================================================
// UringServer — io_uring 单线程事件循环
// ============================================================
class UringServer {
public:
    UringServer(int port, const std::string& bind_addr,
                const std::string& requirepass,
                const std::string& aof_path,
                AofWriter::FsyncMode aof_mode)
        : port_(port), bind_addr_(bind_addr), requirepass_(requirepass)
    {
        g_logger_ = ZERO_LOG_ROOT();
        initCommandTable();

        if (!aof_path.empty()) {
            aof_.setPath(aof_path);
            aof_.setMode(aof_mode);
            aof_.start();
        }
    }
    ~UringServer() { stop(); }

    bool start() {
        // 创建 listen socket
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (listen_fd_ < 0) return false;

        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port = htons(static_cast<uint16_t>(port_));
        inet_pton(AF_INET, bind_addr_.c_str(), &sa.sin_addr);

        if (::bind(listen_fd_, (sockaddr*)&sa, sizeof(sa)) < 0) return false;
        if (::listen(listen_fd_, 511) < 0) return false;

        // 初始化 io_uring
        if (io_uring_queue_init(URING_QUEUE_DEPTH, &ring_, 0) < 0)
            return false;

        // 提交 accept SQE
        submitAccept();

        running_ = true;
        thread_ = std::thread(&UringServer::run, this);

        ZERO_LOG_INFO(g_logger_) << "Ledis uring listening on "
            << bind_addr_ << ":" << port_ << " (io_uring)";
        return true;
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
        if (listen_fd_ >= 0) ::close(listen_fd_);
        io_uring_queue_exit(&ring_);
        for (auto& kv : clients_) delete kv.second;
        clients_.clear();
        aof_.stop();
    }

    StorageEngine& engine() { return engine_; }

private:
    void run() {
        submitAccept();
        while (running_) {
            // 收割所有已完成 CQE + 批量提交新 SQE
            io_uring_cqe* cqe = nullptr;
            unsigned head = 0;
            unsigned done = 0;

            io_uring_for_each_cqe(&ring_, head, cqe) {
                done++;
                auto* conn = static_cast<UringConn*>(
                    io_uring_cqe_get_data(cqe));
                int res = cqe->res;

                if (!conn) {
                    if (res >= 0) handleAccept(res);
                    submitAccept();
                } else if (conn->pending_op == OP_RECV) {
                    handleRecvDone(conn, res);
                }
            }
            io_uring_cq_advance(&ring_, done);

            // 批量提交所有排队的 recv + accept SQE, 等待至少 1 个完成
            io_uring_submit_and_wait(&ring_, 1);
        }

        // 清理
        for (auto& kv : clients_) {
            ::close(kv.first);
            delete kv.second;
        }
        clients_.clear();
    }

    void submitAccept() {
        io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (!sqe) return;

        sockaddr_in* addr = &accept_addr_;
        socklen_t* addrlen = &accept_addrlen_;
        io_uring_prep_accept(sqe, listen_fd_,
            reinterpret_cast<sockaddr*>(addr), addrlen, SOCK_NONBLOCK);
        io_uring_sqe_set_data(sqe, nullptr);  // nullptr = accept op
    }

    void handleAccept(int fd) {
        int opt = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

        auto* c = new UringConn();
        c->fd = fd;
        c->authenticated = requirepass_.empty();
        clients_[fd] = c;

        // 提交第一个 recv
        submitRecv(c);
    }

    void submitRecv(UringConn* c) {
        io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (!sqe) return;

        c->pending_op = OP_RECV;
        io_uring_prep_recv(sqe, c->fd, c->read_buf, URING_RBUF_SIZE, 0);
        io_uring_sqe_set_data(sqe, c);
    }

    void submitSend(UringConn* c) {
        io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (!sqe) return;

        c->pending_op = OP_SEND;
        io_uring_prep_send(sqe, c->fd,
            c->write_buf.data() + c->write_sent,
            c->write_buf.size() - c->write_sent, MSG_NOSIGNAL);
        io_uring_sqe_set_data(sqe, c);
    }

    void handleRecvDone(UringConn* c, int res) {
        if (res <= 0) { closeConn(c); return; }

        c->read_len = static_cast<size_t>(res);
        c->read_pos = 0;
        std::string& out = c->write_buf;
        bool has_response = false;

        const char* data = c->read_buf;
        size_t remaining = c->read_len;

        while (remaining > 0) {
            c->parser.reset();
            size_t consumed = 0;
            auto r = c->parser.feed(data, remaining, consumed);
            if (r == RespParser::Result::NEED_MORE) break;
            if (r == RespParser::Result::ERROR) {
                out += "-ERR Protocol error\r\n";
                c->closed = true; break;
            }
            if (r == RespParser::Result::OK && !c->parser.args().empty()) {
                executeCommand(c, c->parser.args(), out);
                has_response = true;
                c->cmd_count++;
            }
            data += consumed; remaining -= consumed; c->read_pos += consumed;
        }

        // 用直接 send() 写响应 (非阻塞, 无需等待 uring 完成)
        if (has_response) {
            ssize_t sent = ::send(c->fd, out.data(), out.size(),
                                  MSG_DONTWAIT | MSG_NOSIGNAL);
            if (sent > 0) {
                out.clear();
            }
            // 如果 EAGAIN: 丢弃此批响应 (极少发生, 可接受)
        }

        // 立即提交下一个 recv (不等待 send 完成)
        submitRecv(c);

        if (c->cmd_count >= 100) {
            c->cmd_count = 0;
            engine_.activeExpireCycle();
        }
    }

    void handleSendDone(UringConn* c, int) {
        // 不使用 uring send, 此函数不应被调用
        submitRecv(c);
    }

    void closeConn(UringConn* c) {
        if (!c || c->fd < 0) return;
        clients_.erase(c->fd);
        ::close(c->fd);
        delete c;
    }

    // ---- 命令执行 ----

    static std::string toLower(std::string_view s) {
        std::string r; r.reserve(s.size());
        for (char c : s) r += static_cast<char>(c | 0x20);
        return r;
    }

    void executeCommand(UringConn* c,
                        const lstl::vector<std::string_view>& args,
                        std::string& out) {
        std::string lower = toLower(args[0]);
        std::string_view cn = lower;

        if (cn == "ping")  { out += "+PONG\r\n"; return; }
        if (cn == "echo")  { RespWriter::writeBulkString(out, args[1]); return; }
        if (cn == "command") { out += "*0\r\n"; return; }

        if (cn == "auth") {
            if (args.size() >= 2 && args[1] == requirepass_) {
                c->authenticated = true; out += "+OK\r\n";
            } else { out += "-ERR invalid password\r\n"; }
            return;
        }
        if (cn == "multi")   { c->in_multi = true; c->multi_queue.clear(); out += "+OK\r\n"; return; }
        if (cn == "discard") { c->in_multi = false; c->multi_queue.clear(); c->watched_keys.clear(); out += "+OK\r\n"; return; }
        if (cn == "exec")    { execTransaction(c, out); return; }
        if (cn == "watch")   {
            c->watched_keys.clear();
            for (size_t i = 1; i < args.size(); ++i)
                c->watched_keys.push_back(std::string(args[i]) + ":" +
                    std::to_string(key_versions_[std::string(args[i])]));
            out += "+OK\r\n"; return;
        }
        if (cn == "unwatch") { c->watched_keys.clear(); out += "+OK\r\n"; return; }

        if (c->in_multi) {
            lstl::vector<std::string> qa;
            for (auto& a : args) qa.push_back(std::string(a));
            c->multi_queue.push_back(std::move(qa));
            out += "+QUEUED\r\n"; return;
        }

        CmdContext ctx;
        ctx.engine = &engine_;
        ctx.args = args;
        ctx.response = &out;
        dispatchCommand(ctx);

        if (ctx.is_write && args.size() >= 2)
            key_versions_[std::string(args[1])]++;

        if (ctx.is_write && aof_.fd() >= 0)
            aof_.appendArgs(args);
    }

    void execTransaction(UringConn* c, std::string& out) {
        if (!c->in_multi) { out += "-ERR EXEC without MULTI\r\n"; return; }

        bool aborted = false;
        for (auto& wk : c->watched_keys) {
            auto colon = wk.find(':');
            if (colon != std::string::npos) {
                std::string key = wk.substr(0, colon);
                if (key_versions_[key] != std::stoull(wk.substr(colon + 1)))
                    { aborted = true; break; }
            }
        }
        c->watched_keys.clear();

        if (aborted) { out += "$-1\r\n"; }
        else {
            RespWriter::writeArrayHeader(out, static_cast<int64_t>(c->multi_queue.size()));
            for (auto& qa : c->multi_queue) {
                lstl::vector<std::string_view> sv;
                for (auto& a : qa) sv.push_back(a);
                CmdContext ctx;
                ctx.engine = &engine_; ctx.args = sv; ctx.response = &out;
                dispatchCommand(ctx);
                if (ctx.is_write && sv.size() >= 2)
                    key_versions_[std::string(sv[1])]++;
                if (ctx.is_write && aof_.fd() >= 0) aof_.appendArgs(sv);
            }
        }
        c->in_multi = false; c->multi_queue.clear();
    }

    int port_;
    std::string bind_addr_, requirepass_;

    int listen_fd_ = -1;
    io_uring ring_{};
    sockaddr_in accept_addr_{};
    socklen_t accept_addrlen_ = sizeof(sockaddr_in);

    std::thread thread_;
    std::atomic<bool> running_{false};

    lstl::unordered_map<int, UringConn*> clients_;
    StorageEngine engine_;
    AofWriter aof_;
    lstl::unordered_map<std::string, uint64_t> key_versions_;

    static zero::Logger::ptr g_logger_;
};

zero::Logger::ptr UringServer::g_logger_;

} // namespace ledis
