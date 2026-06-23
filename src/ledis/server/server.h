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

#include "ledis/server/session.h"
#include "ledis/server/pubsub.h"
#include "ledis/server/blocking.h"
#include "ledis/core/storage_engine.h"
#include "ledis/core/command.h"
#include "ledis/core/eviction.h"
#include "ledis/core/lua_script.h"
#include "ledis/replication/aof_writer.h"
#include "ledis/protocol/resp_writer.h"
#include "ledis/cluster/cluster_config.h"
#include "ledis/cluster/cluster_manager.h"

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
        int         io_threads   = 1;
        std::string requirepass;
        std::string aof_path;
        AofWriter::FsyncMode aof_mode = AofWriter::EVERYSEC;

        // 集群
        bool        cluster_enabled      = false;
        int         cluster_port         = 0;
        std::string cluster_seeds;
        int         cluster_replicas     = 0;
        int         cluster_gossip_ms    = 1000;
        int         cluster_timeout_ms   = 15000;

        // 慢日志
        int         slowlog_max_len      = 128;
        int64_t     slowlog_threshold_us = 10000;

        // 淘汰
        size_t      maxmemory            = 0;      // 0 = unlimited
        std::string eviction_policy      = "noeviction";
        int         eviction_samples     = 20;

        // 过期 / 缓冲区
        int         active_expire_cycles = 16;
        int         recv_buf_size        = 65536;

        // ACL 用户: username → password
        std::map<std::string, std::string> acl_users;
    };

    explicit LedisServer(Config cfg) : cfg_(std::move(cfg)) {
        g_logger_ = ZERO_LOG_ROOT();
        if (g_logger_->getLevel() == zero::LogLevel::DEBUG) {
            g_logger_->setLevel(zero::LogLevel::INFO);
        }
        // 从配置初始化可调参数
        slowlog_max_          = cfg_.slowlog_max_len;
        slowlog_threshold_us_ = cfg_.slowlog_threshold_us;
        active_expire_cycles_ = cfg_.active_expire_cycles;
        eviction_.setMaxmemory(cfg_.maxmemory);
        if (!cfg_.eviction_policy.empty()) {
            eviction_.setPolicy(evictionPolicyFromString(cfg_.eviction_policy));
        }
        // 加载 ACL 用户
        for (auto& [user, pass] : cfg_.acl_users) {
            acl_users_[user] = pass;
        }
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

        // 启动集群
        if (cfg_.cluster_enabled) {
            cluster::ClusterConfig cl_cfg;
            cl_cfg.enabled           = true;
            cl_cfg.bind_addr         = cfg_.bind_addr;
            cl_cfg.port              = cfg_.port;
            cl_cfg.cluster_port      = cfg_.cluster_port > 0 ? cfg_.cluster_port : cfg_.port + 10000;
            cl_cfg.replicas          = cfg_.cluster_replicas;
            cl_cfg.gossip_interval_ms = cfg_.cluster_gossip_ms;
            cl_cfg.node_timeout_ms    = cfg_.cluster_timeout_ms;

            // 解析种子节点
            if (!cfg_.cluster_seeds.empty()) {
                std::string seeds = cfg_.cluster_seeds;
                size_t pos = 0;
                while (pos < seeds.size()) {
                    size_t comma = seeds.find(',', pos);
                    std::string s = (comma == std::string::npos)
                        ? seeds.substr(pos) : seeds.substr(pos, comma - pos);
                    cl_cfg.seeds.push_back(s);
                    pos = (comma == std::string::npos) ? seeds.size() : comma + 1;
                }
            }

            cluster_ = std::make_shared<cluster::ClusterManager>(cl_cfg, &engine_, g_logger_);
            if (!cluster_->start()) {
                ZERO_LOG_ERROR(g_logger_) << "Failed to start cluster";
                return false;
            }
            cluster_->setClientPort(cfg_.port);  // 必须在 start() 之后 (self node 已创建)
            ZERO_LOG_INFO(g_logger_) << "Cluster started on port "
                << cl_cfg.cluster_port;
        }

        ZERO_LOG_INFO(g_logger_) << "Ledis listening on " << cfg_.bind_addr
            << ":" << cfg_.port << " (single-threaded)";
        return true;
    }

    void stop() {
        if (!running_.exchange(false)) return;
        if (server_) server_->stop();
        if (scheduler_) scheduler_->stop();
        if (cluster_) cluster_->stop();
        aof_.stop();
        ZERO_LOG_INFO(g_logger_) << "Ledis stopped.";
    }

    const Config& config() const { return cfg_; }
    void clusterTick() {
        if (cluster_) { cluster_->processPendingMessages(); cluster_->tick(); }
    }
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
            // 集群维护 (每个循环迭代都检查)
            if (cluster_) {
                cluster_->processPendingMessages();
                cluster_->tick();
            }

            // 纯非阻塞读: 无 hook, 无 epoll 注册
            ssize_t n = ::recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 暂无客户端数据，检查集群消息
                    if (cluster_) {
                        cluster_->processPendingMessages();
                        cluster_->tick();
                    }
                    // yield 让其他 fiber 运行
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
                engine_.activeExpireCycle(active_expire_cycles_);
                // 集群维护
                if (cluster_) {
                    cluster_->processPendingMessages();
                    cluster_->tick();
                }
            }
        }
        // 清理
        pubsub_.cleanup(session.get());
        blocking_.cleanup(session.get());
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
            // AUTH [username] password  (Redis 6+ ACL)
            std::string user, pwd;
            if (args.size() == 2) {
                // AUTH password → 使用 "default" 用户
                user = "default";
                pwd  = std::string(args[1]);
            } else if (args.size() >= 3) {
                // AUTH username password
                user = std::string(args[1]);
                pwd  = std::string(args[2]);
            } else {
                out += "-ERR wrong number of arguments for 'auth' command\r\n";
                return;
            }

            bool ok = false;
            if (user == "default" && !cfg_.requirepass.empty()) {
                ok = (pwd == cfg_.requirepass);
            } else {
                auto it = acl_users_.find(user);
                ok = (it != acl_users_.end() && it->second == pwd);
            }
            // 无密码配置时总是通过
            if (cfg_.requirepass.empty() && acl_users_.empty()) ok = true;

            if (ok) {
                s->authenticated = true;
                s->auth_user = user;
                out += "+OK\r\n";
            } else {
                out += "-WRONGPASS invalid username-password pair or user is disabled.\r\n";
            }
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

        // ---- Lua ----
        if (cn == "eval") { lua_.eval(args, out); return; }
        if (cn == "evalsha") { lua_.evalsha(args, out); return; }
        if (cn == "script") {
            if (args.size() >= 3 && (args[1] == "LOAD" || args[1] == "load"))
                lua_.scriptLoad(args, out);
            else if (args.size() >= 3 && (args[1] == "EXISTS" || args[1] == "exists"))
                lua_.scriptExists(args, out);
            else if (args.size() >= 2 && (args[1] == "FLUSH" || args[1] == "flush"))
                lua_.scriptFlush(out);
            else out += "+OK\r\n";
            return;
        }

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
                else if (args[2] == "maxmemory") RespWriter::writeBulkString(out, std::to_string(eviction_.maxmemory()));
                else if (args[2] == "maxmemory-policy") RespWriter::writeBulkString(out, evictionPolicyName(eviction_.policy()));
                else if (args[2] == "aof_path") RespWriter::writeBulkString(out, cfg_.aof_path);
                else RespWriter::writeNull(out);
            } else if (args.size() >= 4 && (args[1] == "SET" || args[1] == "set")) {
                if (args[2] == "requirepass") cfg_.requirepass = std::string(args[3]);
                else if (args[2] == "maxmemory") eviction_.setMaxmemory(static_cast<size_t>(std::stoull(std::string(args[3]))));
                else if (args[2] == "maxmemory-policy") eviction_.setPolicy(evictionPolicyFromString(std::string(args[3])));
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
            info += "# Memory\r\n";
            info += "maxmemory:" + std::to_string(eviction_.maxmemory()) + "\r\n";
            info += "used_memory_estimate:" + std::to_string(eviction_.estimateMemory(engine_.dict())) + "\r\n";
            info += "maxmemory_policy:" + std::string(evictionPolicyName(eviction_.policy())) + "\r\n";
            RespWriter::writeBulkString(out, info);
            return;
        }
        if (cn == "acl") {
            std::string sub;
            if (args.size() >= 2) { sub.reserve(args[1].size()); for (char c : args[1]) sub += c | 0x20; }

            if (sub == "whoami") {
                RespWriter::writeSimpleString(out, s->authenticated ? ("User " + s->auth_user) : "User default");
            }
            else if (sub == "list") {
                lstl::vector<std::string> lines;
                lines.push_back("user default on >" + cfg_.requirepass + " ~* +@all");
                for (auto& kv : acl_users_)
                    lines.push_back("user " + kv.first + " on >" + kv.second + " ~* +@all");
                RespWriter::writeStringArray(out, lines);
            }
            else if (sub == "users") {
                lstl::vector<std::string> names;
                names.push_back("default");
                for (auto& kv : acl_users_) names.push_back(kv.first);
                RespWriter::writeStringArray(out, names);
            }
            else if (sub == "setuser" && args.size() >= 4) {
                std::string username(args[2]);
                std::string action(args[3]);
                if (action == ">" && args.size() >= 5) {
                    acl_users_[std::move(username)] = std::string(args[4]);
                    out += "+OK\r\n";
                } else {
                    out += "+OK\r\n";
                }
            }
            else if (sub == "deluser" && args.size() >= 3) {
                int n = static_cast<int>(acl_users_.erase(std::string(args[2])));
                RespWriter::writeInteger(out, n);
            }
            else if (sub == "getuser" && args.size() >= 3) {
                std::string username(args[2]);
                auto it = acl_users_.find(username);
                if (it != acl_users_.end() || username == "default") {
                    RespWriter::writeArrayHeader(out, 6);
                    RespWriter::writeBulkString(out, "flags"); RespWriter::writeBulkString(out, "on");
                    RespWriter::writeBulkString(out, "passwords");
                    RespWriter::writeArrayHeader(out, 1);
                    if (username == "default") RespWriter::writeBulkString(out, cfg_.requirepass);
                    else RespWriter::writeBulkString(out, it->second);
                    RespWriter::writeBulkString(out, "commands"); RespWriter::writeBulkString(out, "+@all");
                } else {
                    RespWriter::writeNull(out);
                }
            }
            else if (sub == "load" || sub == "save") {
                out += "+OK\r\n";
            }
            else {
                out += "-ERR Unknown ACL subcommand\r\n";
            }
            return;
        }
        if (cn == "client") {
            if (args.size() >= 2 && (args[1] == "ID" || args[1] == "id")) {
                RespWriter::writeInteger(out, reinterpret_cast<int64_t>(s.get()));
            } else if (args.size() >= 2 && (args[1] == "INFO" || args[1] == "info")) {
                std::string ci;
                ci += "id=" + std::to_string(reinterpret_cast<int64_t>(s.get()));
                ci += " addr=" + s->remote_addr;
                ci += " name=" + (s->name.empty() ? "" : s->name);
                ci += " db=0";
                RespWriter::writeBulkString(out, ci);
            } else if (args.size() >= 3 && (args[1] == "SETNAME" || args[1] == "setname")) {
                s->name = std::string(args[2]); out += "+OK\r\n";
            } else if (args.size() >= 2 && (args[1] == "GETNAME" || args[1] == "getname")) {
                RespWriter::writeBulkString(out, s->name.empty() ? std::string_view{} : std::string_view(s->name));
            } else if (args.size() >= 3 && (args[1] == "UNBLOCK" || args[1] == "unblock")) {
                out += ":1\r\n";  // simplified
            } else if (args.size() >= 2 && (args[1] == "LIST" || args[1] == "list")) {
                out += "*0\r\n";
            } else if (args.size() >= 3 && (args[1] == "KILL" || args[1] == "kill")) {
                out += "+OK\r\n";
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

        // ---- 集群路由 (分布式命令转发) ----
        if (cluster_ && cluster_->enabled()) {
            if (cluster_->routeCommand(ctx))
                return;  // 命令已被集群处理 (本地 cluster 命令或已转发)
        }

        // ---- BZPOPMIN / BZPOPMAX 阻塞处理 ----
        if (cn == "bzpopmin" || cn == "bzpopmax") {
            bool is_min = (cn == "bzpopmin");
            int64_t timeout = 0;
            size_t key_end = args.size();
            auto& last = args[args.size() - 1];
            bool last_is_int = true;
            for (char c : last) if (c < '0' || c > '9') last_is_int = false;
            if (last_is_int && args.size() >= 2) {
                try { timeout = std::stoll(std::string(last)); } catch (...) {}
                key_end = args.size() - 1;
            }
            for (size_t i = 1; i < key_end; ++i) {
                Value* v = engine_.find(std::string(args[i]));
                if (v && v->type == ValueType::ZSET && !v->isExpired(CmdContext::nowMs())) {
                    auto* zd = v->asZSet();
                    if (!zd->by_score.empty()) {
                        auto elem = is_min ? *zd->by_score.begin() : *(----zd->by_score.end());
                        // collect max element (set has no rbegin)
                        if (!is_min) {
                            double mx = -1e308; std::string mm;
                            for (auto& e : zd->by_score)
                                if (e.first >= mx) { mx = e.first; mm = e.second; }
                            elem = {mx, mm};
                        }
                        RespWriter::writeArrayHeader(out, 3);
                        RespWriter::writeBulkString(out, args[i]);
                        RespWriter::writeBulkString(out, elem.second);
                        char buf[64]; snprintf(buf, sizeof(buf), "%.17g", elem.first);
                        RespWriter::writeBulkString(out, std::string(buf));
                        zd->scores.erase(elem.second);
                        zd->by_score.erase(elem);
                        if (args.size() >= 2) key_versions_[std::string(args[1])]++;
                        return;
                    }
                }
            }
            if (timeout > 0) {
                s->blocked = true;
                lstl::vector<std::string> keys;
                for (size_t i = 1; i < key_end; ++i) keys.push_back(std::string(args[i]));
                blocking_.addWaiter(s.get(), keys, timeout);
                s->write_buf_for_block = &out;
                uint64_t deadline = static_cast<uint64_t>(time(nullptr))*1000 + static_cast<uint64_t>(timeout)*1000;
                while (s->blocked && !s->closed) {
                    uint64_t now = static_cast<uint64_t>(time(nullptr))*1000;
                    if (now >= deadline) break;
                    zero::Fiber::YieldToReady();
                }
                if (!s->blocked) return;
                out += "*-1\r\n"; return;
            } else { out += "*-1\r\n"; return; }
        }

        // ---- BLPOP / BRPOP 阻塞处理 ----
        if (cn == "blpop" || cn == "brpop") {
            bool left = (cn == "blpop");
            int64_t timeout = 0;
            size_t key_end = args.size();
            // 最后一个参数可能是 timeout
            if (args.size() >= 2) {
                auto& last = args[args.size() - 1];
                bool is_int = true;
                for (char c : last) if (c < '0' || c > '9') { is_int = false; break; }
                if (is_int) {
                    try { timeout = std::stoll(std::string(last)); } catch (...) {}
                    key_end = args.size() - 1;
                }
            }

            // 尝试弹出
            for (size_t i = 1; i < key_end; ++i) {
                Value* v = engine_.find(std::string(args[i]));
                if (v && v->type == ValueType::LIST && !v->isExpired(CmdContext::nowMs())) {
                    auto* ld = v->asList();
                    if (!ld->elements.empty()) {
                        std::string val = left ? std::move(ld->elements.front()) : std::move(ld->elements.back());
                        if (left) ld->elements.pop_front(); else ld->elements.pop_back();
                        RespWriter::writeArrayHeader(out, 2);
                        RespWriter::writeBulkString(out, args[i]);
                        RespWriter::writeBulkString(out, val);
                        if (key_end >= 3) key_versions_[std::string(args[1])]++;
                        if (!cfg_.aof_path.empty()) {
                            lstl::vector<std::string_view> aof_args;
                            aof_args.push_back(args[0]);
                            aof_args.push_back(args[i]);
                            aof_args.push_back(val);
                            aof_.appendArgs(aof_args);
                        }
                        return;
                    }
                }
            }

            // 无数据，阻塞
            if (timeout > 0) {
                s->blocked = true;
                lstl::vector<std::string> keys;
                for (size_t i = 1; i < key_end; ++i) keys.push_back(std::string(args[i]));
                blocking_.addWaiter(s.get(), keys, timeout);
                s->write_buf_for_block = &out;  // 用于 LPUSH 时写入响应
                // spin-wait 直到被唤醒或超时
                uint64_t deadline = static_cast<uint64_t>(time(nullptr)) * 1000 + static_cast<uint64_t>(timeout) * 1000;
                while (s->blocked && !s->closed) {
                    uint64_t now = static_cast<uint64_t>(time(nullptr)) * 1000;
                    if (now >= deadline) break;
                    zero::Fiber::YieldToReady();
                }
                if (!s->blocked) return;  // 已在 LPUSH 中写入响应
                // 超时: 返回 nil
                out += "*-1\r\n";
                return;
            } else {
                out += "*-1\r\n";  // timeout=0, 立即返回
                return;
            }
        }

        // ---- LPUSH/RPUSH 后检查阻塞等待者 ----
        if (cn == "lpush" || cn == "rpush") {
            // 先执行 push
            CmdContext ctx;
            ctx.engine = &engine_; ctx.args = args; ctx.response = &out;
            dispatchCommand(ctx);
            if (ctx.is_write && args.size() >= 2) key_versions_[std::string(args[1])]++;
            if (ctx.is_write && !cfg_.aof_path.empty()) aof_.appendArgs(args);

            // 检查阻塞等待者
            auto* w = blocking_.popWaiter(std::string(args[1]));
            if (w && w->session && !w->session->closed) {
                // 重新获取 list (可能刚被 push 修改)
                Value* v = engine_.find(w->keys.empty() ? "" : w->keys[0]);
                if (v && v->type == ValueType::LIST) {
                    auto* ld = v->asList();
                    if (!ld->elements.empty()) {
                        bool left = true;  // BLPOP 默认
                        std::string val = left ? std::move(ld->elements.front()) : std::move(ld->elements.back());
                        if (left) ld->elements.pop_front(); else ld->elements.pop_back();
                        // 写响应到被阻塞的 session
                        std::string& wb = w->session->pubsub_buf;  // 复用 pubsub_buf
                        wb.clear();
                        RespWriter::writeArrayHeader(wb, 2);
                        RespWriter::writeBulkString(wb, args[1]);
                        RespWriter::writeBulkString(wb, val);
                        w->session->blocked = false;
                    }
                }
                delete w;
            }
            return;
        }

        // maxmemory 淘汰检查 (写命令前)
        const CmdInfo* info = lookupCommand(std::string(args[0]));
        bool is_write_cmd = info && (info->flags & CMD_WRITE);
        if (is_write_cmd && eviction_.maxmemory() > 0) {
            if (eviction_.policy() == EVICT_NOEVICTION) {
                size_t used = eviction_.estimateMemory(engine_.dict());
                if (used > eviction_.maxmemory()) {
                    out += "-ERR OOM command not allowed when used memory > 'maxmemory'\r\n";
                    return;
                }
            } else {
                size_t target = eviction_.maxmemory() > 1048576
                    ? eviction_.maxmemory() - 1048576 : eviction_.maxmemory() / 2;
                eviction_.evict(engine_.dict(), target);
            }
        }

        // XREAD BLOCK 处理 (dispatchCommand 返回后检查)
        bool is_xread_block = false;
        {
            std::string lc; for (char c : args[0]) lc += c | 0x20;
            is_xread_block = (lc == "xread" && ctx.block_for_stream);
        }

        auto start_us = std::chrono::steady_clock::now();
        dispatchCommand(ctx);

        // XREAD BLOCK: 无数据时阻塞等待
        if (is_xread_block && ctx.block_for_stream) {
            s->blocked = true;
            int64_t timeout = ctx.block_ms;
            ctx.block_for_stream = false;
            uint64_t deadline = static_cast<uint64_t>(time(nullptr))*1000 + static_cast<uint64_t>(timeout);
            // 注册到 block manager (用第一个 key)
            if (!ctx.block_keys.empty()) {
                lstl::vector<std::string> bkeys;
                for (auto& k : ctx.block_keys) bkeys.push_back(k);
                blocking_.addWaiter(s.get(), bkeys, timeout / 1000);
            }
            while (s->blocked && !s->closed) {
                if (static_cast<uint64_t>(time(nullptr))*1000 >= deadline) break;
                zero::Fiber::YieldToReady();
            }
            if (!s->blocked) return;  // XADD 已唤醒并写入响应
            // 超时: 返回 null
            out.clear();
            out += "$-1\r\n";
            return;
        }
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
            if (slowlog_.size() >= slowlog_max_) slowlog_.erase(slowlog_.begin());
            slowlog_.push_back(std::move(e));
        }

        // 写命令后自动唤醒阻塞等待者 (LPUSH/ZADD 等)
        if (ctx.is_write && args.size() >= 2) {
            std::string wk(args[1]);
            key_versions_[wk]++;
            auto* w = blocking_.popWaiter(wk);
            if (w && w->session && !w->session->closed) {
                // 重新获取数据并写响应
                Value* wv = engine_.find(w->keys.empty() ? "" : w->keys[0]);
                std::string& wb = w->session->pubsub_buf; wb.clear();
                if (wv && wv->type == ValueType::LIST && !wv->asList()->elements.empty()) {
                    auto* ld = wv->asList();
                    std::string val = std::move(ld->elements.front());
                    ld->elements.pop_front();
                    RespWriter::writeArrayHeader(wb, 2);
                    RespWriter::writeBulkString(wb, w->keys[0]);
                    RespWriter::writeBulkString(wb, val);
                } else if (wv && wv->type == ValueType::ZSET && !wv->asZSet()->by_score.empty()) {
                    auto* zd = wv->asZSet();
                    auto elem = *zd->by_score.begin();
                    RespWriter::writeArrayHeader(wb, 3);
                    RespWriter::writeBulkString(wb, w->keys[0]);
                    RespWriter::writeBulkString(wb, elem.second);
                    char b[64]; snprintf(b, sizeof(b), "%.17g", elem.first);
                    RespWriter::writeBulkString(wb, std::string(b));
                    zd->scores.erase(elem.second);
                    zd->by_score.erase(elem);
                }
                w->session->blocked = false;
                delete w;
            }
        }
        if (ctx.is_write && !cfg_.aof_path.empty())
            aof_.appendArgs(args);
        // 集群复制: 写命令后异步复制到副本节点
        if (ctx.is_write && cluster_) {
            cluster_->onWriteCommand(args);
        }
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

    // Blocking
    BlockingManager blocking_;

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
    size_t slowlog_max_;              // from cfg_.slowlog_max_len
    int64_t slowlog_threshold_us_;    // from cfg_.slowlog_threshold_us

    // INFO
    uint64_t start_time_sec_ = 0;

    // Eviction
    EvictionManager eviction_;

    // 过期循环次数 (from cfg_)
    int active_expire_cycles_;

    // Lua
    LuaScriptEngine lua_{&engine_};

    // ACL
    lstl::unordered_map<std::string, std::string> acl_users_;

    // Cluster
    cluster::ClusterManager::ptr cluster_;
};

} // namespace ledis
