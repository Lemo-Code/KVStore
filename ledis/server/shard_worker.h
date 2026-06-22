#pragma once

// ============================================================
// v5: 多核并行 — 共享 Dict + SpinLock
// ============================================================
// 设计思路:
//   1. N 个 worker 线程，每个独立 epoll 事件循环
//   2. SO_REUSEPORT 内核级连接分发
//   3. 所有线程共享一个 Dict，用 SpinLock 保护
//   4. 临界区只有 Dict 操作（~200ns），自旋等待远快于 mutex 阻塞
//
// 为什么 SpinLock 而不是 Mutex:
//   - Mutex: 阻塞线程 → 内核调度 → ~5-10μs 唤醒延迟
//   - SpinLock: 用户态自旋 → ~200ns 等到锁 → 零上下文切换
//   - 临界区越短，SpinLock 优势越大
//
// 为什么不用 fiber (v1-v4):
//   - fiber yield 是协作式，持有锁时 yield → 死锁
//   - 线程是抢占式，持有锁时被抢占 → OS 会调度其他线程 → 最终释放
//
// 与 v1-v4 的区别:
//   - v1-v4: 单线程 fiber，无锁，30万 rps
//   - v5: 多线程 epoll + SpinLock，预期 4 核 → 80-100万 rps
//

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <lstl/container/vector.h>
#include <lstl/container/unordered_map.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

#include "zero/net/address.h"
#include "zero/thread/mutex.h"   // SpinLock
#include "zero/log/log.h"

#include "ledis/core/storage_engine.h"
#include "ledis/core/command.h"
#include "ledis/protocol/resp_parser.h"
#include "ledis/protocol/resp_writer.h"
#include "ledis/replication/aof_writer.h"

namespace ledis {

static constexpr int MAX_EPOLL_EVENTS = 256;
static constexpr size_t READ_BUF_SIZE = 65536;

// ============================================================
// ClientConn
// ============================================================
struct ClientConn {
    int fd = -1;
    RespParser parser;

    char   read_buf[READ_BUF_SIZE];
    size_t read_pos = 0;
    size_t read_len = 0;

    std::string write_buf;
    size_t write_sent = 0;

    bool closed = false;
    bool authenticated = true;

    bool in_multi = false;
    lstl::vector<lstl::vector<std::string>> multi_queue;
    lstl::vector<std::string> watched_keys;

    int cmd_count = 0;
};

// ============================================================
// ShardWorker — 独立 epoll 线程, 共享 Dict
// ============================================================
class ShardWorker {
public:
    ShardWorker(int id, int port, const std::string& bind_addr,
                const std::string& requirepass,
                StorageEngine* shared_engine,
                zero::SpinLock* engine_lock,
                lstl::unordered_map<std::string, uint64_t>* key_versions,
                AofWriter* shared_aof,
                zero::Mutex* aof_lock)
        : id_(id), port_(port), bind_addr_(bind_addr),
          requirepass_(requirepass),
          engine_(shared_engine),
          engine_lock_(engine_lock),
          key_versions_(key_versions),
          aof_(shared_aof),
          aof_lock_(aof_lock)
    {
        g_logger_ = ZERO_LOG_ROOT();
    }

    ~ShardWorker() { stop(); }

    bool start() {
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ < 0) return false;

        // SO_REUSEPORT listen socket
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (listen_fd_ < 0) return false;

        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port = htons(static_cast<uint16_t>(port_));
        inet_pton(AF_INET, bind_addr_.c_str(), &sa.sin_addr);

        if (::bind(listen_fd_, (sockaddr*)&sa, sizeof(sa)) < 0)
            return false;
        if (::listen(listen_fd_, 511) < 0) return false;

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = listen_fd_;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev);

        running_ = true;
        thread_ = std::thread(&ShardWorker::run, this);
        return true;
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
        if (listen_fd_ >= 0) ::close(listen_fd_);
        if (epoll_fd_ >= 0) ::close(epoll_fd_);
        for (auto& kv : clients_) delete kv.second;
        clients_.clear();
    }

    size_t clientCount() const { return clients_.size(); }

