/**
 * @file    log_demo_mdc.cpp
 * @brief   MDC + 限流 + 格式化高级用法
 *
 * 演示:
 *   1. MDC 在格式化中的展开 (%X{key})
 *   2. RateLimiter 限制日志频率
 *   3. 多线程 MDC 隔离
 *   4. 日志变更回调 (热更新)
 */

#include "zero/log/log.h"
#include "zero/log/config.h"
#include <cstdio>
#include <thread>
#include <chrono>
#include <atomic>

using namespace zero;

// 模拟请求: 设置 MDC, 打印请求日志, 清理 MDC
void processRequest(int id, const std::string& user, const std::string& action) {
    // ==== MDC: Fiber-local 上下文 ====
    MDC::put("request_id", std::to_string(id));
    MDC::put("user", user);
    MDC::put("action", action);

    auto logger = LoggerMgr::GetInstance()->getLogger("zero.app.request");

    // 日志中的 %X{request_id} %X{user} %X{action} 会自动展开
    ZERO_LOG_INFO(logger) << "start";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ZERO_LOG_INFO(logger) << "done";

    MDC::clear();
}

// 限流示例
void rateLimitDemo() {
    auto logger = LoggerMgr::GetInstance()->getLogger("zero.app.ratelimit");

    // 设置限流器: 每秒最多 5 条, 突发 3 条
    logger->setRateLimiter(std::make_shared<RateLimiter>(5.0, 3));

    printf("=== Rate limit demo (5/sec, burst 3) ===\n");
    int emitted = 0;

    for (int i = 0; i < 50; ++i) {
        // 直接调用 log: 限流器会透明过滤
        auto event = std::make_shared<LogEvent>(
            logger->shared_from_this(), LogLevel::INFO,
            __FILE__, __LINE__, 0, 0, 0, "demo");
        event->getSS() << "rate-limited message #" << i;
        logger->log(LogLevel::INFO, event);
        ++emitted;
    }
    printf("  Attempted 50, rate-limited output\n");
}

int main() {
    // ==== 1. 快速配置: 仅控制台, MDC-aware 格式 ====
    auto root = LoggerMgr::GetInstance()->getRoot();
    root->setFormatter(
        "%d{%H:%M:%S} [%p] [%N] "
        "rid=%X{request_id} user=%X{user} act=%X{action} "
        "%m%n");
    root->clearAppenders();
    root->addAppender(std::make_shared<ConsoleLogAppender>(false));

    // ==== 2. 多请求 MDC 隔离 ====
    printf("=== MDC demo ===\n");
    processRequest(1001, "alice", "login");
    processRequest(1002, "bob",   "upload");
    processRequest(1003, "alice", "logout");

    // ==== 3. 多线程 MDC 隔离 ====
    printf("\n=== Multi-thread MDC ===\n");
    std::atomic<int> counter{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&counter]() {
            int id = counter.fetch_add(1);
            MDC::put("request_id", std::to_string(id));
            MDC::put("thread_id", std::to_string(
                std::hash<std::thread::id>{}(std::this_thread::get_id()) % 1000));
            auto logger = LoggerMgr::GetInstance()->getLogger("zero.thread");
            ZERO_LOG_INFO(logger) << "thread work";
            MDC::clear();
        });
    }
    for (auto& t : threads) t.join();

    // ==== 4. 限流演示 ====
    printf("\n");
    rateLimitDemo();

    printf("\n=== Done ===\n");
    return 0;
}
