#!/usr/bin/env bash
# 使用 ThreadSanitizer 对多线程日志压测做死锁 / 数据竞争检测。
# 须以 -DNET_LOG_BENCH_TSAN=ON 配置并完整重编 net_log、bench_log_mt。
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${ROOT}/build"
BIN="${ROOT}/bin/net/bench_log_mt"

if [[ ! -x "${BIN}" ]]; then
  echo "error: ${BIN} not found. Build with TSan:" >&2
  echo "  cd build && cmake .. -DNET_LOG_BENCH_TSAN=ON && make bench_log_mt" >&2
  exit 1
fi

echo "=== ThreadSanitizer check on bench_log_mt (quick workload) ==="
export NET_BENCH_QUICK=1
"${BIN}" --quick "$@"
echo "=== ThreadSanitizer: no deadlock / race reported ==="
