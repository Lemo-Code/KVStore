#!/usr/bin/env bash
# ============================================================
# KVStore 一键环境配置 + 编译 + 压测脚本
#
# 用法:
#   bash shell/setup.sh              # 完整: 安装依赖 + 编译 + 测试
#   bash shell/setup.sh --build-only # 仅编译
#   bash shell/setup.sh --bench-only # 仅压测 (跳过编译)
#   bash shell/setup.sh --clean      # 清理所有构建产物
#
# 首次 clone 项目后直接运行:
#   git clone <repo> && cd KVStore && bash shell/setup.sh
# ============================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"

# ── 颜色 ──
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; BOLD='\033[1m';   NC='\033[0m'
info()  { echo -e "${BLUE}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[ OK ]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
fail()  { echo -e "${RED}[FAIL]${NC}  $*"; }
step()  { echo -e "\n${BOLD}═══════════════════════════════════════════════${NC}"; echo -e "${BOLD}  $*${NC}"; echo -e "${BOLD}═══════════════════════════════════════════════${NC}"; }

# ── 参数解析 ──
MODE="full"
CLEAN=false
for arg in "$@"; do
    case "$arg" in
        --build-only) MODE="build" ;;
        --bench-only) MODE="bench" ;;
        --clean) CLEAN=true ;;
        --help|-h) 
            echo "Usage: bash shell/setup.sh [--build-only|--bench-only|--clean]"
            exit 0 ;;
    esac
done

# ── 系统检测 ──
NCORES=$(nproc 2>/dev/null || echo 4)
ARCH=$(uname -m)
OS=$(uname -s)
START_TIME=$(date +%s)

echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║   KVStore — 一键环境配置 + 编译 + 压测        ║"
echo "╠══════════════════════════════════════════════╣"
echo "║   架构: ${ARCH}  核心: ${NCORES}  系统: ${OS}             ║"
echo "║   模式: ${MODE}                                 ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

# ═══════════════════════════════════════════════
# Phase 1: 环境检查 + 依赖安装
# ═══════════════════════════════════════════════
if [ "$MODE" != "bench" ]; then
step "Phase 1/5: 环境检查与依赖"

# 1.1 编译器
if command -v g++ &>/dev/null; then
    GCC_VER=$(g++ -dumpversion | cut -d. -f1)
    ok "g++ ${GCC_VER}.x 已安装"
else
    fail "g++ 未安装"; info "请运行: sudo apt install g++"
    exit 1
fi

# 1.2 cmake
if command -v cmake &>/dev/null; then
    CMAKE_VER=$(cmake --version | head -1 | awk '{print $3}')
    ok "cmake ${CMAKE_VER} 已安装"
else
    fail "cmake 未安装"; info "请运行: sudo apt install cmake"
    exit 1
fi

# 1.3 必需的库
MISSING=""
for lib in yaml-cpp pthread; do
    if ldconfig -p 2>/dev/null | grep -q "$lib"; then
        info "  ✓ $lib"
    else
        warn "  ✗ $lib (可能需要安装)"
        MISSING="$MISSING $lib"
    fi
done

# 1.4 可选依赖
info "可选依赖检查:"
for dep in "libluajit-5.1-dev:luajit" "liburing-dev:io_uring" "libevent-dev:libevent" "redis-server:redis"; do
    pkg="${dep%%:*}"
    name="${dep##*:}"
    if dpkg -l "$pkg" 2>/dev/null | grep -q "^ii"; then
        info "  ✓ ${name} ($pkg)"
    else
        warn "  ✗ ${name} (可选, sudo apt install $pkg)"
    fi
done

# 1.5 检查 yaml-cpp (必需的)
if ! ldconfig -p 2>/dev/null | grep -q yaml-cpp; then
    echo ""
    warn "yaml-cpp 未找到，尝试安装..."
    if command -v apt-get &>/dev/null; then
        info "运行: sudo apt-get install -y libyaml-cpp-dev"
        if sudo apt-get install -y libyaml-cpp-dev 2>/dev/null; then
            ok "yaml-cpp 安装成功"
        else
            fail "yaml-cpp 安装失败，请手动安装"
        fi
    fi
fi

fi # end phase 1

# ═══════════════════════════════════════════════
# Phase 2: 清理 (可选)
# ═══════════════════════════════════════════════
if $CLEAN; then
    step "清理构建产物"
    rm -rf "${ROOT}/build" "${ROOT}/build-stress" "${ROOT}/build-res-zero" \
           "${ROOT}/build-check" "${ROOT}/bin"
    ok "清理完成"
    exit 0
fi

# ═══════════════════════════════════════════════
# Phase 3: 编译 zero + lstl + ledis
# ═══════════════════════════════════════════════
if [ "$MODE" != "bench" ]; then
step "Phase 2/5: 编译 zero 网络库"

ZERO_DIR="${ROOT}/src/zero"
ZERO_BUILD="${ROOT}/build-res-zero"

if [ -f "${ZERO_BUILD}/libzero.a" ]; then
    ok "zero 库已编译，跳过"
else
    mkdir -p "${ZERO_BUILD}"
    cd "${ZERO_BUILD}"
    cmake "${ZERO_DIR}" -DCMAKE_BUILD_TYPE=Release -DZERO_BUILD_EXAMPLES=OFF 2>&1 | tail -3
    make -j${NCORES} 2>&1 | tail -5
    ok "zero 库编译完成"
fi

step "Phase 3/5: 编译 KVStore (ledis + 压测工具)"

