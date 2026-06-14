#!/usr/bin/env bash
# wrk 风格 echo QPS 对比：lemo vs Go
#
# 对标 wrk: -t <threads> -c <connections> -d <duration>
# 场景 A（loopback）：各自进程内 server+client，公平对比全栈吞吐
# 场景 B（server）：统一 wrk 客户端压远端 server（Go 可用；lemo server 对外部客户端有限制）
#
# 用法:
#   scripts/bench_echo_wrk_compare.sh
#   THREADS=4 CONNECTIONS=128 DURATION=5s PAYLOAD=128 scripts/bench_echo_wrk_compare.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LEMO_BIN="${ROOT}/bin/lemo/socket/bench_echo_server"
GO_BENCH="go run -C ${ROOT}/tools/bench_echo_go ."
WRK_CLIENT="go run -C ${ROOT}/tools/bench_echo_wrk ."

THREADS="${THREADS:-4}"
CONNECTIONS="${CONNECTIONS:-128}"
DURATION="${DURATION:-5s}"
PAYLOAD="${PAYLOAD:-128}"
LEMO_PORT="${LEMO_PORT:-19000}"
GO_PORT="${GO_PORT:-19001}"
LATENCY="${LATENCY:-0}"
RUNS="${RUNS:-1}"

if [[ ! -x "$LEMO_BIN" ]]; then
  echo "missing $LEMO_BIN"
  echo "build: cmake --build build-lemo-test --target bench_echo_server"
  exit 1
fi

parse_qps() {
  grep '^SUMMARY ' | tail -1 | sed -n 's/.*qps=\([0-9.]*\).*/\1/p'
}

run_lemo_loopback() {
  "$LEMO_BIN" --mode local --threads "$THREADS" --connections "$CONNECTIONS" \
    --duration "$DURATION" --payload "$PAYLOAD" 2>&1
}

run_go_loopback() {
  $GO_BENCH --threads "$THREADS" --connections "$CONNECTIONS" \
    --duration "$DURATION" --payload "$PAYLOAD" 2>&1
}

run_wrk_client() {
  local host=$1 port=$2
  local extra=()
  if [[ "$LATENCY" == "1" ]]; then
    extra+=(--latency)
  fi
  $WRK_CLIENT -t "$THREADS" -c "$CONNECTIONS" -d "$DURATION" \
    --host "$host" --port "$port" --payload "$PAYLOAD" "${extra[@]}" 2>&1
}

bench_remote_server() {
  local name=$1 port=$2
  shift 2
  echo ""
  echo "========== $name (port $port) =========="
  fuser -k "${port}/tcp" 2>/dev/null || true
  sleep 0.2
  "$@" &
  local pid=$!
  sleep 0.5
  local out
  out=$(run_wrk_client 127.0.0.1 "$port")
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
  echo "$out"
  echo "$out" | parse_qps
}

echo "=== wrk-style echo QPS compare ==="
echo "host=$(uname -n) arch=$(uname -m) nproc=$(nproc) date=$(date -Iseconds)"
echo "threads=$THREADS connections=$CONNECTIONS duration=$DURATION payload=${PAYLOAD}B runs=$RUNS"

echo ""
echo "========== 场景 A: loopback（全栈，wrk 参数） =========="
LEMO_LOOP=0
GO_LOOP=0
for ((i = 1; i <= RUNS; ++i)); do
  set +e
  q=$(run_lemo_loopback | parse_qps)
  set -e
  if [[ -n "$q" ]]; then
    LEMO_LOOP=$(awk "BEGIN{print $LEMO_LOOP + $q}")
  fi
  q=$(run_go_loopback | parse_qps)
  if [[ -n "$q" ]]; then
    GO_LOOP=$(awk "BEGIN{print $GO_LOOP + $q}")
  fi
done
LEMO_LOOP=$(awk "BEGIN{printf \"%.0f\", $LEMO_LOOP / $RUNS}")
GO_LOOP=$(awk "BEGIN{printf \"%.0f\", $GO_LOOP / $RUNS}")

echo "lemo loopback (last run):"
run_lemo_loopback | tail -3
echo "go loopback (last run):"
run_go_loopback | tail -3
echo ""
printf "  %-22s %12s req/s\n" "lemo loopback" "$LEMO_LOOP"
printf "  %-22s %12s req/s\n" "go loopback" "$GO_LOOP"
if [[ -n "$LEMO_LOOP" && -n "$GO_LOOP" && "$GO_LOOP" != "0" ]]; then
  awk "BEGIN{printf \"  lemo/go ratio: %.2f x\\n\", $LEMO_LOOP / $GO_LOOP}"
fi

echo ""
echo "========== 场景 B: 远端 server + wrk 客户端 =========="
LEMO_SRV=$(bench_remote_server "lemo echo_server" "$LEMO_PORT" \
  "$LEMO_BIN" --mode server --threads "$THREADS" --port "$LEMO_PORT" --payload "$PAYLOAD")
GO_SRV=$(bench_remote_server "go echo_server" "$GO_PORT" \
  $GO_BENCH --mode server --threads "$THREADS" --port "$GO_PORT" --payload "$PAYLOAD")

echo ""
echo "========== 汇总 =========="
printf "  %-22s %12s req/s  (loopback)\n" "lemo" "$LEMO_LOOP"
printf "  %-22s %12s req/s  (loopback)\n" "go" "$GO_LOOP"
printf "  %-22s %12s req/s  (server+wrk)\n" "lemo server" "$LEMO_SRV"
printf "  %-22s %12s req/s  (server+wrk)\n" "go server" "$GO_SRV"
