#!/usr/bin/env bash
# 使用 Valgrind Helgrind 对多线程日志压测做死锁 / 锁序检测。
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${ROOT}/bin/net/bench_log_mt"

if [[ ! -x "${BIN}" ]]; then
  echo "error: ${BIN} not found. Build first:" >&2
  echo "  cd build && make bench_log_mt" >&2
  exit 1
fi

if ! command -v valgrind >/dev/null 2>&1; then
  echo "error: valgrind not installed (sudo apt install valgrind)" >&2
  exit 1
fi

echo "=== Helgrind deadlock check on bench_log_mt (quick workload) ==="
export NET_BENCH_QUICK=1

valgrind --error-exitcode=1 \
  --tool=helgrind \
  --history-level=full \
  --conflict-cache-size=1000000 \
  "${BIN}" --quick "$@"

echo "=== Helgrind: no deadlock reported ==="
