#!/usr/bin/env bash
# 调度器：正确性 + 性能压测 +（可选）Sylar 对比
# 用法:
#   tests/net/run_scheduler_perf.sh
#   tests/net/run_scheduler_perf.sh --quick
#   tests/net/run_scheduler_perf.sh --tasks 1100 --threads 4
set -euo pipefail
export PATH="/usr/bin:${PATH}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
KV_BUILD="${ROOT}/build"
KV_BIN="${ROOT}/bin/net"
SYLAR_DIR="${ROOT}/third_party/sylars"
SYLAR_BIN="${SYLAR_DIR}/bin/benchmark_scheduler"
ARGS=("$@")
if [[ ${#ARGS[@]} -eq 0 ]]; then
  ARGS=(--quick)
fi

echo "=============================================="
echo " KVStore Scheduler — 正确性 + 性能"
echo " $(date '+%Y-%m-%d %H:%M:%S')  Release build"
echo "=============================================="

echo ""
echo ">>> [1/4] Release 构建"
mkdir -p "${KV_BUILD}"
cmake -S "${ROOT}" -B "${KV_BUILD}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLSTL_BUILD_NET_LOG=ON \
  -DLSTL_BUILD_BENCH=OFF >/dev/null
cmake --build "${KV_BUILD}" \
  --target net_log bench_scheduler test_scheduler -j"$(nproc)" 2>&1 | tail -3

echo ""
echo ">>> [2/4] 单元测试 test_scheduler"
"${KV_BIN}/test_scheduler"

echo ""
echo ">>> [3/4] 正确性冒烟 (1100 任务 × 5 次)"
cat > /tmp/kv_sched_smoke.cc << 'EOF'
#include "fiber/module.h"
#include <atomic>
#include <cstdio>
int main() {
  const int N = 1100;
  for (int run = 0; run < 5; ++run) {
    net::Scheduler sch(4, false, "smoke");
    sch.start();
    std::atomic<int> done{0};
    for (int i = 0; i < N; ++i)
      sch.schedule([&done]() { done.fetch_add(1, std::memory_order_relaxed); });
    for (int i = 0; i < 200000000; ++i) {
      if (done.load(std::memory_order_acquire) >= N) break;
    }
    if (done.load() != N) {
      std::printf("FAIL run=%d done=%d\n", run + 1, done.load());
      sch.stop();
      return 1;
    }
    sch.stop();
  }
  std::printf("PASS 5x1100\n");
  return 0;
}
EOF
g++ -std=c++11 -O2 -I"${ROOT}/module/net" /tmp/kv_sched_smoke.cc \
  -L"${KV_BUILD}/module/net" -lnet_log -lyaml-cpp -lpthread -o /tmp/kv_sched_smoke
/tmp/kv_sched_smoke

echo ""
echo ">>> [4/4] 性能压测 bench_scheduler ${ARGS[*]}"
echo "--- KVStore net::Scheduler ---"
"${KV_BIN}/bench_scheduler" "${ARGS[@]}"

if [[ -x "${SYLAR_BIN}" ]]; then
  echo ""
  echo "--- Sylar sylar::Scheduler ---"
  export LD_LIBRARY_PATH="${SYLAR_DIR}/lib:${LD_LIBRARY_PATH:-}"
  "${SYLAR_BIN}" "${ARGS[@]}"
elif [[ -d "${SYLAR_DIR}" ]]; then
  echo ""
  echo ">>> Sylar 未构建，尝试编译..."
  SYLAR_BUILD="${SYLAR_DIR}/build"
  mkdir -p "${SYLAR_BUILD}"
  if cmake -S "${SYLAR_DIR}" -B "${SYLAR_BUILD}" >/dev/null 2>&1 && \
     cmake --build "${SYLAR_BUILD}" --target benchmark_scheduler -j"$(nproc)" 2>/dev/null; then
    echo "--- Sylar sylar::Scheduler ---"
    export LD_LIBRARY_PATH="${SYLAR_DIR}/lib:${LD_LIBRARY_PATH:-}"
    "${SYLAR_BIN}" "${ARGS[@]}"
  else
    echo "WARN: Sylar 构建失败，跳过对比"
  fi
else
  echo ""
  echo "WARN: 未找到 ${SYLAR_DIR}，仅输出 KVStore 结果"
fi

echo ""
echo "=============================================="
echo " 完成"
echo "=============================================="
