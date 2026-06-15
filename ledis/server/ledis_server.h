#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <lstl/container/vector.h>
#include <sys/socket.h>
#include "zero/net/tcp_server.h"
#include "zero/net/socket_stream.h"
#include "zero/scheduler/scheduler.h"
#include "zero/scheduler/reactor.h"
#include "zero/net/address.h"
#include "zero/log/log.h"
#include "ledis/server/client_context.h"
#include "ledis/server/command_queue.h"
#include "ledis/server/storage_worker.h"
#include "ledis/storage/storage_engine.h"
#include "ledis/cmd/cmd_context.h"
#include "ledis/cmd/cmd_table.h"
#include "ledis/protocol/resp_parser.h"
#include "ledis/protocol/resp_writer.h"
#include "ledis/persistence/aof_writer.h"

namespace ledis {

class LedisServer : public std::enable_shared_from_this<LedisServer> {
public:
    using ptr = std::shared_ptr<LedisServer>;
    struct Config {
        std::string bind_addr="0.0.0.0"; int port=6379; int io_threads=3;
        int storage_shards=1; int tcp_backlog=511; int max_clients=10000;
        std::string log_level="info"; std::string requirepass="";
        std::string aof_path=""; AofWriter::FsyncMode aof_mode=AofWriter::EVERYSEC;
    };

    explicit LedisServer(Config cfg) : cfg_(std::move(cfg)) {
        g_logger = ZERO_LOG_ROOT();
        if(cfg_.io_threads<1) cfg_.io_threads=1;
        if(cfg_.storage_shards<1) cfg_.storage_shards=1;
    }
    ~LedisServer() { stop(); }

    bool start() {
        scheduler_ = std::make_shared<zero::Scheduler>(cfg_.io_threads, false, "ledis-io");
        scheduler_->start();
        // Create N storage shards
        for (int i = 0; i < cfg_.storage_shards; i++) {
            auto* sw = new StorageWorker(cfg_.io_threads,
                i==0 ? cfg_.aof_path : "", cfg_.aof_mode);
            if (!cfg_.requirepass.empty()) sw->setPassword(cfg_.requirepass);
            sw->start();
            shards_.push_back(sw);
        }
        auto addr = zero::IPv4Address::Create(cfg_.bind_addr.c_str(), cfg_.port);
        if (!addr) { ZERO_LOG_ERROR(g_logger)<<"Invalid address"; return false; }
        auto self = shared_from_this();
        server_ = std::make_shared<zero::TcpServer>(scheduler_.get(), addr, "ledis");
        server_->setConnectionCallback([this](zero::Socket::ptr s) { onConnection(std::move(s)); });
        if (!server_->start()) { ZERO_LOG_ERROR(g_logger)<<"Failed to start"; return false; }
        running_ = true;
        ZERO_LOG_INFO(g_logger)<<"Ledis listening on "<<cfg_.bind_addr<<":"<<cfg_.port
            <<" ("<<cfg_.io_threads<<" IO + "<<cfg_.storage_shards<<" storage shards)";
        return true;
    }

    void stop() {
        if (!running_.exchange(false)) return;
        if(server_) server_->stop();
        for (auto* s : shards_) { s->stop(); delete s; }
        shards_.clear();
        if(scheduler_) scheduler_->stop();
        ZERO_LOG_INFO(g_logger)<<"Ledis stopped.";
    }

    const Config& config() const { return cfg_; }
    std::atomic<uint64_t> total_connections{0};
    std::atomic<uint64_t> total_commands{0};

private:
    // Hash function for key → shard routing
    int getShardIndex(std::string_view key) const {
        if (key.empty() || cfg_.storage_shards <= 1) return 0;
        uint64_t h = 0;
        for (char c : key) { h = h * 31 + c; }
        return static_cast<int>(h % cfg_.storage_shards);
    }

    int getShardForCmd(const lstl::vector<std::string_view>& args) const {
        if (cfg_.storage_shards <= 1) return 0;
        // Use first key argument (args[1] for most commands)
        if (args.size() >= 2) return getShardIndex(args[1]);
        return 0;
    }

    void onConnection(zero::Socket::ptr sock) {
        int io_thread_id = getIoThreadId();
        handleClient(std::move(sock), io_thread_id);
    }

