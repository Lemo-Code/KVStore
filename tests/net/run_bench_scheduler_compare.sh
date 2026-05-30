#!/usr/bin/env bash
# 对比 KVStore net::Scheduler 与 Sylar sylar::Scheduler 性能
set -euo pipefail
export PATH="/usr/bin:${PATH}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
KV_BUILD="${ROOT}/build"
SYLAR_DIR="${ROOT}/third_party/sylars"
SYLAR_BUILD="${SYLAR_DIR}/build"
ARGS="${*:-}"

echo "=== Build KVStore (Release) ==="
mkdir -p "${KV_BUILD}"
cmake -S "${ROOT}" -B "${KV_BUILD}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLSTL_BUILD_NET_LOG=ON \
  -DLSTL_BUILD_BENCH=OFF >/dev/null
cmake --build "${KV_BUILD}" --target bench_scheduler -j"$(nproc)"

echo ""
echo "=== Build Sylar benchmark_scheduler ==="
if [[ ! -d "${SYLAR_BUILD}" ]]; then
  mkdir -p "${SYLAR_BUILD}"
  cmake -S "${SYLAR_DIR}" -B "${SYLAR_BUILD}" >/dev/null
fi
cmake --build "${SYLAR_BUILD}" --target benchmark_scheduler -j"$(nproc)" 2>/dev/null || {
  echo "WARN: Sylar build failed, only running KVStore benchmark"
  "${ROOT}/bin/net/bench_scheduler" ${ARGS}
  exit 0
}

KV_BIN="${ROOT}/bin/net/bench_scheduler"
KV_TEST="${ROOT}/bin/net/test_scheduler"
SYLAR_BIN="${SYLAR_DIR}/bin/benchmark_scheduler"

echo ""
echo ">>> 正确性: test_scheduler"
"${KV_TEST}"

echo ""
echo "======== KVStore net::Scheduler ========"
"${KV_BIN}" ${ARGS}

echo ""
echo "======== Sylar sylar::Scheduler ========"
"${SYLAR_BIN}" ${ARGS}
