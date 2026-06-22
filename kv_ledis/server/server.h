#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <lstl/container/unordered_map.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

#include "zero/net/tcp_server.h"
#include "zero/net/socket_stream.h"
#include "zero/scheduler/scheduler.h"
#include "zero/log/log.h"

#include "kv_ledis/server/session.h"
#include "kv_ledis/server/pubsub.h"
#include "kv_ledis/core/storage_engine.h"
#include "kv_ledis/core/command.h"
#include "kv_ledis/replication/aof_writer.h"
#include "kv_ledis/protocol/resp_writer.h"

namespace ledis {

// ============================================================
// LedisServer — 单线程执行 (类 Redis 架构)
// ============================================================
// 所有命令在 IO fiber 中直接执行，无需锁、无需队列、无上下文切换。
// 单调度线程保证 Dict 独占访问。
// 多线程扩展: 可启动多个实例，通过 cluster 模式分发请求。
//
class LedisServer : public std::enable_shared_from_this<LedisServer> {
public:
    using ptr = std::shared_ptr<LedisServer>;

    struct Config {
        std::string bind_addr   = "0.0.0.0";
        int         port         = 6379;
        int         io_threads   = 1;   // 单线程，如需多线程 IO 可加 reactor 线程
        std::string requirepass;
        std::string aof_path;
        AofWriter::FsyncMode aof_mode = AofWriter::EVERYSEC;
    };

    explicit LedisServer(Config cfg) : cfg_(std::move(cfg)) {
        g_logger_ = ZERO_LOG_ROOT();
    }

    ~LedisServer() { stop(); }

    bool start() {
        initCommandTable();
        start_time_sec_ = static_cast<uint64_t>(std::time(nullptr));

        scheduler_ = std::make_shared<zero::Scheduler>(1, false, "ledis");
        scheduler_->start();
        running_ = true;

        if (!cfg_.aof_path.empty()) {
            loadAof();
            aof_.start();
        }

        auto addr = zero::IPv4Address::Create(cfg_.bind_addr.c_str(), cfg_.port);
        if (!addr) {
            ZERO_LOG_ERROR(g_logger_) << "Invalid address";
            return false;
        }

        auto self = shared_from_this();
        server_ = std::make_shared<zero::TcpServer>(scheduler_.get(), addr, "ledis");
        server_->setConnectionCallback(
            [this](zero::Socket::ptr s) { onConnection(std::move(s)); });

        if (!server_->start()) {
            ZERO_LOG_ERROR(g_logger_) << "Failed to start TCP server";
            return false;
        }

        ZERO_LOG_INFO(g_logger_) << "Ledis listening on " << cfg_.bind_addr
            << ":" << cfg_.port << " (single-threaded)";
        return true;
    }

    void stop() {
        if (!running_.exchange(false)) return;
        if (server_) server_->stop();
        if (scheduler_) scheduler_->stop();
        aof_.stop();
        ZERO_LOG_INFO(g_logger_) << "Ledis stopped.";
    }

    const Config& config() const { return cfg_; }
    std::atomic<uint64_t> total_connections{0};
    std::atomic<uint64_t> total_commands{0};

private:
    void onConnection(zero::Socket::ptr sock) {
        sock->setTcpNoDelay(true);  // 禁用 Nagle, 小包即时发送
        auto session = std::make_shared<Session>(std::move(sock));
        session->authenticated = cfg_.requirepass.empty();
        total_connections++;
        scheduler_->schedule(
            [this, session]() { handleClient(session); });
    }

    static std::string toLower(std::string_view s) {
        std::string r; r.reserve(s.size());
        for (char c : s) r += static_cast<char>(c | 0x20);
        return r;
    }

    void handleClient(std::shared_ptr<Session> session) {
        char buf[65536];
        std::string out;
        out.reserve(65536);
        int cmd_count = 0;
        int fd = session->sock->getSocket();

        while (!session->closed && running_) {
            // 纯非阻塞读: 无 hook, 无 epoll 注册
            ssize_t n = ::recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 暂无数据, yield 让其他 fiber 运行
                    zero::Fiber::YieldToReady();
                    continue;
                }
                break;
            }
            if (n == 0) break;