    void handleClient(zero::Socket::ptr sock, int io_thread_id) {
        ClientContext client(std::move(sock));
        client.io_thread_id = io_thread_id;
        total_connections++;

        char raw_buf[65536];
        while (!client.closed && running_) {
            ssize_t n = client.stream.read(raw_buf, sizeof(raw_buf));
            if (n <= 0) break;
            const char* data = raw_buf;
            size_t remaining = static_cast<size_t>(n);
            std::string read_batch;  // accumulated read responses
            int write_count = 0;

            // Phase 1: parse all commands, execute reads, collect writes
            while (remaining > 0 && !client.closed) {
                client.parser.reset();
                size_t consumed = 0;
                auto result = client.parser.feed(data, remaining, consumed);
                if (result == RespParser::Result::NEED_MORE) break;
                if (result == RespParser::Result::ERROR) {
                    read_batch.clear(); write_count = 0;
                    client.write_buf.clear();
                    RespWriter::writeError(client.write_buf, "ERR Protocol error: "+client.parser.errorMsg());
                    client.stream.writeFixed(client.write_buf.data(), client.write_buf.size());
                    client.closed = true; break;
                }
                if (result == RespParser::Result::OK) {
                    const auto& args = client.parser.args();
                    if (args.empty()) { data += consumed; remaining -= consumed; continue; }

                    int shard = getShardForCmd(args);
                    StorageWorker* sw = shards_[shard];
                    CommandQueue* cmd_queue = sw->getQueue(io_thread_id);
                    StorageEngine* engine = &sw->engine();

                    if (isReadOnlyCommand(args[0])) {
                        std::string rsp;
                        CmdContext ctx; ctx.client = &client; ctx.engine = engine;
                        ctx.args = args; ctx.response_buf = &rsp;
                        dispatchCommand(*engine, ctx);
                        read_batch.append(rsp);
                    } else {
                        PendingCommand cmd;
                        cmd.client = &client; cmd.args = args; cmd.db_index = client.db_index;
                        cmd_queue->push(std::move(cmd));
                        write_count++;
                    }
                    total_commands++;
                    data += consumed; remaining -= consumed;
                }
            }

            // Phase 2: flush read responses
            if (!read_batch.empty() && !client.closed) {
                ssize_t w = client.stream.writeFixed(read_batch.data(), read_batch.size());
                if (w <= 0) { client.closed = true; break; }
            }

            // Phase 3: wait for all writes (single yield!)
            if (write_count > 0 && !client.closed) {
                client.write_buf.clear();
                client.response_ready.store(false);
                {
                    zero::Reactor* reactor = zero::GetCurrentReactor();
                    reactor->addEvent(client.response_event_fd_, zero::Reactor::READ, zero::Fiber::GetThis());
                    zero::Fiber::YieldToHold();
                    uint64_t val = 0; ::read(client.response_event_fd_, &val, sizeof(val));
                }
                if (!client.closed && !client.write_buf.empty()) {
                    ssize_t w = client.stream.writeFixed(client.write_buf.data(), client.write_buf.size());
                    if (w <= 0) client.closed = true;
                }
            }

            if (!client.closed && (!client.channels.empty() || !client.patterns.empty())) {
                // PubSub uses shard 0
                pubsubMessageLoop(client, shards_[0]);
                if (client.closed) break;
            }
        }
        client.stream.close();
    }

    void pubsubMessageLoop(ClientContext& client, StorageWorker* sw) {
        zero::Reactor* reactor = zero::GetCurrentReactor();
        CommandQueue* cmd_queue = sw->getQueue(client.io_thread_id);
        if (!cmd_queue) return;
        while (!client.closed && running_ && (!client.channels.empty() || !client.patterns.empty())) {
            reactor->addEvent(client.response_event_fd_, zero::Reactor::READ, zero::Fiber::GetThis());
            zero::Fiber::YieldToHold();
            uint64_t val = 0; ::read(client.response_event_fd_, &val, sizeof(val));
            if (client.closed) break;
            if (!client.write_buf.empty()) {
                ssize_t w = client.stream.writeFixed(client.write_buf.data(), client.write_buf.size());
                if (w <= 0) { client.closed = true; break; }
                client.write_buf.clear();
            }
            char peek[4096];
            ssize_t n = ::recv(client.sock->getSocket(), peek, sizeof(peek), MSG_DONTWAIT);
            if (n > 0) {
                client.parser.reset(); size_t consumed = 0;
                auto r = client.parser.feed(peek, static_cast<size_t>(n), consumed);
                if (r == RespParser::Result::OK && !client.parser.args().empty()) {
                    client.response_ready.store(false);
                    PendingCommand cmd;
                    cmd.client = &client; cmd.args = client.parser.args(); cmd.db_index = client.db_index;
                    cmd_queue->push(std::move(cmd));
                }
            }
        }
    }

    // Fast read-only check: switch on first character (hot path for GET/SET benchmarks)
    static bool isReadOnlyCommand(std::string_view name) {
        if (name.empty()) return false;
        switch (name[0] | 0x20) {  // fast tolower
        case 'g': return name.size() == 3;                             // GET
        case 'm': return name.size() == 4;                             // MGET
        case 'l': return name.size() == 4;                             // LLEN
        case 'd': return true;                                         // DBSIZE
        case 'p': return true;                                         // PING, PTTL
        case 't': return true;                                         // TTL, TYPE, TIME
        case 'k': return true;                                         // KEYS
        case 'i': return true;                                         // INFO
        case 'r': return true;                                         // RANDOMKEY
        case 'c': return true;                                         // COMMAND, CONFIG, CLIENT
        case 'e': return name.size() == 4 || name.size() == 6;        // ECHO, EXISTS
        case 'h': return name.size() >= 3 && (name[1]|0x20) != 's';   // HGET/HEXISTS/HLEN... (not HSET)
        case 's': return (name[1]|0x20) != 'e';                        // SCARD/SISMEMBER... (not SET)
        case 'z': return (name[1]|0x20) == 's' || (name[1]|0x20) == 'r' || (name[1]|0x20) == 'c'; // ZSCORE/ZRANK/ZCARD
        default: {
            std::string lower;
            for (char ch : name) lower += (ch|0x20);
            const CmdInfo* info = lookupCmd(lower);
            return info && (info->flags & CMD_READONLY) && !(info->flags & CMD_PUBSUB);
        }}
    }

    int getIoThreadId() { return zero::t_per_thread ? zero::t_per_thread->thread_index : 0; }

    Config cfg_;
    std::shared_ptr<zero::Scheduler> scheduler_;
    std::shared_ptr<zero::TcpServer> server_;
    lstl::vector<StorageWorker*> shards_;
    std::atomic<bool> running_{false};
    zero::Logger::ptr g_logger;
};

} // namespace ledis
