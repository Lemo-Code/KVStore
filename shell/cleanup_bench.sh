#!/usr/bin/env bash
# 清理 KVStore 压测残留 + 可选关闭非必要开发服务/重复 IDE
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FULL="${1:-}"

echo "[cleanup] 终止压测相关进程 ..."

pkill -f 'redis-benchmark.*1638' 2>/dev/null || true
pkill -f 'ledis-server --port' 2>/dev/null || true
pkill -f "${ROOT}/bin/ledis-server" 2>/dev/null || true
pkill -f 'echo_minimal' 2>/dev/null || true
pkill -f "${ROOT}/bin/http_server" 2>/dev/null || true
pkill -f 'stress_libevent_http' 2>/dev/null || true
pkill -f 'stress_lstl_bench|stress_log_matrix|stress_net_compare|stress_ledis' 2>/dev/null || true

if command -v nginx >/dev/null 2>&1; then
    nginx -s stop -c "${ROOT}/tests/stress/net/nginx_http_wrk.conf" 2>/dev/null || true
    nginx -s stop -c "${ROOT}/tests/stress/net/nginx_stream_echo.conf" 2>/dev/null || true
fi

rm -f /tmp/kvstore_nginx_http.pid /tmp/kvstore_nginx_echo.pid 2>/dev/null || true

if [[ "${FULL}" == "--all" || "${FULL}" == "-a" ]]; then
    echo "[cleanup] 关闭 VS Code Server（保留 Cursor）..."
    pkill -f '\.vscode-server' 2>/dev/null || true
    pkill -f 'code-fcf604774b' 2>/dev/null || true

    echo "[cleanup] 尝试停止 mysql / mongod / etcd / iperf3 / redis-server ..."
    if sudo -n systemctl stop mysql mongod etcd iperf3 redis-server 2>/dev/null; then
        echo "[cleanup] 系统服务已停止"
    else
        echo "[cleanup] 需要 sudo 密码，请手动执行:"
        echo "  sudo systemctl stop mysql mongod etcd iperf3 redis-server"
    fi
fi

sleep 0.3

REMAIN=$(pgrep -af 'ledis-server|echo_minimal|http_server|stress_libevent|stress_lstl|stress_log|stress_net|stress_ledis|redis-benchmark.*1638' 2>/dev/null || true)
if [[ -n "${REMAIN}" ]]; then
    echo "[cleanup] 仍有压测残留:"
    echo "${REMAIN}"
    exit 1
fi

echo "[cleanup] 完成"
if [[ "${FULL}" == "--all" || "${FULL}" == "-a" ]]; then
    echo "[cleanup] 提示: Cursor 进程已保留；mysql/mongod/etcd/iperf3 需 sudo 才能停止"
else
    echo "[cleanup] 压测端口已释放。全面清理: bash shell/cleanup_bench.sh --all"
fi
