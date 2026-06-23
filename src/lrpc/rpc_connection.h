#pragma once
// ============================================================
// lrpc/rpc_connection.h — RPC 连接管理
// ============================================================
//
// RpcNode 在独立线程中运行 epoll 事件循环，管理所有集群连接。
//
// 线程模型:
//   - I/O 线程: 运行 ioLoop(), 处理所有 socket I/O
//   - Server 线程: 调用 sendMessage/sendRequest 添加消息到写缓冲区
//   - 通过 send_mutex_ 保护写缓冲区，避免竞态
//   - 响应通过 eventfd 通知 server 线程 (不阻塞 I/O 线程)
//

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <chrono>

#include <lstl/container/vector.h>
#include <lstl/container/unordered_map.h>

#include "lrpc/protocol.h"

namespace lrpc {

static constexpr int    MAX_EPOLL_EVENTS = 64;
static constexpr size_t RPC_IO_BUF_SIZE  = 65536;

// ============================================================
// Peer — 单条对等连接
// ============================================================
struct Peer {
    int         fd = -1;
    std::string peer_id;
    bool        inbound = false;   // true = 对方发起的连接
    bool        closed  = false;

    bool        connecting = false; // 正在建立连接 (EINPROGRESS)

    // 读缓冲
    uint8_t read_buf[RPC_IO_BUF_SIZE];
    size_t  read_pos = 0;  // 已处理位置
    size_t  read_len = 0;  // 已接收总长度

    // 写缓冲
    std::string write_buf;
    size_t      write_sent = 0;

    uint64_t accept_time_ms = 0;
};

// ============================================================
// PendingCall — 等待响应的调用
// ============================================================
struct PendingCall {
    uint32_t    call_id = 0;
    int         event_fd = -1;       // 用于唤醒等待线程
    bool        done = false;
    std::string response_body;
    bool        is_error = false;
};

// ============================================================
// RpcNode — RPC 节点 (管理所有集群连接)
// ============================================================
class RpcNode {
public:
    // 消息处理器: (msg_type, sender_id, headers, body, call_id, flags)
    // call_id 非零时，处理器应答时需用 sendResponse(call_id, body) 回复
    using MessageHandler = std::function<void(
        uint16_t msg_type,
        const std::string& sender_id,
        const lstl::vector<std::pair<std::string, std::string>>& headers,
        const std::string& body,
        uint32_t call_id,
        uint8_t flags)>;

    RpcNode() {
        notify_fd_ = eventfd(0, EFD_NONBLOCK);
    }

    ~RpcNode() { stop(); }

    // ---- 生命周期 ----

    bool start(int listen_port, const std::string& bind_addr) {
        listen_port_ = listen_port;
        bind_addr_   = bind_addr;

        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ < 0) return false;

        // 创建监听 socket
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (listen_fd_ < 0) { ::close(epoll_fd_); epoll_fd_ = -1; return false; }

        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port   = htons(static_cast<uint16_t>(listen_port_));
        inet_pton(AF_INET, bind_addr_.c_str(), &sa.sin_addr);

        if (::bind(listen_fd_, (sockaddr*)&sa, sizeof(sa)) < 0) {
            ::close(listen_fd_); ::close(epoll_fd_);
            listen_fd_ = -1; epoll_fd_ = -1;
            return false;
        }
        if (::listen(listen_fd_, SOMAXCONN) < 0) {
            ::close(listen_fd_); ::close(epoll_fd_);
            listen_fd_ = -1; epoll_fd_ = -1;
            return false;
        }

        addEpoll(listen_fd_);

        running_ = true;
        io_thread_ = std::thread(&RpcNode::ioLoop, this);
        return true;
    }

    void stop() {
        if (!running_.exchange(false)) return;
        if (io_thread_.joinable()) io_thread_.join();

        for (auto& kv : peers_) {
            if (kv.second) {
                ::close(kv.second->fd);
                delete kv.second;
            }
        }
        peers_.clear();
        fd_to_id_.clear();

        if (listen_fd_ >= 0) { ::close(listen_fd_); listen_fd_ = -1; }
        if (epoll_fd_  >= 0) { ::close(epoll_fd_);  epoll_fd_  = -1; }
        if (notify_fd_ >= 0) { ::close(notify_fd_); notify_fd_ = -1; }
    }

