#pragma once
#include <atomic>
#include <memory>
#include <thread>
#include <lstl/container/vector.h>
#include <poll.h>
#include "ledis/server/command_queue.h"
#include "ledis/storage/storage_engine.h"
#include "ledis/cmd/cmd_context.h"
#include "ledis/cmd/cmd_table.h"
#include "ledis/server/client_context.h"
#include "ledis/persistence/aof_writer.h"
#include "ledis/protocol/resp_parser.h"
#include "ledis/server/pubsub_manager.h"
#include "ledis/server/slowlog.h"

namespace ledis {

class StorageWorker {
public:
    StorageWorker(int num_io_threads, const std::string& aof_path = "",
                  AofWriter::FsyncMode aof_mode = AofWriter::EVERYSEC)
        : aof_enabled_(!aof_path.empty()), aof_(aof_path, aof_mode) {
        for (int i = 0; i < num_io_threads; ++i) queues_.push_back(new CommandQueue());
    }
    ~StorageWorker() { stop(); for (auto* q : queues_) delete q; }

    CommandQueue* getQueue(int id) { return (id>=0&&id<(int)queues_.size())?queues_[id]:nullptr; }
    int queueCount() const { return (int)queues_.size(); }
    uint64_t totalProcessed() const { return total_processed_.load(); }
    StorageEngine& engine() { return engine_; }
    PubSubManager& pubsub() { return pubsub_; }
    void setPassword(const std::string& pw) { password_ = pw; }
    const std::string& password() const { return password_; }

    void start() {
        if (aof_enabled_) { loadAof(); aof_.start(); }
        running_ = true; thread_ = std::thread(&StorageWorker::run, this);
    }
    void stop() {
        running_ = false; if (thread_.joinable()) thread_.join();
        if (aof_enabled_) aof_.stop();
    }

    void loadAof() {
        int fd = open(aof_.path().c_str(), O_RDONLY);
        if (fd < 0) return;
        std::string content; content.reserve(1024*1024);
        char buf[65536]; ssize_t n;
        while ((n = ::read(fd, buf, sizeof(buf))) > 0) content.append(buf, n);
        ::close(fd);
        if (content.empty()) return;
        RespParser parser;
        const char* data = content.data(); size_t rem = content.size(); uint64_t loaded = 0;
        while (rem > 0) {
            size_t consumed = 0;
            auto r = parser.feed(data, rem, consumed);
            if (r == RespParser::Result::ERROR || r == RespParser::Result::NEED_MORE) break;
            if (r == RespParser::Result::OK) {
                const auto& args = parser.args();
                if (!args.empty()) {
                    std::string dummy; CmdContext ctx;
                    ctx.engine = &engine_; ctx.args = args; ctx.response_buf = &dummy;
                    dispatchCommand(engine_, ctx); loaded++;
                }
                data += consumed; rem -= consumed; parser.reset();
            }
        }
        // AOF recovery complete (logged via zero logging framework)
        static auto aof_logger = ZERO_LOG_ROOT();
        ZERO_LOG_INFO(aof_logger) << "AOF loaded: " << loaded
            << " commands, " << engine_.size() << " keys restored";
    }

private:
    void run() {
        lstl::vector<struct pollfd> fds(queues_.size());
        while (running_) {
            for (size_t i = 0; i < queues_.size(); ++i) {
                fds[i].fd = queues_[i]->eventFd(); fds[i].events = POLLIN; fds[i].revents = 0;
            }
            bool did_work = drainAll();
            int timeout_ms = did_work ? 0 : 1;
            if (poll(fds.data(), fds.size(), timeout_ms) > 0) {
                for (size_t i = 0; i < fds.size(); ++i)
                    if (fds[i].revents & POLLIN) { uint64_t v; ::read(fds[i].fd, &v, sizeof(v)); }
                drainAll();
            }
            periodicTasks();
        }
    }

    bool drainAll() {
        bool did_work = false;
        for (auto& q : queues_) { auto b = q->drain(); if (!b.empty()) { processBatch(b); did_work = true; } }
        return did_work;
    }

    void processBatch(lstl::vector<PendingCommand>& batch) {
        for (auto& cmd : batch) executeCommand(cmd);
        total_processed_ += batch.size();
    }

