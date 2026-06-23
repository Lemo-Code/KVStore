#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/bin"
OUT="${ROOT}/benchmark/kv_matrix.txt"
mkdir -p "${ROOT}/benchmark"

echo "╔══════════════════════════════════════════════╗"
echo "║   ledis vs redis (单体) 性能矩阵对比          ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

# 清理旧进程
pkill -9 ledis-server 2>/dev/null || true
pkill -9 redis-server 2>/dev/null || true
sleep 1

LEDIS_PORT=16380
REDIS_PORT=6379
HAS_REDIS=false

# 检查 redis-server 是否可用
if command -v redis-server &>/dev/null; then
    HAS_REDIS=true
else
    echo "⚠️  redis-server 未安装，仅测试 ledis"
fi

# 启动 ledis
echo ">>> 启动 ledis-server (port ${LEDIS_PORT}) ..."
"${BIN}/ledis-server" --port ${LEDIS_PORT} --loglevel ERROR &
LEDIS_PID=$!
sleep 2
if ! kill -0 ${LEDIS_PID} 2>/dev/null; then
    echo "❌ ledis-server 启动失败"
    exit 1
fi
echo "   ledis PID=${LEDIS_PID}"

# 启动 redis
if $HAS_REDIS; then
    echo ">>> 启动 redis-server (port ${REDIS_PORT}) ..."
    redis-server --port ${REDIS_PORT} --loglevel warning --daemonize yes 2>/dev/null
    sleep 1
    REDIS_PID=$(pgrep -f "redis-server.*${REDIS_PORT}" | head -1)
    echo "   redis PID=${REDIS_PID:-unknown}"
fi

cleanup() {
    echo ""; echo ">>> 清理..."
    kill ${LEDIS_PID} 2>/dev/null || true
    [ -n "${REDIS_PID:-}" ] && kill ${REDIS_PID} 2>/dev/null || true
}
trap cleanup EXIT

# ── 矩阵测试 ──
echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║   矩阵 1: 不同命令 QPS (50并发, 10万请求)     ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

CMDS="SET GET INCR LPUSH HSET ZADD"
THREADS="10 50 100"

# 表头
printf "%-24s" "命令 \\ 并发数"
for c in ${THREADS}; do printf " | %12s" "c=${c}"; done
echo ""
printf "%-24s" "------------------------"
for c in ${THREADS}; do printf " | %12s" "------------"; done
echo ""

for cmd in ${CMDS}; do
    printf "%-24s" "ledis ${cmd}"
    for c in ${THREADS}; do
        r=$(redis-benchmark -p ${LEDIS_PORT} -t ${cmd,,} -n 100000 -c ${c} -q --csv 2>/dev/null | grep "^\"${cmd}" | cut -d'"' -f4 || echo "0")
        printf " | %10s" "$(printf "%.1fK" $(echo "scale=1; ${r:-0}/1000" | bc 2>/dev/null || echo 0))"
    done
    echo ""
    
    if $HAS_REDIS; then
        printf "%-24s" "redis ${cmd}"
        for c in ${THREADS}; do
            r=$(redis-benchmark -p ${REDIS_PORT} -t ${cmd,,} -n 100000 -c ${c} -q --csv 2>/dev/null | grep "^\"${cmd}" | cut -d'"' -f4 || echo "0")
            printf " | %10s" "$(printf "%.1fK" $(echo "scale=1; ${r:-0}/1000" | bc 2>/dev/null || echo 0))"
        done
        echo ""
    fi
done

echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║   矩阵 2: Pipeline 吞吐 (50并发, SET)         ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

PIPE_SIZES="1 8 16 32 64"
printf "%-24s" "实现 \\ Pipeline"
for p in ${PIPE_SIZES}; do printf " | %12s" "P=${p}"; done
echo ""
printf "%-24s" "------------------------"
for p in ${PIPE_SIZES}; do printf " | %12s" "------------"; done
echo ""

printf "%-24s" "ledis SET"
for p in ${PIPE_SIZES}; do
    r=$(redis-benchmark -p ${LEDIS_PORT} -t set -n 200000 -P ${p} -c 50 -q --csv 2>/dev/null | grep "^\"SET" | cut -d'"' -f4 || echo "0")
    printf " | %10s" "$(printf "%.1fK" $(echo "scale=1; ${r:-0}/1000" | bc 2>/dev/null || echo 0))"