    // 获取 eventfd (server 线程用 select/poll 监听此 fd)
    int getNotifyFd() const { return notify_fd_; }

    // ---- 消息处理器 ----

    void setMessageHandler(MessageHandler h) { msg_handler_ = std::move(h); }

    // ---- 连接管理 ----

    // 连接到对等节点
    bool connectTo(const std::string& node_id, const std::string& ip, int port) {
        if (getPeer(node_id)) return true;  // 已连接

        int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd < 0) return false;

        int opt = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port   = htons(static_cast<uint16_t>(port));
        inet_pton(AF_INET, ip.c_str(), &sa.sin_addr);

        int ret = ::connect(fd, (sockaddr*)&sa, sizeof(sa));
        if (ret < 0 && errno != EINPROGRESS) {
            ::close(fd); return false;
        }

        auto* peer = new Peer();
        peer->fd        = fd;
        peer->peer_id   = node_id;
        peer->inbound   = false;
        peer->connecting = (ret < 0 && errno == EINPROGRESS);

        // EINPROGRESS: 注册 EPOLLOUT 等待连接完成
        addEpoll(fd, peer->connecting);
        peers_[node_id]   = peer;
        fd_to_id_[fd]     = node_id;

        return true;
    }

    bool isConnected(const std::string& node_id) {
        return getPeer(node_id) != nullptr;
    }

    // ---- 发送消息 ----

    // 发送单向消息 (fire-and-forget, 不等待响应)
    void sendOneWay(const std::string& target_id, uint16_t msg_type,
                    const std::string& body = "",
                    const lstl::vector<std::pair<std::string, std::string>>& headers = {}) {
        std::string frame = buildFrame(0, msg_type, FLAG_ONEWAY, body, headers);
        sendRaw(target_id, std::move(frame));
    }

    // 发送 RPC 响应 (由消息处理器调用，匹配请求的 call_id)
    void sendResponse(const std::string& target_id, uint32_t call_id,
                      uint16_t msg_type, const std::string& body = "",
                      bool is_error = false) {
        uint8_t flags = FLAG_RESPONSE;
        if (is_error) flags |= FLAG_ERROR;
        std::string frame = buildFrame(call_id, msg_type, flags, body, {});
        sendRaw(target_id, std::move(frame));
    }

    // 发送请求并等待响应 (阻塞 server 线程直到收到回复或超时)
    // 返回 {body, is_error}
    std::pair<std::string, bool> sendRequest(const std::string& target_id,
                                              uint16_t msg_type,
                                              const std::string& body = "",
                                              const lstl::vector<std::pair<std::string, std::string>>& headers = {},
                                              int timeout_ms = 5000) {
        uint32_t call_id = nextCallId();

        // 创建 PendingCall
        auto* pc = new PendingCall();
        pc->call_id  = call_id;
        pc->event_fd = eventfd(0, EFD_NONBLOCK);

        {
            std::lock_guard<std::mutex> lk(pending_mutex_);
            pending_[call_id] = pc;
        }

        // 发送请求帧 (无 ONEWAY 标志)
        std::string frame = buildFrame(call_id, msg_type, 0, body, headers);
        sendRaw(target_id, std::move(frame));

        // 等待响应 (使用 eventfd + poll)
        std::string result;
        bool is_error = true;

        struct pollfd pfd;
        pfd.fd      = pc->event_fd;
        pfd.events  = POLLIN;
        pfd.revents = 0;

        int ret = ::poll(&pfd, 1, timeout_ms);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            // 读取 eventfd
            uint64_t val;
            ::read(pc->event_fd, &val, sizeof(val));

            if (pc->done) {
                result   = std::move(pc->response_body);
                is_error = pc->is_error;
            }
        }
        // else: timeout

        // 清理
        {
            std::lock_guard<std::mutex> lk(pending_mutex_);
            pending_.erase(call_id);
        }
        ::close(pc->event_fd);
        delete pc;

        if (ret <= 0) {
            return {"timeout", true};
        }

        return {result, is_error};
    }

    // 向所有已知节点广播单向消息
    void broadcast(uint16_t msg_type,
                   const std::string& body = "",
                   const lstl::vector<std::pair<std::string, std::string>>& headers = {}) {
        std::string frame = buildFrame(0, msg_type, FLAG_ONEWAY, body, headers);
        for (auto& kv : peers_) {
            if (kv.second && !kv.second->closed)
                sendRaw(kv.first, frame);  // copy frame per send
        }
    }

    // 获取所有已连接节点 ID
    void getConnectedPeers(lstl::vector<std::string>& out) {
        for (auto& kv : peers_)
            if (kv.second && !kv.second->closed)
                out.push_back(kv.first);
    }

    // 通知 server 线程 (通过 notify_fd)
    void notifyServer() {
        uint64_t val = 1;
        ::write(notify_fd_, &val, sizeof(val));
    }

    // 清空通知 (server 线程在 poll 返回后调用)
    void drainNotify() {
        uint64_t val;
        while (::read(notify_fd_, &val, sizeof(val)) > 0) {}
    }

