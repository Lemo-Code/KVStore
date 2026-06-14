#include "db_worker.h"
#include "sylar/log.h"
#include "sylar/util.h"
#include <chrono>

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("db");

namespace sylar {
namespace db {

DBWorker* DBWorker::GetInstance() {
    static DBWorker instance;
    return &instance;
}

DBWorker::DBWorker() : next_heartbeat_id_(1), stop_(false) {
    thread_ = std::thread(&DBWorker::loop, this);
    SYLAR_LOG_DEBUG(g_logger) << "DBWorker started";
}

DBWorker::~DBWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_one();
    if (thread_.joinable()) thread_.join();
    SYLAR_LOG_DEBUG(g_logger) << "DBWorker stopped";
}

void DBWorker::addTask(std::function<void()> task) {
    if (!task) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

uint64_t DBWorker::addHeartbeat(std::function<void()> cb, uint64_t interval_ms) {
    if (!cb || interval_ms == 0) return 0;
    uint64_t id;
    uint64_t now = sylar::GetCurrentMS();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        id = next_heartbeat_id_++;
        heartbeats_[id] = { std::move(cb), interval_ms, now + interval_ms };
    }
    cv_.notify_one();
    return id;
}

void DBWorker::removeHeartbeat(uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    heartbeats_.erase(id);
}

void DBWorker::loop() {
    while (true) {
        std::function<void()> task;
        uint64_t sleep_ms = 1000;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                return stop_ || !tasks_.empty();
            });
            if (stop_) break;
            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            } else {
                uint64_t now = sylar::GetCurrentMS();
                for (auto& kv : heartbeats_) {
                    if (kv.second.next_run_ms <= now) {
                        task = kv.second.cb;
                        kv.second.next_run_ms = now + kv.second.interval_ms;
                        break;
                    }
                }
                if (!task) {
                    uint64_t next = now + 1000;
                    for (const auto& kv : heartbeats_) {
                        if (kv.second.next_run_ms < next) next = kv.second.next_run_ms;
                    }
                    sleep_ms = (next > now) ? (next - now) : 1;
                    cv_.wait_for(lock, std::chrono::milliseconds(sleep_ms));
                }
            }
        }
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                SYLAR_LOG_ERROR(g_logger) << "DBWorker task exception: " << e.what();
            } catch (...) {
                SYLAR_LOG_ERROR(g_logger) << "DBWorker task unknown exception";
            }
        }
    }
}

} // namespace db
} // namespace sylar