            const char* data = buf;
            size_t remaining = static_cast<size_t>(n);
            out.clear();

            while (remaining > 0 && !session->closed) {
                session->parser.reset();
                size_t consumed = 0;
                auto r = session->parser.feed(data, remaining, consumed);
                if (r == RespParser::Result::NEED_MORE) break;
                if (r == RespParser::Result::ERROR) {
                    out += "-ERR Protocol error\r\n";
                    session->stream.writeFixed(out.data(), out.size());
                    session->closed = true; break;
                }
                if (r == RespParser::Result::OK && !session->parser.args().empty()) {
                    execute(session, session->parser.args(), out);
                    total_commands++; cmd_count++;
                    data += consumed; remaining -= consumed;
                }
            }

            if (!out.empty() && !session->closed) {
                // 非阻塞写: 跳过 hook
                ssize_t w = ::send(fd, out.data(), out.size(),
                                   MSG_DONTWAIT | MSG_NOSIGNAL);
                if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    // 缓冲区满, yield 后重试
                    zero::Fiber::YieldToReady();
                    continue;
                }
                if (w <= 0) { session->closed = true; break; }
            }

            // Pub/Sub 消息投递
            if (!session->closed && !session->pubsub_buf.empty()) {
                ssize_t w = ::send(fd, session->pubsub_buf.data(),
                                   session->pubsub_buf.size(),
                                   MSG_DONTWAIT | MSG_NOSIGNAL);
                if (w > 0) session->pubsub_buf.clear();
            }

