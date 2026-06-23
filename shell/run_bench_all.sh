#!/usr/bin/env bash
# KVStore 性能对比一键脚本
#
# 用法:
#   bash shell/run_bench_all.sh [输出目录]
#
# 默认输出: benchInfo/perf_compare/
#   lstl_compare.log       — LSTL vs STL (单位: ops/s)
#   log_compare.log        — zeroLog vs spdlog 异步 (单位: lines/s)
#   net_compare.log        — zero vs libevent vs nginx (单位: req/s)
#   ledis_redis_compare.log — Ledis vs Redis (单位: req/s)
#   SUMMARY.log            — 汇总
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"

CMAKE="${CMAKE:-/usr/bin/cmake}"
BUILD_DIR="${ROOT}/build-stress"
BIN="${ROOT}/bin"
OUT="${1:-benchInfo/perf_compare}"
NCORES="$(nproc)"
LEDIS_PORT="${LEDIS_PORT:-16379}"
REDIS_PORT="${REDIS_PORT:-6379}"

mkdir -p "${OUT}" "${BIN}"

export KVSTORE_ROOT="${ROOT}"
export KVSTORE_BIN="${BIN}"

log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "${OUT}/run.log"; }

# ---- 构建 ----
log "构建压测二进制 (CPU=${NCORES}核) ..."
"${CMAKE}" -S "${ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DBUILD_STRESS_TESTS=ON \
    -DBUILD_EXAMPLES=ON \
    -DBUILD_BENCHMARKS=OFF 2>&1 | tail -5

"${CMAKE}" --build "${BUILD_DIR}" --target \
    stress_lstl_bench stress_log_matrix stress_net_compare stress_ledis_redis_compare \
    echo_minimal ledis-server -j"$(nproc)" 2>&1 | tail -8

# ---- 清理函数 ----
PID_LEDIS=""

cleanup() {
    [[ -n "${PID_LEDIS}" ]] && kill "${PID_LEDIS}" 2>/dev/null || true
}
trap cleanup EXIT

# ---- 1. LSTL vs STL ----
log ">>> [1/4] LSTL vs STL 矩阵 (线程 1→$((NCORES*2)), 单位 ops/s)"
"${BIN}/stress_lstl_bench" "${OUT}" 2>&1 | tee -a "${OUT}/run.log"

# ---- 2. zeroLog vs spdlog ----
log ">>> [2/4] zeroLog vs spdlog 异步 QPS (单位 lines/s)"
"${BIN}/stress_log_matrix" "${OUT}" 2>&1 | tee -a "${OUT}/run.log"

# ---- 3. 网络库对比 (TCP echo 客户端) ----
log ">>> [3/4] 网络 TCP Echo QPS (zero / libevent / nginx, 内置客户端, 单位 req/s)"
"${BIN}/stress_net_compare" "${OUT}" 2>&1 | tee -a "${OUT}/run.log"

# ---- 4. Ledis vs Redis ----
log ">>> [4/4] Ledis vs Redis 高并发 (线程 ${NCORES}→$((NCORES*2)), c=50, 单位 req/s)"

"${BIN}/ledis-server" --port "${LEDIS_PORT}" --loglevel OFF &
PID_LEDIS=$!
sleep 2

if redis-cli -p "${REDIS_PORT}" ping >/dev/null 2>&1; then
    "${BIN}/stress_ledis_redis_compare" "${LEDIS_PORT}" "${REDIS_PORT}" "${OUT}" \
        2>&1 | tee -a "${OUT}/run.log"
else
    log "WARN: Redis :${REDIS_PORT} 未运行, 跳过 Ledis vs Redis 对比"
    {
        echo "================================================================"
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] Ledis vs Redis — 跳过"
        echo "原因: redis-cli -p ${REDIS_PORT} ping 失败"
        echo "请先启动 Redis 后重新运行: bash shell/run_bench_all.sh"
        echo "================================================================"
    } > "${OUT}/ledis_redis_compare.log"
fi

kill "${PID_LEDIS}" 2>/dev/null || true
PID_LEDIS=""

# ---- 汇总 ----
SUMMARY="${OUT}/SUMMARY.log"
{
    echo "################################################################"
    echo "# KVStore 性能对比汇总"
    echo "# 生成时间: $(date -Iseconds)"
    echo "# CPU 核心: ${NCORES} | 线程轴(通用): 1,2,4,...,$((NCORES*2))"
    echo "# 输出目录: ${ROOT}/${OUT}"
    echo "################################################################"
    echo ""
    for f in lstl_compare log_compare net_compare ledis_redis_compare; do
        fp="${OUT}/${f}.log"
        if [[ -f "${fp}" ]]; then
            echo ""
            cat "${fp}"
        fi
    done
    echo ""
    echo "################################################################"
    echo "# 单位说明"
    echo "#   lstl_compare.log        — ops/s (QPS, 容器/内存操作)"
    echo "#   log_compare.log         — lines/s (QPS, 日志行)"
    echo "#   net_compare.log         — req/s (QPS, TCP echo 往返)"
    echo "#   ledis_redis_compare.log — req/s (QPS, RESP 命令往返)"
    echo "################################################################"
} > "${SUMMARY}"

log "完成! 结果目录: ${ROOT}/${OUT}/"
log "  汇总: ${ROOT}/${SUMMARY}"
ls -la "${OUT}/"*.log 2>/dev/null || true
echo ""
echo "输出位置: ${ROOT}/${OUT}/"