private:
    // ---- 构建帧 ----
    std::string buildFrame(uint32_t call_id, uint16_t msg_type, uint8_t flags,
                           const std::string& body,
                           const lstl::vector<std::pair<std::string, std::string>>& headers) {
        size_t hbs  = headerBlockSize(headers);
        size_t fsz  = frameSize(body.size(), hbs);
        std::string frame(fsz, '\0');

        FrameHeader hdr;
        hdr.magic     = RPC_MAGIC;
        hdr.frame_len = static_cast<uint32_t>(fsz);
        hdr.call_id   = call_id;
        hdr.msg_type  = msg_type;
        hdr.flags     = flags;
        hdr.reserved  = 0;
        hdr.body_len  = static_cast<uint32_t>(body.size());

        uint8_t* buf = reinterpret_cast<uint8_t*>(frame.data());
        encodeFrameHeader(buf, hdr);

        size_t hp = FRAME_HDR_SIZE;
        size_t hn = encodeHeaders(buf + hp, hbs, headers);
        hp += hn;

        if (body.size() > 0) {
            memcpy(buf + hp, body.data(), body.size());
        }

        return frame;
    }

    // ---- 发送原始帧到写缓冲区 ----
    void sendRaw(const std::string& target_id, std::string frame) {
        Peer* peer = nullptr;
        {
            std::lock_guard<std::mutex> lk(send_mutex_);
            peer = getPeer(target_id);
            if (!peer || peer->closed) return;

            bool was_empty = peer->write_buf.empty();
            peer->write_buf += frame;

            if (!was_empty) return;  // handleWrite 正在发送
        }

        // 尝试直接发送 (锁外，避免与 ioLoop 死锁)
        bool need_epollout = false;
        {
            std::lock_guard<std::mutex> lk(send_mutex_);
            // 重新获取 peer (可能已被关闭)
            peer = getPeer(target_id);
            if (!peer || peer->closed) return;

            while (peer->write_sent < peer->write_buf.size()) {
                ssize_t n = ::send(peer->fd,
                                   peer->write_buf.data() + peer->write_sent,
                                   peer->write_buf.size() - peer->write_sent,
                                   MSG_DONTWAIT | MSG_NOSIGNAL);
                if (n > 0) {
                    peer->write_sent += static_cast<size_t>(n);
                } else if (n == 0) {
                    peer->closed = true; return;
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOTCONN) {
                        // ENOTCONN: 非阻塞 connect 尚未完成
                        need_epollout = true;
                        break;
                    }
                    peer->closed = true; return;
                }
            }

            if (peer->write_sent >= peer->write_buf.size()) {
                peer->write_buf.clear();
                peer->write_sent = 0;
            }
        }

        // 注册 EPOLLOUT (锁外)
        if (need_epollout && !peer->closed) {
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLOUT;
            ev.data.fd = peer->fd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, peer->fd, &ev);
        }
    }

    // ---- I/O 事件循环 ----
    void ioLoop() {
        struct epoll_event events[MAX_EPOLL_EVENTS];

        while (running_) {
            int nfds = epoll_wait(epoll_fd_, events, MAX_EPOLL_EVENTS, 100);
            if (nfds < 0) {
                if (errno == EINTR) continue;
                break;
            }

            for (int i = 0; i < nfds; ++i) {
                int fd = events[i].data.fd;

                if (fd == listen_fd_) {
                    handleAccept();
                    continue;
                }

                auto* peer = getPeerByFd(fd);
                if (!peer) continue;

                uint32_t ev = events[i].events;
                if (ev & (EPOLLERR | EPOLLHUP)) {
                    handleClose(peer);
                    continue;
                }

                if (ev & EPOLLIN)  handleRead(peer);
                if (ev & EPOLLOUT) handleWrite(peer);
            }
        }
    }

    void handleAccept() {
        while (true) {
            sockaddr_in sa{};
            socklen_t slen = sizeof(sa);
            int fd = ::accept4(listen_fd_, (sockaddr*)&sa, &slen, SOCK_NONBLOCK);
            if (fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                break;
            }

            int opt = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

            auto* peer   = new Peer();
            peer->fd      = fd;
            peer->inbound = true;
            peer->accept_time_ms = nowMs();

            addEpoll(fd);

            // 临时索引: 用 fd 作为 key，等收到第一条消息后设置 peer_id
            std::string tmp_key = "__pending_" + std::to_string(fd);
            fd_to_id_[fd]   = tmp_key;
            peers_[tmp_key] = peer;
        }
    }

    void handleRead(Peer* peer) {
        // 单次非阻塞读取
        ssize_t n = ::recv(peer->fd,
                           peer->read_buf + peer->read_len,
                           RPC_IO_BUF_SIZE - peer->read_len,
                           MSG_DONTWAIT);
        if (n > 0) {
            peer->read_len += static_cast<size_t>(n);
        } else if (n == 0) {
            handleClose(peer); return;
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                handleClose(peer);
            return;
        }

        // 解析所有完整帧
        while (peer->read_len - peer->read_pos >= FRAME_HDR_SIZE) {
            const uint8_t* data = peer->read_buf + peer->read_pos;
            size_t remaining    = peer->read_len - peer->read_pos;

            FrameHeader hdr;
            if (!decodeFrameHeader(data, remaining, hdr)) {
                handleClose(peer); return;
            }

            if (remaining < hdr.frame_len) break;  // 帧不完整，等待更多数据

            // 解析 headers
            lstl::vector<std::pair<std::string, std::string>> headers;
            size_t body_offset = FRAME_HDR_SIZE;
            if (hdr.body_len < hdr.frame_len - FRAME_HDR_SIZE) {
                size_t hn = decodeHeaders(data + body_offset,
                                          hdr.frame_len - body_offset - hdr.body_len,
                                          headers);
                body_offset += hn;
            }

            std::string body;
            if (hdr.body_len > 0)
                body.assign(reinterpret_cast<const char*>(data + body_offset), hdr.body_len);

            // 分发消息
            dispatchMessage(peer, hdr, headers, body);

            peer->read_pos += hdr.frame_len;
        }

        // 压缩缓冲区
        if (peer->read_pos >= peer->read_len) {
            peer->read_pos = 0;
            peer->read_len = 0;
        } else if (peer->read_pos > RPC_IO_BUF_SIZE / 2) {
            size_t leftover = peer->read_len - peer->read_pos;
            memmove(peer->read_buf, peer->read_buf + peer->read_pos, leftover);
            peer->read_pos = 0;
            peer->read_len = leftover;
        }
    }

    void handleWrite(Peer* peer) {
        // 处理连接完成 (EINPROGRESS → connected)
        if (peer->connecting) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(peer->fd, SOL_SOCKET, SO_ERROR, &err, &len);
            if (err != 0) {
                handleClose(peer); return;  // 连接失败
            }
            peer->connecting = false;
        }

        std::string& out = peer->write_buf;
        while (peer->write_sent < out.size()) {
            ssize_t n = ::send(peer->fd,
                               out.data() + peer->write_sent,
                               out.size() - peer->write_sent,
                               MSG_DONTWAIT | MSG_NOSIGNAL);
            if (n > 0) {
                peer->write_sent += static_cast<size_t>(n);
            } else if (n == 0) {
                handleClose(peer); return;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                handleClose(peer); return;
            }
        }

        if (peer->write_sent >= out.size()) {
            out.clear();
            peer->write_sent = 0;
            // 写完成，只监听读事件
            struct epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.fd = peer->fd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, peer->fd, &ev);
        }
    }

    void handleClose(Peer* peer) {
        if (!peer || peer->closed) return;
        peer->closed = true;
        closeConn(peer);
    }

    void closeConn(Peer* peer) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, peer->fd, nullptr);
        ::close(peer->fd);

        std::string node_id = peer->peer_id;
        if (!node_id.empty()) {
            peers_.erase(node_id);
        } else {
            // 清除临时 key
            std::string tmp_key = "__pending_" + std::to_string(peer->fd);
            peers_.erase(tmp_key);
        }
        fd_to_id_.erase(peer->fd);
        delete peer;
    }

    // ---- 消息分发 ----
    void dispatchMessage(Peer* peer, FrameHeader& hdr,
                         lstl::vector<std::pair<std::string, std::string>>& headers,
                         std::string& body) {
        // 从 headers 中提取 sender_id
        std::string sender_id;
        for (auto& h : headers) {
            if (h.first == "sender_id") {
                sender_id = h.second;
                break;
            }
        }
        if (sender_id.empty()) {
            // 尝试从 body 中解析 (兼容简单消息)
            // 或者使用 peer 的已知 ID
            sender_id = peer->peer_id;
        }

        // 入站连接的第一条消息: 设置 peer_id
        if (peer->inbound && !sender_id.empty() && sender_id != peer->peer_id) {
            std::string old_key = peer->peer_id;
            if (!old_key.empty())
                peers_.erase(old_key);

            peer->peer_id = sender_id;
            peers_[sender_id] = peer;
            fd_to_id_[peer->fd] = sender_id;
        }

        // 检查是否是响应
        if (hdr.flags & FLAG_RESPONSE) {
            handleResponse(hdr.call_id, hdr.flags, body);
            return;
        }

        // 调用消息处理器 (在 I/O 线程中调用)
        if (msg_handler_) {
            msg_handler_(hdr.msg_type, sender_id, headers, body,
                         hdr.call_id, hdr.flags);
        }
    }

    void handleResponse(uint32_t call_id, uint8_t flags, const std::string& body) {
        std::lock_guard<std::mutex> lk(pending_mutex_);
        auto it = pending_.find(call_id);
        if (it != pending_.end()) {
            auto* pc = it->second;
            pc->response_body = body;
            pc->is_error       = (flags & FLAG_ERROR) != 0;
            pc->done           = true;

            // 唤醒等待线程
            uint64_t val = 1;
            ::write(pc->event_fd, &val, sizeof(val));
        }
    }

    // ---- 工具 ----
    Peer* getPeer(const std::string& node_id) {
        // 注意: lstl::unordered_map 的 operator[] 可能有副作用, 用 find
        for (auto& kv : peers_)
            if (kv.first == node_id && kv.second)
                return kv.second;
        return nullptr;
    }

    Peer* getPeerByFd(int fd) {
        for (auto& kv : fd_to_id_)
            if (kv.first == fd)
                return getPeer(kv.second);
        return nullptr;
    }

    void addEpoll(int fd, bool writable = false) {
        struct epoll_event ev;
        ev.events   = EPOLLIN;
        if (writable) ev.events |= EPOLLOUT;
        ev.data.fd  = fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
    }

    uint32_t nextCallId() {
        static std::atomic<uint32_t> counter{1};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }

    static uint64_t nowMs() {
        auto now = std::chrono::steady_clock::now();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());
    }

    // ---- 成员 ----
    int listen_port_ = 0;
    std::string bind_addr_;
    int listen_fd_ = -1;
    int epoll_fd_  = -1;
    int notify_fd_ = -1;

    std::thread io_thread_;
    std::atomic<bool> running_{false};

    // 连接表: node_id → Peer*
    lstl::unordered_map<std::string, Peer*> peers_;
    lstl::unordered_map<int, std::string>   fd_to_id_;

    // 写缓冲区锁
    std::mutex send_mutex_;

    // 待处理调用: call_id → PendingCall*
    std::mutex pending_mutex_;
    lstl::unordered_map<uint32_t, PendingCall*> pending_;

    // 消息处理器
    MessageHandler msg_handler_;
};

} // namespace lrpc