private:
    void run() {
        epoll_event events[MAX_EPOLL_EVENTS];
        while (running_) {
            int n = epoll_wait(epoll_fd_, events, MAX_EPOLL_EVENTS, 0);
            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                if (fd == listen_fd_) {
                    acceptClient();
                } else {
                    auto it = clients_.find(fd);
                    if (it == clients_.end()) continue;
                    ClientConn* c = it->second;
                    uint32_t ev = events[i].events;

                    if (ev & (EPOLLERR | EPOLLHUP)) {
                        closeClient(c);
                        continue;
                    }
                    if (ev & EPOLLIN)  handleRead(c);
                    if (ev & EPOLLOUT) handleWrite(c);
                }
            }
        }

        for (auto& kv : clients_) { ::close(kv.first); delete kv.second; }
        clients_.clear();
    }

    void acceptClient() {
        while (true) {
            sockaddr_in ca{}; socklen_t calen = sizeof(ca);
            int fd = ::accept4(listen_fd_, (sockaddr*)&ca, &calen, SOCK_NONBLOCK);
            if (fd < 0) break;

            int opt = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

            auto* c = new ClientConn();
            c->fd = fd;
            c->authenticated = requirepass_.empty();

            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLOUT;  // 同时监听读写, 水平触发
            ev.data.fd = fd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);

            clients_[fd] = c;
        }
    }

    void handleRead(ClientConn* c) {
        // 边缘触发: 读到 EAGAIN
        while (true) {
            ssize_t n = ::recv(c->fd, c->read_buf + c->read_len,
                               READ_BUF_SIZE - c->read_len, MSG_DONTWAIT);
            if (n > 0) c->read_len += static_cast<size_t>(n);
            else if (n == 0) { closeClient(c); return; }
            else if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            else { closeClient(c); return; }
        }

        const char* data = c->read_buf + c->read_pos;
        size_t remaining = c->read_len - c->read_pos;
        std::string& out = c->write_buf;

        while (remaining > 0) {
            c->parser.reset();
            size_t consumed = 0;
            auto r = c->parser.feed(data, remaining, consumed);
            if (r == RespParser::Result::NEED_MORE) break;
            if (r == RespParser::Result::ERROR) {
                out += "-ERR Protocol error\r\n"; c->closed = true; break;
            }
            if (r == RespParser::Result::OK && !c->parser.args().empty()) {
                executeCommand(c, c->parser.args(), out);
                c->cmd_count++;
            }
            data += consumed; remaining -= consumed; c->read_pos += consumed;
        }

        // 压缩读缓冲区
        if (c->read_pos > 0 && remaining == 0) {
            c->read_pos = 0; c->read_len = 0;
        } else if (c->read_pos > READ_BUF_SIZE / 2) {
            size_t leftover = c->read_len - c->read_pos;
            memmove(c->read_buf, c->read_buf + c->read_pos, leftover);
            c->read_pos = 0; c->read_len = leftover;
        }

        // 有响应数据, 下一次 epoll_wait 会触发 EPOLLOUT (水平触发)
        // 无需手动 MOD — 已在 EPOLL_CTL_ADD 时注册了 EPOLLOUT

        // 定期过期
        if (c->cmd_count >= 100) {
            c->cmd_count = 0;
            engine_lock_->lock();
            engine_->activeExpireCycle();
            engine_lock_->unlock();
        }
    }

    void handleWrite(ClientConn* c) {
        std::string& out = c->write_buf;
        while (c->write_sent < out.size()) {
            ssize_t n = ::send(c->fd, out.data() + c->write_sent,
                               out.size() - c->write_sent,
                               MSG_DONTWAIT | MSG_NOSIGNAL);
            if (n > 0) c->write_sent += static_cast<size_t>(n);
            else if (n == 0) { closeClient(c); return; }
            else if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            else { closeClient(c); return; }
        }

        if (c->write_sent >= out.size()) {
            out.clear(); c->write_sent = 0;
            // 水平触发: EPOLLOUT 会自动停掉 (buffer 空, 不可写)
        }
    }

    void closeClient(ClientConn* c) {
        if (!c || c->fd < 0) return;
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, c->fd, nullptr);
        ::close(c->fd);
        clients_.erase(c->fd);
        delete c;
    }

    // ---- 命令执行 ----

    static std::string toLower(std::string_view s) {
        std::string r; r.reserve(s.size());
        for (char c : s) r += static_cast<char>(c | 0x20);
        return r;
    }

    void executeCommand(ClientConn* c,
                        const lstl::vector<std::string_view>& args,
                        std::string& out) {
        std::string lower = toLower(args[0]);
        std::string_view cn = lower;

        // 无需锁的命令
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
            engine_lock_->lock();
            c->watched_keys.clear();
            for (size_t i = 1; i < args.size(); ++i)
                c->watched_keys.push_back(std::string(args[i]) + ":" +
                    std::to_string((*key_versions_)[std::string(args[i])]));
            engine_lock_->unlock();
            out += "+OK\r\n"; return;
        }
        if (cn == "unwatch") { c->watched_keys.clear(); out += "+OK\r\n"; return; }

        if (c->in_multi) {
            lstl::vector<std::string> qa;
            for (auto& a : args) qa.push_back(std::string(a));
            c->multi_queue.push_back(std::move(qa));
            out += "+QUEUED\r\n"; return;
        }

        // 加锁 → 执行 → 解锁 (临界区 ~200ns)
        engine_lock_->lock();
        {
            CmdContext ctx;
            ctx.engine = engine_;
            ctx.args = args;
            ctx.response = &out;
            dispatchCommand(ctx);

            if (ctx.is_write && args.size() >= 2)
                (*key_versions_)[std::string(args[1])]++;

            if (ctx.is_write && aof_ && aof_->fd() >= 0) {
                aof_lock_->lock();
                aof_->appendArgs(args);
                aof_lock_->unlock();
            }
        }
        engine_lock_->unlock();
    }

    void execTransaction(ClientConn* c, std::string& out) {
        if (!c->in_multi) { out += "-ERR EXEC without MULTI\r\n"; return; }

        engine_lock_->lock();

        bool aborted = false;
        for (auto& wk : c->watched_keys) {
            auto colon = wk.find(':');
            if (colon != std::string::npos) {
                std::string key = wk.substr(0, colon);
                if ((*key_versions_)[key] != std::stoull(wk.substr(colon + 1)))
                    { aborted = true; break; }
            }
        }
        c->watched_keys.clear();

        if (aborted) {
            out += "$-1\r\n";
        } else {
            RespWriter::writeArrayHeader(out, static_cast<int64_t>(c->multi_queue.size()));
            for (auto& qa : c->multi_queue) {
                lstl::vector<std::string_view> sv;
                for (auto& a : qa) sv.push_back(a);
                CmdContext ctx;
                ctx.engine = engine_; ctx.args = sv; ctx.response = &out;
                dispatchCommand(ctx);
                if (ctx.is_write && sv.size() >= 2)
                    (*key_versions_)[std::string(sv[1])]++;
                if (ctx.is_write && aof_ && aof_->fd() >= 0) {
                    aof_lock_->lock();
                    aof_->appendArgs(sv);
                    aof_lock_->unlock();
                }
            }
        }

        engine_lock_->unlock();
        c->in_multi = false; c->multi_queue.clear();
    }

    int id_, port_;
    std::string bind_addr_, requirepass_;

    int epoll_fd_ = -1, listen_fd_ = -1;
    std::thread thread_;
    std::atomic<bool> running_{false};

    lstl::unordered_map<int, ClientConn*> clients_;

    // 共享状态 (所有 worker 共用)
    StorageEngine* engine_;
    zero::SpinLock* engine_lock_;
    lstl::unordered_map<std::string, uint64_t>* key_versions_;
    AofWriter* aof_;
    zero::Mutex* aof_lock_;

    static zero::Logger::ptr g_logger_;
};

zero::Logger::ptr ShardWorker::g_logger_;

} // namespace ledis
