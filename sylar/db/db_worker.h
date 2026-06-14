#ifndef __SYLAR_DB_DB_WORKER_H__
#define __SYLAR_DB_DB_WORKER_H__

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <unordered_map>

namespace sylar {
namespace db {

/**
 * DB 专用后台工人：单线程执行建连任务与心跳，不依赖 IOManager/协程。
 * 保证用户在任何线程、任意时刻调用 getPool/getConn 都不会因调度器上下文引发错误。
 */
class DBWorker {
public:
    static DBWorker* GetInstance();

    /** 投递建连等任务，由后台线程执行 */
    void addTask(std::function<void()> task);

    /** 注册定时心跳，返回 id；interval_ms 为间隔（毫秒） */
    uint64_t addHeartbeat(std::function<void()> cb, uint64_t interval_ms);

    /** 取消心跳 */
    void removeHeartbeat(uint64_t id);

private:
    DBWorker();
    ~DBWorker();
    DBWorker(const DBWorker&) = delete;
    DBWorker& operator=(const DBWorker&) = delete;

    void loop();

    struct HeartbeatEntry {
        std::function<void()> cb;
        uint64_t interval_ms;
        uint64_t next_run_ms;
    };

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    std::unordered_map<uint64_t, HeartbeatEntry> heartbeats_;
    uint64_t next_heartbeat_id_;
    bool stop_;
    std::thread thread_;
};

} // namespace db
} // namespace sylar

#endif
