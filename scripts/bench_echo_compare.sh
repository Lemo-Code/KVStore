#!/usr/bin/env bash
# lemo echo 压测：server/client 分离，便于与 Go 对比。
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/bin/lemo/socket/bench_echo_server"
PORT="${PORT:-19000}"
THREADS="${THREADS:-4}"
CONNECTIONS="${CONNECTIONS:-256}"
MESSAGES="${MESSAGES:-10000}"
PAYLOAD="${PAYLOAD:-128}"
FIXED128="${FIXED128:-1}"

if [[ ! -x "$BIN" ]]; then
  echo "missing $BIN — build: cmake --build build-lemo-test --target bench_echo_server"
  exit 1
fi

EXTRA=()
if [[ "$FIXED128" == "1" ]]; then
  EXTRA+=(--fixed128)
fi

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]]; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

echo "=== lemo echo bench ==="
echo "threads=$THREADS connections=$CONNECTIONS messages=$MESSAGES payload=${PAYLOAD}B fixed128=$FIXED128"

"$BIN" --mode server --threads "$THREADS" --port "$PORT" --payload "$PAYLOAD" "${EXTRA[@]}" &
SERVER_PID=$!
sleep 0.3

"$BIN" --mode client --host 127.0.0.1 --port "$PORT" \
  --threads "$THREADS" --connections "$CONNECTIONS" \
  --messages "$MESSAGES" --payload "$PAYLOAD" "${EXTRA[@]}"

kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true
SERVER_PID=
