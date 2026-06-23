/**
 * stress_ledis_engine.cpp — Ledis 引擎矩阵压测 (无网络, 进程内)
 *
 * 纵轴: ops/线程 [1K, 10K, 50K, 100K]
 * 横轴: threads [1, 2, 4, 6, 8]
 *
 * Usage: stress_ledis_engine [output_csv_prefix]
 */
#include "bench_utils.h"
#include "matrix.h"
using namespace stress;

#include "ledis/core/storage_engine.h"
#include "ledis/core/command.h"
#include "ledis/protocol/resp_parser.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace ledis;

// ============================================================
// 构建 RESP 请求
// ============================================================
static std::string buildSet(int id) {
    char k[32], v[32];
    snprintf(k, sizeof(k), "k%d", id);
    snprintf(v, sizeof(v), "v%d", id);
    char buf[128];
    int n = snprintf(buf, sizeof(buf),
        "*3\r\n$3\r\nSET\r\n$%zu\r\n%s\r\n$%zu\r\n%s\r\n",
        strlen(k), k, strlen(v), v);
    return std::string(buf, n);
}

static std::string buildGet(int id) {
    char k[32];
    snprintf(k, sizeof(k), "k%d", id);
    char buf[64];
    int n = snprintf(buf, sizeof(buf),
        "*2\r\n$3\r\nGET\r\n$%zu\r\n%s\r\n", strlen(k), k);
    return std::string(buf, n);
}

// ============================================================
// 单个命令往返 (RESP 解析 → dispatch → 响应序列化)
// ============================================================
static uint64_t runCmdPath(StorageEngine& eng, const std::string& req,
                           int repeat) {
    CmdContext ctx;
    std::string response;
    ctx.response = &response;
    ctx.engine = &eng;

    uint64_t count = 0;
    for (int i = 0; i < repeat; ++i) {
        RespParser parser;
        size_t consumed = 0;
        if (parser.feed(req.data(), req.size(), consumed) != RespParser::Result::OK)
            continue;

        // 将 string_view 拷贝到持久存储 (避免悬空)
        static thread_local std::vector<std::string> storage;
        storage.clear();
        ctx.args.clear();
        for (auto& a : parser.args())
            storage.emplace_back(a.data(), a.size());
        for (auto& s : storage)
            ctx.args.push_back(s);

        response.clear();
        dispatchCommand(ctx);
        count++;
    }
    return count;
}

// ============================================================
// main — 矩阵压测
// ============================================================
int main(int argc, char** argv) {
    std::string prefix = (argc > 1) ? argv[1] : "benchInfo/ledis_engine";

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Ledis Engine Matrix  (RESP→引擎→响应, 不含网络)      ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    // 纵轴: 每个线程的 ops 数量
    struct OpRow { const char* label; int ops_per_thread; };
    OpRow op_rows[] = {
        {"1K ops/线程",   1000},
        {"10K ops/线程",  10000},
        {"50K ops/线程",  50000},
        {"100K ops/线程", 100000},
    };

    const char* cmd_names[] = {"SET", "GET"};

    for (auto* cmd : cmd_names) {
        std::string bench_name = std::string("ledis_engine_") + cmd;

        // 预热
        {
            StorageEngine warmup;
            std::string req = (cmd[0] == 'S') ? buildSet(0) : buildGet(0);
            for (int i = 0; i < 1000; ++i) runCmdPath(warmup, req, 1);
        }

        for (auto& orow : op_rows) {
            MatrixRunner runner(bench_name + "_" + orow.label);

            // --- SET/GET 基准 (每线程独立引擎, 测纯单线程吞吐叠加) ---
            runner.addRow(cmd, [&](int threads, uint64_t& ops, double& sec) {
                std::string req = (cmd[0] == 'S') ? buildSet(0) : buildGet(0);
                stress::runMultiThread(threads, [&](int tid, std::atomic<uint64_t>& counter) {
                    StorageEngine eng;
                    uint64_t local = runCmdPath(eng, req, orow.ops_per_thread);
                    counter.fetch_add(local, std::memory_order_relaxed);
                }, ops, sec);
            });

            // --- Pipeline 风格 (16 条命令一批, 每线程独立引擎) ---
            runner.addRow(std::string(cmd) + "+P16", [&](int threads, uint64_t& ops, double& sec) {
                std::string batch;
                for (int i = 0; i < 16; ++i) {
                    std::string one = (cmd[0] == 'S') ? buildSet(i) : buildGet(i);
                    batch += one;
                }
                stress::runMultiThread(threads, [&](int tid, std::atomic<uint64_t>& counter) {
                    StorageEngine eng;
                    RespParser parser;
                    size_t consumed = 0;
                    uint64_t local = 0;
                    int rounds = orow.ops_per_thread / 16;

                    for (int r = 0; r < rounds; ++r) {
                        parser.reset();
                        if (parser.feed(batch.data(), batch.size(), consumed) != RespParser::Result::OK)
                            continue;
                        for (int c = 0; c < 16; ++c) {
                            CmdContext ctx2;
                            std::string resp2;
                            ctx2.response = &resp2;
                            ctx2.engine = &eng;
                            static thread_local std::vector<std::string> st;
                            st.clear();
                            for (auto& a : parser.args())
                                st.emplace_back(a.data(), a.size());
                            ctx2.args.clear();
                            for (auto& s : st) ctx2.args.push_back(s);
                            dispatchCommand(ctx2);
                            local++;
                        }
                    }
                    counter.fetch_add(local, std::memory_order_relaxed);
                }, ops, sec);
            });

            runner.run();
            runner.printMatrix();

            std::string csv = prefix + "_" + cmd + "_" +
                std::to_string(orow.ops_per_thread) + ".csv";
            std::string md  = prefix + "_" + cmd + "_" +
                std::to_string(orow.ops_per_thread) + ".md";
            runner.saveCsv(csv);
            runner.saveMd(md);
        }
    }

    return 0;
}
