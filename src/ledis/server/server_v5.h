#pragma once

// ============================================================
// v5: 多核并行 — 共享 Dict + SpinLock
// ============================================================
// 架构:
//   - N 个 worker 线程, 每个独立 epoll 事件循环
//   - SO_REUSEPORT 内核级连接分发
//   - 一个共享 Dict, 用 SpinLock 保护 (~200ns 临界区)
//   - 读命令在锁内执行, 写命令在锁内执行 + AOF
//
// 为什么比 fiber(v1-v4) 快:
//   - 多线程 I/O 并行 (recv/send 可以同时在多核执行)
//   - SpinLock 等待 ~200ns (用户态自旋), 对比 mutex 阻塞 ~5-10μs
//   - 无 fiber 调度开销
//

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <lstl/container/vector.h>
#include <lstl/container/unordered_map.h>

#include "zero/thread/mutex.h"
#include "zero/log/log.h"

#include "ledis/server/shard_worker.h"
#include "ledis/core/command.h"
#include "ledis/replication/aof_writer.h"

namespace ledis {

class LedisServerV5 {
public:
    using ptr = std::shared_ptr<LedisServerV5>;

    struct Config {
        std::string bind_addr   = "0.0.0.0";
        int         port         = 6379;
        int         io_threads   = 4;
        std::string io_engine    = "epoll"; // "epoll" 或 "uring"
        std::string requirepass;
        std::string aof_path;
        AofWriter::FsyncMode aof_mode = AofWriter::EVERYSEC;

        // 集群配置 (v5 多线程下 cluster 支持有限，推荐使用 v2)
        bool        cluster_enabled  = false;
        int         cluster_port     = 0;
        std::string cluster_seeds;
        int         cluster_replicas = 0;
    };

    explicit LedisServerV5(Config cfg) : cfg_(std::move(cfg)) {
        g_logger_ = ZERO_LOG_ROOT();
        if (cfg_.io_threads < 1) cfg_.io_threads = 1;
    }
    ~LedisServerV5() { stop(); }

    bool start() {
        initCommandTable();
        running_ = true;

        // 启动 AOF (共享)
        if (!cfg_.aof_path.empty()) {
            aof_.setPath(cfg_.aof_path);
            aof_.setMode(cfg_.aof_mode);
            aof_.start();
        }

        // 创建 worker 线程 (共享 engine_ + engine_lock_ + key_versions_)
        for (int i = 0; i < cfg_.io_threads; ++i) {
            auto* w = new ShardWorker(i, cfg_.port, cfg_.bind_addr,
                cfg_.requirepass,
                &engine_, &engine_lock_, &key_versions_,
                cfg_.aof_path.empty() ? nullptr : &aof_,
                &aof_lock_);
            if (!w->start()) {
                ZERO_LOG_ERROR(g_logger_) << "Worker " << i << " failed";
                return false;
            }
            workers_.push_back(w);
        }

        ZERO_LOG_INFO(g_logger_)
            << "Ledis v5 listening on " << cfg_.bind_addr << ":" << cfg_.port
            << " (" << cfg_.io_threads << " workers, shared Dict + SpinLock)";
        return true;
    }

    void stop() {
        if (!running_.exchange(false)) return;
        for (auto* w : workers_) { w->stop(); delete w; }
        workers_.clear();
        aof_.stop();
        ZERO_LOG_INFO(g_logger_) << "Ledis v5 stopped.";
    }

    const Config& config() const { return cfg_; }
    void clusterTick() {}  // v5 cluster support: TODO

private:
    Config cfg_;
    lstl::vector<ShardWorker*> workers_;

    // 共享状态
    StorageEngine engine_;
    zero::SpinLock engine_lock_;
    lstl::unordered_map<std::string, uint64_t> key_versions_;
    AofWriter aof_;
    zero::Mutex aof_lock_;

    std::atomic<bool> running_{false};
    zero::Logger::ptr g_logger_;
};

} // namespace ledis
