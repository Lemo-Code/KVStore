#!/usr/bin/env bash
# HTTP echo QPS 对比：Go stdlib vs nginx(OpenResty) + wrk
#
# 参数对标 TCP echo：THREADS=4 CONNECTIONS=64 DURATION=10s PAYLOAD=128
#
# 用法:
#   scripts/bench_http_wrk_compare.sh
#   THREADS=4 CONNECTIONS=64 DURATION=10s PAYLOAD=128 scripts/bench_http_wrk_compare.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GO_HTTP="go run -C ${ROOT}/tools/bench_http_go ."
WRK_LUA="${ROOT}/tools/bench_http_wrk/post_echo.lua"
NGINX_CONF="${ROOT}/bench_configs/nginx_echo_openresty.conf"
NGINX_STATIC_CONF="${ROOT}/bench_configs/nginx_echo_static.conf"
GO_PORT="${GO_PORT:-19002}"
NGINX_PORT="${NGINX_PORT:-19003}"
OPENRESTY_IMAGE="${OPENRESTY_IMAGE:-openresty/openresty:alpine}"

THREADS="${THREADS:-4}"
CONNECTIONS="${CONNECTIONS:-64}"
DURATION="${DURATION:-10s}"
PAYLOAD="${PAYLOAD:-128}"
RUNS="${RUNS:-1}"

parse_go_qps() {
  grep '^SUMMARY ' | tail -1 | sed -n 's/.*qps=\([0-9.]*\).*/\1/p'
}

parse_wrk_qps() {
  grep 'Requests/sec:' | tail -1 | awk '{print $2}' | sed 's/\..*//' 
}

run_wrk() {
  local url=$1
  PAYLOAD="$PAYLOAD" wrk -t"$THREADS" -c"$CONNECTIONS" -d"$DURATION" \
    -s "$WRK_LUA" "$url" 2>&1
}

bench_remote() {
  local name=$1 url=$2
  shift 2
  echo ""
  echo "========== $name =========="
  fuser -k "${GO_PORT}/tcp" "${NGINX_PORT}/tcp" 2>/dev/null || true
  sleep 0.3
  "$@" &
  local pid=$!
  sleep 0.8
  local out
  out=$(run_wrk "$url")
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
  echo "$out"
  parse_wrk_qps <<<"$out"
}

start_openresty() {
  docker rm -f lemo-bench-openresty 2>/dev/null || true
  docker run -d --name lemo-bench-openresty \
    --network host \
    -v "${NGINX_CONF}:/usr/local/openresty/nginx/conf/nginx.conf:ro" \
    "$OPENRESTY_IMAGE" 2>/dev/null
}

stop_openresty() {
  docker rm -f lemo-bench-openresty 2>/dev/null || true
}

stop_nginx_proc() {
  if [[ -f /tmp/nginx_echo_static.pid ]]; then
    kill "$(cat /tmp/nginx_echo_static.pid)" 2>/dev/null || true
    rm -f /tmp/nginx_echo_static.pid
  fi
  fuser -k "${NGINX_PORT}/tcp" 2>/dev/null || true
}

start_nginx_static() {
  if ! command -v nginx >/dev/null; then
    return 1
  fi
  stop_nginx_proc
  nginx -c "$NGINX_STATIC_CONF" 2>/dev/null
}

bench_nginx() {
  if command -v docker >/dev/null && docker info >/dev/null 2>&1; then
    stop_openresty
    if timeout 20 docker pull "$OPENRESTY_IMAGE" >/dev/null 2>&1; then
      start_openresty
      sleep 1
      if curl -sf "http://127.0.0.1:${NGINX_PORT}/ping" >/dev/null; then
        echo "========== nginx/openresty server (port $NGINX_PORT, POST echo) ==========" >&2
        run_wrk "http://127.0.0.1:${NGINX_PORT}"
        stop_openresty
        return 0
      fi
      stop_openresty
    fi
  fi
  if start_nginx_static && curl -sf "http://127.0.0.1:${NGINX_PORT}/ping" >/dev/null; then
    echo "========== nginx static server (port $NGINX_PORT, 128B fixed response) ==========" >&2
    run_wrk "http://127.0.0.1:${NGINX_PORT}"
    stop_nginx_proc
    return 0
  fi
  echo "nginx unavailable (install nginx or docker openresty)" >&2
  return 1
}

echo "=== HTTP echo QPS compare (wrk POST /echo) ==="
echo "host=$(uname -n) arch=$(uname -m) nproc=$(nproc) date=$(date -Iseconds)"
echo "threads=$THREADS connections=$CONNECTIONS duration=$DURATION payload=${PAYLOAD}B runs=$RUNS"

echo ""
echo "========== 场景 A: Go loopback（内置 client+server） =========="
GO_LOOP=0
for ((i = 1; i <= RUNS; ++i)); do
  q=$($GO_HTTP --threads "$THREADS" --connections "$CONNECTIONS" \
    --duration "$DURATION" --payload "$PAYLOAD" 2>&1 | parse_go_qps)
  if [[ -n "$q" ]]; then
    GO_LOOP=$(awk "BEGIN{print $GO_LOOP + $q}")
  fi
done
GO_LOOP=$(awk "BEGIN{printf \"%.0f\", $GO_LOOP / $RUNS}")
echo "go http loopback (last run):"
$GO_HTTP --threads "$THREADS" --connections "$CONNECTIONS" \
  --duration "$DURATION" --payload "$PAYLOAD" 2>&1 | tail -3
printf "  %-22s %12s req/s\n" "go http loopback" "$GO_LOOP"

echo ""
echo "========== 场景 B: 远端 server + wrk =========="
GO_SRV=$(bench_remote "go http server (port $GO_PORT)" \
  "http://127.0.0.1:${GO_PORT}" \
  $GO_HTTP --mode server --port "$GO_PORT" --payload "$PAYLOAD")

if NGX_OUT=$(bench_nginx); then
  NGX_SRV=$(parse_wrk_qps <<<"$NGX_OUT")
else
  NGX_SRV=0
fi

echo ""
echo "========== 汇总 =========="
printf "  %-22s %12s req/s  (loopback)\n" "go http" "$GO_LOOP"
printf "  %-22s %12s req/s  (server+wrk)\n" "go http server" "$GO_SRV"
if [[ "${NGX_SRV:-0}" != "0" ]]; then
  printf "  %-22s %12s req/s  (server+wrk)\n" "nginx/openresty" "$NGX_SRV"
  if [[ -n "$GO_SRV" && "$GO_SRV" != "0" ]]; then
    awk "BEGIN{printf \"  go/nginx ratio: %.2f x\\n\", $GO_SRV / $NGX_SRV}"
  fi
fi