STRESS_BUILD="${ROOT}/build-stress"
BIN_DIR="${ROOT}/bin"
mkdir -p "${BIN_DIR}"

cd "${ROOT}"
cmake -B "${STRESS_BUILD}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_STRESS_TESTS=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_EXAMPLES=ON \
    2>&1 | tail -5

cmake --build "${STRESS_BUILD}" -j${NCORES} 2>&1 | tail -10

# 复制二进制
for bin in ledis-server stress_log_matrix stress_lstl_bench \
           stress_net_compare stress_ledis_redis_compare \
           stress_ledis_concurrent echo_minimal bench_echo; do
    src=$(find "${STRESS_BUILD}" -name "$bin" -type f 2>/dev/null | head -1)
    if [ -n "$src" ] && [ -x "$src" ]; then
        cp "$src" "${BIN_DIR}/" 2>/dev/null && info "  ✓ $bin"
    else
        warn "  ✗ $bin (未找到)"
    fi
done

ok "编译完成"

fi # end build

# ═══════════════════════════════════════════════
# Phase 4: 功能验证
# ═══════════════════════════════════════════════
if [ "$MODE" != "bench" ]; then
step "Phase 4/5: 快速功能验证"

BIN_DIR="${ROOT}/bin"

# 4.1 ledis 启动测试
info "测试 ledis-server 启动..."
if timeout 3 "${BIN_DIR}/ledis-server" --port 19999 --loglevel ERROR 2>/dev/null &
then
    sleep 1
    if redis-cli -p 19999 PING 2>/dev/null | grep -q PONG; then
        ok "ledis-server PING 正常"
    else
        warn "ledis-server PING 异常"
    fi
    kill %1 2>/dev/null || true
else
    warn "ledis-server 启动超时，可能需要先编译"
fi

# 4.2 压测工具可用性
info "检查压测工具..."
BINS_OK=0; BINS_TOTAL=0
for bin in stress_log_matrix stress_lstl_bench stress_net_compare \
           stress_ledis_redis_compare stress_ledis_concurrent; do
    BINS_TOTAL=$((BINS_TOTAL + 1))
    if [ -x "${BIN_DIR}/${bin}" ]; then
        BINS_OK=$((BINS_OK + 1))
    else
        warn "  ✗ ${bin} 未找到"
    fi
done
ok "压测工具: ${BINS_OK}/${BINS_TOTAL} 可用"

fi # end verify

# ═══════════════════════════════════════════════
# Phase 5: 运行全部基准测试
# ═══════════════════════════════════════════════
if [ "$MODE" != "build" ]; then
step "Phase 5/5: 运行基准测试矩阵"

OUT_DIR="${ROOT}/benchmark"
mkdir -p "${OUT_DIR}"
SUMMARY="${OUT_DIR}/SETUP_SUMMARY.txt"

{
    echo "################################################################"
    echo "# KVStore 一键测试报告"
    echo "# 生成时间: $(date -Iseconds)"
    echo "# 架构: ${ARCH}  核心: ${NCORES}  系统: ${OS}"
    echo "################################################################"
    echo ""
} > "${SUMMARY}"

BENCH_DIR="${ROOT}/shell"
TOTAL=5; PASSED=0

run_bench() {
    local name="$1" script="$2"
    echo ""; info ">>> [${name}] 运行中..."
    if [ -x "${BENCH_DIR}/${script}" ]; then
        if timeout 120 bash "${BENCH_DIR}/${script}" 2>&1 | tail -15 | tee -a "${SUMMARY}"; then
            ok "${name} 完成"; PASSED=$((PASSED + 1))
        else
            warn "${name} 部分失败或超时"
        fi
    else
        warn "${name} 脚本不存在: ${BENCH_DIR}/${script}"
    fi
}

# 5.1 日志对比
run_bench "1/5 zero_log vs spdlog" "bench_log.sh"

# 5.2 内存池对比
run_bench "2/5 内存池 vs malloc" "bench_pool.sh"

# 5.3 容器对比
run_bench "3/5 zstl vs STL" "bench_zstl.sh"

# 5.4 网络对比 (时间较长)
info ">>> [4/5] 网络对比 (可能需60秒)..."
timeout 120 bash "${BENCH_DIR}/bench_net.sh" 2>&1 | tail -20 | tee -a "${SUMMARY}" && PASSED=$((PASSED + 1)) || warn "4/5 网络对比超时"

# 5.5 KV对比 (时间较长)
info ">>> [5/5] KV对比 (可能需60秒)..."
timeout 120 bash "${BENCH_DIR}/bench_kv.sh" 2>&1 | tail -40 | tee -a "${SUMMARY}" && PASSED=$((PASSED + 1)) || warn "5/5 KV对比超时"

# ── 最终报告 ──
ELAPSED=$(($(date +%s) - START_TIME))

echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║            一 键 测 试 完 成                  ║"
echo "╠══════════════════════════════════════════════╣"
echo "║  完成: ${PASSED}/${TOTAL}  耗时: ${ELAPSED}s                    ║"
echo "║  报告: benchmark/SETUP_SUMMARY.txt            ║"
echo "║  数据: benchmark/*.txt                        ║"
echo "║  二进制: bin/                                 ║"
echo "╚══════════════════════════════════════════════╝"
echo ""
echo "  快速命令:"
echo "    启动服务:  bin/ledis-server --port 6379"
echo "    集群模式:  bin/ledis-server --port 7000 --cluster-enabled"
echo "    运行测试:  for s in shell/bench_*.sh; do bash \$s; done"
echo ""


fi # end bench

exit 0
