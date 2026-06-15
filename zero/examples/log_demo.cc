/**
 * @file    log_demo.cc
 * @brief   日志库完整使用示例 — 配置、层级、MDC、限流、格式化
 *
 * 编译: g++ -std=c++14 -I.. log_demo.cc -L../build -lzero -lyaml-cpp -lpthread
 * 运行: ./a.out [config.yaml]
 */

#include "zero/log/log.h"
#include "zero/log/config.h"
#include <cstdio>
#include <thread>
#include <vector>

using namespace zero;

// 模拟网络请求处理
void handleRequest(int req_id, const std::string& path) {
    // 设置 MDC (fiber-local)
    MDC::put("request_id", std::to_string(req_id));
    MDC::put("path", path);

    auto logger = LoggerMgr::GetInstance()->getLogger("zero.net.http");
    ZERO_LOG_INFO(logger) << "request start path=" << path;

    // 模拟处理
    if (path.find("admin") != std::string::npos) {
        ZERO_LOG_WARN(logger) << "admin access attempt";
    }

    ZERO_LOG_DEBUG(logger) << "request done status=200";

    MDC::clear();
}

int main(int argc, char** argv) {
    // ============================================================
    // 1. 从 YAML 文件加载配置
    // ============================================================
    const char* config_file = (argc > 1) ? argv[1] : "examples/log_config.yaml";
    printf("=== Loading config: %s ===\n", config_file);
    LogConfig::LoadFromYamlFile(config_file);

    // ============================================================
    // 2. 获取 Logger (层级自动创建)
    // ============================================================
    auto root  = LoggerMgr::GetInstance()->getRoot();
    auto net   = LoggerMgr::GetInstance()->getLogger("zero.net");
    auto http  = LoggerMgr::GetInstance()->getLogger("zero.net.http");
    auto sched = LoggerMgr::GetInstance()->getLogger("zero.scheduler");

    printf("Logger tree:\n");
    printf("  root          level=%s\n", LogLevel::ToString(root->getLevel()));
    printf("  zero.net      level=%s parent=%s\n",
           LogLevel::ToString(net->getLevel()),
           net->getParent() ? net->getParent()->getName().c_str() : "none");
    printf("  zero.net.http level=%s parent=%s\n",
           LogLevel::ToString(http->getLevel()),
           http->getParent()->getName().c_str());
    printf("  zero.scheduler level=%s\n", LogLevel::ToString(sched->getLevel()));

    // ============================================================
    // 3. 各级别日志
    // ============================================================
    printf("\n=== Level demonstration ===\n");
    ZERO_LOG_TRACE(root) << "this is TRACE (may be filtered)";
    ZERO_LOG_DEBUG(root) << "this is DEBUG";
    ZERO_LOG_INFO(root)  << "this is INFO";
    ZERO_LOG_WARN(root)  << "this is WARN";
    ZERO_LOG_ERROR(root) << "this is ERROR";
    // ZERO_LOG_FATAL(root) << "this is FATAL (uncomment to test)";

    // ============================================================
    // 4. 格式化示例 (自定义 pattern)
    // ============================================================
    printf("\n=== Custom formatter ===\n");
    auto custom = LoggerMgr::GetInstance()->getLogger("zero.custom");
    custom->setFormatter("%d{%H:%M:%S} [%p] [%N] [%f:%l] %m%n");
    ZERO_LOG_INFO(custom) << "custom format message";

    // ============================================================
    // 5. MDC 示例 (在 pattern 中使用 %X{key})
    // ============================================================
    printf("\n=== MDC demonstration ===\n");
    auto mdc_logger = LoggerMgr::GetInstance()->getLogger("zero.mdc");
    mdc_logger->setFormatter("%d [%p] req=%X{request_id} path=%X{path} %m%n");

    MDC::put("request_id", "req-001");
    MDC::put("path", "/api/users");
    ZERO_LOG_INFO(mdc_logger) << "processing";
    MDC::clear();

    // ============================================================
    // 6. 模拟多请求 (MDC 隔离)
    // ============================================================
    printf("\n=== Multi-request simulation ===\n");
    handleRequest(1001, "/api/users");
    handleRequest(1002, "/admin/config");
    handleRequest(1003, "/api/status");

    // ============================================================
    // 7. 程序化配置 (不使用 YAML)
    // ============================================================
    printf("\n=== Programmatic setup ===\n");
    LogConfig::QuickSetup(true, "/tmp/zero_demo.log", LogLevel::DEBUG);
    ZERO_LOG_INFO(root) << "quick setup applied";

    printf("\n=== Done ===\n");
    return 0;
}