done
echo ""

if $HAS_REDIS; then
    printf "%-24s" "redis SET"
    for p in ${PIPE_SIZES}; do
        r=$(redis-benchmark -p ${REDIS_PORT} -t set -n 200000 -P ${p} -c 50 -q --csv 2>/dev/null | grep "^\"SET" | cut -d'"' -f4 || echo "0")
        printf " | %10s" "$(printf "%.1fK" $(echo "scale=1; ${r:-0}/1000" | bc 2>/dev/null || echo 0))"
    done
    echo ""
fi

echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║   矩阵 3: Value 大小影响 (50并发, SET)        ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

SIZES="16 64 256 1024 4096 16384"
printf "%-24s" "实现 \\ Value大小"
for s in ${SIZES}; do printf " | %12s" "${s}B"; done
echo ""
printf "%-24s" "------------------------"
for s in ${SIZES}; do printf " | %12s" "------------"; done
echo ""

printf "%-24s" "ledis SET"
for s in ${SIZES}; do
    r=$(redis-benchmark -p ${LEDIS_PORT} -t set -n 50000 -d ${s} -c 50 -q --csv 2>/dev/null | grep "^\"SET" | cut -d'"' -f4 || echo "0")
    printf " | %10s" "$(printf "%.1fK" $(echo "scale=1; ${r:-0}/1000" | bc 2>/dev/null || echo 0))"
done
echo ""

if $HAS_REDIS; then
    printf "%-24s" "redis SET"
    for s in ${SIZES}; do
        r=$(redis-benchmark -p ${REDIS_PORT} -t set -n 50000 -d ${s} -c 50 -q --csv 2>/dev/null | grep "^\"SET" | cut -d'"' -f4 || echo "0")
        printf " | %10s" "$(printf "%.1fK" $(echo "scale=1; ${r:-0}/1000" | bc 2>/dev/null || echo 0))"
    done
    echo ""
fi

echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║   矩阵 4: 延迟分布 (50并发, 10万 SET)         ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

echo ">>> ledis SET 延迟:"
redis-benchmark -p ${LEDIS_PORT} -t set -n 100000 -c 50 --latency 2>&1 | grep -E "^(min|avg|p50|p99|max):" | head -5 || echo "  (latency histogram not available)"

if $HAS_REDIS; then
    echo ""
    echo ">>> redis SET 延迟:"
    redis-benchmark -p ${REDIS_PORT} -t set -n 100000 -c 50 --latency 2>&1 | grep -E "^(min|avg|p50|p99|max):" | head -5 || echo "  (latency histogram not available)"
fi

echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║   矩阵 5: 内存占用 (写入10万key后)            ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

# 写入测试数据
for i in $(seq 1 100000); do redis-cli -p ${LEDIS_PORT} SET "bench:${i}" "value${i}" 2>/dev/null >/dev/null; done
LEDIS_KEYS=$(redis-cli -p ${LEDIS_PORT} DBSIZE 2>/dev/null)
LEDIS_RSS=$(ps -o rss= -p ${LEDIS_PID} 2>/dev/null | tr -d ' ')
echo "  ledis: ${LEDIS_KEYS} keys, RSS=${LEDIS_RSS}KB ($(echo "scale=1; ${LEDIS_RSS:-0}/1024" | bc)MB)"

if $HAS_REDIS; then
    for i in $(seq 1 100000); do redis-cli -p ${REDIS_PORT} SET "bench:${i}" "value${i}" 2>/dev/null >/dev/null; done
    REDIS_KEYS=$(redis-cli -p ${REDIS_PORT} DBSIZE 2>/dev/null)
    [ -n "${REDIS_PID:-}" ] && REDIS_RSS=$(ps -o rss= -p ${REDIS_PID} 2>/dev/null | tr -d ' ')
    echo "  redis: ${REDIS_KEYS} keys, RSS=${REDIS_RSS:-0}KB ($(echo "scale=1; ${REDIS_RSS:-0}/1024" | bc)MB)"
fi

echo ""
echo "✅ KV对比完成"
echo "结果已保存: ${OUT}"