    void executeCommand(PendingCommand& cmd) {
        ClientContext* client = cmd.client;
        if (!client || client->closed) return;

        // AUTH
        std::string_view cn = cmd.args.empty() ? std::string_view{} : cmd.args[0];
        if (cn == "AUTH" || cn == "auth") {
            client->write_buf.clear();
            if (cmd.args.size() >= 2 && cmd.args[1] == password_) { client->authenticated = true; RespWriter::writeOK(client->write_buf); }
            else { RespWriter::writeError(client->write_buf, "ERR invalid password"); }
            notifyClient(client); return;
        }
        if (!client->authenticated && !password_.empty() &&
            cn != "PING" && cn != "ping" && cn != "COMMAND" && cn != "command") {
            client->write_buf.clear(); RespWriter::writeError(client->write_buf, "NOAUTH Authentication required.");
            notifyClient(client); return;
        }

        // WATCH / UNWATCH
        if (cn == "WATCH" || cn == "watch") {
            client->write_buf.clear(); client->watched_keys.clear();
            for (size_t i = 1; i < cmd.args.size(); ++i) {
                std::string key(cmd.args[i]);
                client->watched_keys.push_back(key + ":" + std::to_string(key_versions_[key]));
            }
            RespWriter::writeOK(client->write_buf); notifyClient(client); return;
        }
        if (cn == "UNWATCH" || cn == "unwatch") {
            client->write_buf.clear(); client->watched_keys.clear();
            RespWriter::writeOK(client->write_buf); notifyClient(client); return;
        }

        // MULTI / EXEC / DISCARD
        if (cn == "MULTI" || cn == "multi") {
            client->write_buf.clear(); client->in_multi = true; client->multi_queue.clear();
            RespWriter::writeOK(client->write_buf); notifyClient(client); return;
        }
        if (cn == "DISCARD" || cn == "discard") {
            client->write_buf.clear(); client->in_multi = false; client->multi_queue.clear(); client->watched_keys.clear();
            RespWriter::writeOK(client->write_buf); notifyClient(client); return;
        }
        if (cn == "EXEC" || cn == "exec") {
            client->write_buf.clear();
            if (!client->in_multi) { RespWriter::writeError(client->write_buf, "ERR EXEC without MULTI"); notifyClient(client); return; }
            bool aborted = false;
            for (auto& wk : client->watched_keys) {
                auto colon = wk.find(':');
                if (colon != std::string::npos) {
                    std::string key = wk.substr(0, colon);
                    uint64_t ver = std::stoull(wk.substr(colon + 1));
                    if (key_versions_[key] != ver) { aborted = true; break; }
                }
            }
            client->watched_keys.clear();
            if (aborted) { RespWriter::writeNull(client->write_buf); client->in_multi = false; client->multi_queue.clear(); notifyClient(client); return; }
            std::string& buf = client->write_buf;
            RespWriter::writeArrayHeader(buf, (int64_t)client->multi_queue.size());
            for (auto& qa : client->multi_queue) {
                lstl::vector<std::string_view> sv;
                for (auto& a : qa) sv.push_back(a);
                std::string sr; CmdContext ctx;
                ctx.client = client; ctx.engine = &engine_; ctx.pubsub = &pubsub_;
                ctx.args = sv; ctx.response_buf = &sr;
                dispatchCommand(engine_, ctx); buf.append(sr);
                if (aof_enabled_ && ctx.is_write) aof_.appendArgs(sv);
            }
            client->in_multi = false; client->multi_queue.clear(); notifyClient(client); return;
        }

        // Queue in MULTI mode
        if (client->in_multi) {
            client->write_buf.clear();
            lstl::vector<std::string> oa;
            for (auto& a : cmd.args) oa.push_back(std::string(a));
            client->multi_queue.push_back(std::move(oa));
            RespWriter::writeSimpleString(client->write_buf, "QUEUED"); notifyClient(client); return;
        }

        // Normal command (write_buf cleared by IO fiber before batch)
        CmdContext ctx;
        ctx.client = client; ctx.engine = &engine_; ctx.pubsub = &pubsub_;
        ctx.args = cmd.args; ctx.response_buf = &client->write_buf; ctx.db_index = cmd.db_index;

        auto start_us = std::chrono::steady_clock::now();
        dispatchCommand(engine_, ctx);
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start_us).count();
        slowlog_.record(elapsed, cmd.args, client->remote_addr);

        if (ctx.is_write && cmd.args.size() >= 2) key_versions_[std::string(cmd.args[1])]++;
        if (aof_enabled_ && ctx.is_write) aof_.appendArgs(cmd.args);

        notifyClient(client);
    }

    void notifyClient(ClientContext* client) {
        client->response_ready.store(true, std::memory_order_release);
        uint64_t v = 1; ::write(client->response_event_fd_, &v, sizeof(v));
    }

    void periodicTasks() {
        static int tick = 0;
        if (++tick >= 100) { tick = 0; engine_.activeExpireCycle(); }
    }

    lstl::vector<CommandQueue*> queues_;
    StorageEngine engine_;
    bool aof_enabled_ = false;
    AofWriter aof_;
    PubSubManager pubsub_;
    std::string password_;
    Slowlog slowlog_;
    lstl::unordered_map<std::string, uint64_t> key_versions_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> total_processed_{0};
};

} // namespace ledis