            if (cmd_count >= 100) {
                cmd_count = 0;
                engine_.activeExpireCycle();
            }
        }
        // 清理 Pub/Sub 订阅
        pubsub_.cleanup(session.get());
        // 清理 Monitor
        if (session->is_monitor) {
            for (auto it = monitors_.begin(); it != monitors_.end(); ++it) {
                if (*it == session.get()) { monitors_.erase(it); break; }
            }
        }
        session->stream.close();
    }

    void execute(std::shared_ptr<Session>& s,
                 const lstl::vector<std::string_view>& args,
                 std::string& out) {
        // 栈上小写转换
        char lo[16];
        size_t sz = args[0].size();
        for (size_t i = 0; i < sz && i < 16; ++i) lo[i] = args[0][i] | 0x20;
        std::string_view cn(lo, sz);

        // 无需 Dict 的命令
        if (cn == "ping")  { out += "+PONG\r\n"; return; }
        if (cn == "echo")  { if (args.size()>=2) RespWriter::writeBulkString(out, args[1]); return; }
        if (cn == "command") { out += "*0\r\n"; return; }
        if (cn == "auth") {
            if (args.size() >= 2 && args[1] == cfg_.requirepass) out += "+OK\r\n";
            else out += "-ERR invalid password\r\n";
            s->authenticated = (args.size() >= 2 && args[1] == cfg_.requirepass);
            return;
        }
        if (cn == "multi")   { s->in_multi = true; s->multi_queue.clear(); out += "+OK\r\n"; return; }
        if (cn == "discard") { s->in_multi = false; s->multi_queue.clear(); s->watched_keys.clear(); out += "+OK\r\n"; return; }
        if (cn == "exec")    { execTransaction(s, out); return; }
        if (cn == "watch")   {
            s->watched_keys.clear();
            for (size_t i = 1; i < args.size(); ++i)
                s->watched_keys.push_back(std::string(args[i]) + ":" +
                    std::to_string(key_versions_[std::string(args[i])]));
            out += "+OK\r\n"; return;
        }
        if (cn == "unwatch") { s->watched_keys.clear(); out += "+OK\r\n"; return; }

        // ---- Pub/Sub ----
        if (cn == "subscribe") {
            for (size_t i = 1; i < args.size(); ++i) {
                pubsub_.subscribe(s.get(), args[i]);
                RespWriter::writeArrayHeader(out, 3);
                RespWriter::writeBulkString(out, "subscribe");
                RespWriter::writeBulkString(out, args[i]);
                RespWriter::writeInteger(out, static_cast<int>(s->channels.size()));
            }
            return;
        }
        if (cn == "unsubscribe") {
            if (args.size() == 1) {
                int n = pubsub_.unsubscribeAll(s.get());
                RespWriter::writeArrayHeader(out, 3);
                RespWriter::writeBulkString(out, "unsubscribe");
                RespWriter::writeNull(out);
                RespWriter::writeInteger(out, 0);
            } else {
                for (size_t i = 1; i < args.size(); ++i) {
                    pubsub_.unsubscribe(s.get(), args[i]);
                    RespWriter::writeArrayHeader(out, 3);
                    RespWriter::writeBulkString(out, "unsubscribe");
                    RespWriter::writeBulkString(out, args[i]);
                    RespWriter::writeInteger(out, static_cast<int>(s->channels.size()));
                }
            }
            return;
        }
        if (cn == "psubscribe") {
            for (size_t i = 1; i < args.size(); ++i) {
                pubsub_.psubscribe(s.get(), args[i]);
                RespWriter::writeArrayHeader(out, 3);
                RespWriter::writeBulkString(out, "psubscribe");
                RespWriter::writeBulkString(out, args[i]);
                RespWriter::writeInteger(out, static_cast<int>(s->patterns.size()));
            }
            return;
        }
        if (cn == "punsubscribe") {
            if (args.size() == 1) {
                pubsub_.punsubscribeAll(s.get());
            } else {
                for (size_t i = 1; i < args.size(); ++i)
                    pubsub_.punsubscribe(s.get(), args[i]);
            }
            out += "+OK\r\n"; return;
        }
        if (cn == "publish") {
            int count = pubsub_.publish(args[1], args[2]);
            out += ":"; out += std::to_string(count); out += "\r\n";
            return;
        }
        if (cn == "pubsub") {
            if (args.size() >= 2 && (args[1] == "CHANNELS" || args[1] == "channels")) {
                auto channels = pubsub_.pubsubChannels(args.size() >= 3 ? args[2] : "");
                RespWriter::writeStringArray(out, channels);
            } else if (args.size() >= 3 && (args[1] == "NUMSUB" || args[1] == "numsub")) {
                RespWriter::writeArrayHeader(out, static_cast<int64_t>((args.size()-2)*2));
                for (size_t i = 2; i < args.size(); ++i) {
                    RespWriter::writeBulkString(out, args[i]);
                    RespWriter::writeInteger(out, pubsub_.pubsubNumsub(args[i]));
                }
            } else if (args.size() >= 2 && (args[1] == "NUMPAT" || args[1] == "numpat")) {
                RespWriter::writeInteger(out, pubsub_.pubsubNumpat());
            }
            return;
        }

        // ---- 服务器管理 ----
        if (cn == "config") {
            if (args.size() >= 3 && (args[1] == "GET" || args[1] == "get")) {
                RespWriter::writeArrayHeader(out, 2);
                RespWriter::writeBulkString(out, args[2]);
                if (args[2] == "port") RespWriter::writeBulkString(out, std::to_string(cfg_.port));
                else if (args[2] == "bind") RespWriter::writeBulkString(out, cfg_.bind_addr);
                else if (args[2] == "requirepass") RespWriter::writeBulkString(out, cfg_.requirepass.empty() ? "" : cfg_.requirepass);
                else if (args[2] == "aof_path") RespWriter::writeBulkString(out, cfg_.aof_path);
                else RespWriter::writeNull(out);
            } else if (args.size() >= 4 && (args[1] == "SET" || args[1] == "set")) {
                if (args[2] == "requirepass") cfg_.requirepass = std::string(args[3]);
                out += "+OK\r\n";
            } else out += "+OK\r\n";
            return;
        }
        if (cn == "info") {
            std::string info;
            info += "# Server\r\n";
            info += "ledis_version:2.0.0\r\n";
            info += "uptime_in_seconds:" + std::to_string(static_cast<uint64_t>(std::time(nullptr)) - start_time_sec_) + "\r\n";
            info += "# Stats\r\n";
            info += "total_commands:" + std::to_string(total_commands.load()) + "\r\n";
            info += "total_connections:" + std::to_string(total_connections.load()) + "\r\n";
            info += "# Keyspace\r\n";
            info += "keys:" + std::to_string(engine_.size()) + "\r\n";
            RespWriter::writeBulkString(out, info);
            return;
        }
        if (cn == "client") {
            if (args.size() >= 3 && (args[1] == "SETNAME" || args[1] == "setname")) {
                s->name = std::string(args[2]); out += "+OK\r\n";
            } else if (args.size() >= 2 && (args[1] == "GETNAME" || args[1] == "getname")) {
                RespWriter::writeBulkString(out, s->name.empty() ? std::string_view{} : std::string_view(s->name));
            } else if (args.size() >= 2 && (args[1] == "LIST" || args[1] == "list")) {
                out += "+OK\r\n";  // stub
            } else if (args.size() >= 3 && (args[1] == "KILL" || args[1] == "kill")) {
                out += "+OK\r\n";  // stub
            }
            return;
        }
        if (cn == "monitor") {
            s->is_monitor = true;
            monitors_.push_back(s.get());
            out += "+OK\r\n"; return;
        }
        if (cn == "shutdown") {
            out += "+OK\r\n";
            running_ = false; return;
        }
        if (cn == "slowlog") {
            if (args.size() >= 2 && (args[1] == "GET" || args[1] == "get")) {
                int64_t n = 10;
                if (args.size() >= 3) tryParseInt64(std::string(args[2]), n);
                RespWriter::writeArrayHeader(out, static_cast<int64_t>(
                    std::min(static_cast<size_t>(n), slowlog_.size())));
                for (size_t i = slowlog_.size() > static_cast<size_t>(n) ? slowlog_.size() - static_cast<size_t>(n) : 0;
                     i < slowlog_.size(); ++i) {
                    auto& e = slowlog_[i];
                    RespWriter::writeArrayHeader(out, 4);
                    RespWriter::writeInteger(out, static_cast<int64_t>(e.id));
                    RespWriter::writeInteger(out, static_cast<int64_t>(e.time_us / 1000000));
                    RespWriter::writeInteger(out, static_cast<int64_t>(e.duration_us));
                    RespWriter::writeStringArray(out, e.args);
                }
            } else if (args.size() >= 2 && (args[1] == "RESET" || args[1] == "reset")) {
                slowlog_.clear(); out += "+OK\r\n";
            } else if (args.size() >= 2 && (args[1] == "LEN" || args[1] == "len")) {
                RespWriter::writeInteger(out, static_cast<int64_t>(slowlog_.size()));
            }
            return;
        }

        if (s->in_multi) {
            lstl::vector<std::string> qa;
            for (auto& a : args) qa.push_back(std::string(a));
            s->multi_queue.push_back(std::move(qa));
            out += "+QUEUED\r\n"; return;
        }

        // Monitor: 记录写命令
        if (!monitors_.empty() && args[0] != "ping") {
            std::string mon_msg;
            mon_msg += "+";
            mon_msg += s->remote_addr;
            mon_msg += " [0 0] ";
            for (auto& a : args) { mon_msg += "\""; mon_msg += std::string(a); mon_msg += "\" "; }
            mon_msg += "\r\n";
            for (auto* m : monitors_) m->pubsub_buf += mon_msg;  // reuse pubsub_buf for monitor output
        }

        CmdContext ctx;
        ctx.engine = &engine_;
        ctx.args = args;
        ctx.response = &out;

        auto start_us = std::chrono::steady_clock::now();
        dispatchCommand(ctx);
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start_us).count();

        // Slowlog
        if (elapsed >= slowlog_threshold_us_) {
            SlowlogEntry e;
            e.id = slowlog_id_++;
            e.time_us = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            e.duration_us = static_cast<uint64_t>(elapsed);
            for (auto& a : args) e.args.push_back(std::string(a));
            if (slowlog_.size() >= SLOWLOG_MAX) slowlog_.erase(slowlog_.begin());
            slowlog_.push_back(std::move(e));
        }

        if (ctx.is_write && args.size() >= 2)
            key_versions_[std::string(args[1])]++;
        if (ctx.is_write && !cfg_.aof_path.empty())
            aof_.appendArgs(args);
    }

    static bool tryParseInt64(const std::string& s, int64_t& out) {
        try { size_t p; out = std::stoll(s, &p); return p == s.size(); }
        catch (...) { return false; }
    }

    void execTransaction(std::shared_ptr<Session>& s, std::string& out) {
        if (!s->in_multi) {
            RespWriter::writeError(out, "ERR EXEC without MULTI"); return;
        }
        bool aborted = false;
        for (auto& wk : s->watched_keys) {
            auto colon = wk.find(':');
            if (colon != std::string::npos) {
                std::string key = wk.substr(0, colon);
                if (key_versions_[key] != std::stoull(wk.substr(colon + 1)))
                    { aborted = true; break; }
            }
        }
        s->watched_keys.clear();

        if (aborted) {
            RespWriter::writeNull(out);
        } else {
            RespWriter::writeArrayHeader(out, static_cast<int64_t>(s->multi_queue.size()));
            for (auto& qa : s->multi_queue) {
                lstl::vector<std::string_view> sv;
                for (auto& a : qa) sv.push_back(a);
                std::string sr;
                CmdContext ctx;
                ctx.engine = &engine_; ctx.args = sv; ctx.response = &sr;
                dispatchCommand(ctx); out.append(sr);
                if (ctx.is_write) {
                    if (sv.size() >= 2) key_versions_[std::string(sv[1])]++;
                    if (!cfg_.aof_path.empty()) aof_.appendArgs(sv);
                }
            }
        }
        s->in_multi = false; s->multi_queue.clear();
    }

    void loadAof() {
        int fd = ::open(cfg_.aof_path.c_str(), O_RDONLY);
        if (fd < 0) return;
        std::string content; content.reserve(1024*1024);
        char buf[65536]; ssize_t n;
        while ((n = ::read(fd, buf, sizeof(buf))) > 0)
            content.append(buf, static_cast<size_t>(n));
        ::close(fd);
        if (content.empty()) return;
        RespParser parser;
        const char* data = content.data(); size_t rem = content.size();
        while (rem > 0) {
            size_t consumed = 0;
            auto r = parser.feed(data, rem, consumed);
            if (r == RespParser::Result::ERROR || r == RespParser::Result::NEED_MORE) break;
            if (r == RespParser::Result::OK && !parser.args().empty()) {
                std::string dummy; CmdContext ctx;
                ctx.engine = &engine_; ctx.args = parser.args(); ctx.response = &dummy;
                dispatchCommand(ctx);
            }
            data += consumed; rem -= consumed; parser.reset();
        }
        ZERO_LOG_INFO(g_logger_) << "AOF loaded: " << engine_.size() << " keys";
    }

    Config cfg_;
    std::shared_ptr<zero::Scheduler> scheduler_;
    std::shared_ptr<zero::TcpServer> server_;
    StorageEngine engine_;
    AofWriter aof_;
    lstl::unordered_map<std::string, uint64_t> key_versions_;
    std::atomic<bool> running_{false};
    zero::Logger::ptr g_logger_;

    // Pub/Sub
    PubSubManager pubsub_;

    // Monitor
    lstl::vector<Session*> monitors_;

    // Slowlog
    struct SlowlogEntry {
        uint64_t id;
        uint64_t time_us;
        uint64_t duration_us;
        lstl::vector<std::string> args;
    };
    lstl::vector<SlowlogEntry> slowlog_;
    uint64_t slowlog_id_ = 0;
    static constexpr size_t SLOWLOG_MAX = 128;
    int64_t slowlog_threshold_us_ = 10000;  // 10ms

    // INFO
    uint64_t start_time_sec_ = 0;
};

} // namespace ledis
