#!/usr/bin/env bash
# net 模块：fiber/scheduler/timer/io/hook/socket/buffer 正确性测试
# 用法:
#   tests/net/run_net_io_tests.sh
#   tests/net/run_net_io_tests.sh --with-bench   # 附加 bench_scheduler --quick
set -euo pipefail
export PATH="/usr/bin:${PATH}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
KV_BUILD="${ROOT}/build"
KV_BIN="${ROOT}/bin/net"
WITH_BENCH=0
if [[ "${1:-}" == "--with-bench" ]]; then
  WITH_BENCH=1
fi

# 按依赖顺序：fiber → scheduler/timer → io/hook → socket/buffer
NET_CORE_TESTS=(
  test_fiber
  test_scheduler
  test_timer
  test_timer_wheel
  test_iomanager
  test_hook
  test_address
  test_ring_buffer
  test_byte_array
  test_thread
)

echo "=============================================="
echo " KVStore net — IO/Hook 及后续模块测试"
echo " $(date '+%Y-%m-%d %H:%M:%S')  Release"
echo "=============================================="

echo ""
echo ">>> [1/2] Release 构建"
mkdir -p "${KV_BUILD}"
cmake -S "${ROOT}" -B "${KV_BUILD}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLSTL_BUILD_NET_LOG=ON \
  -DLSTL_BUILD_BENCH=OFF >/dev/null

TARGETS=(net_log)
for t in "${NET_CORE_TESTS[@]}"; do
  TARGETS+=("${t}")
done
if [[ "${WITH_BENCH}" -eq 1 ]]; then
  TARGETS+=(bench_scheduler)
fi
cmake --build "${KV_BUILD}" --target "${TARGETS[@]}" -j"$(nproc)" 2>&1 | tail -5

echo ""
echo ">>> [2/2] 运行测试"
PASS=0
FAIL=0
FAILED=()
for t in "${NET_CORE_TESTS[@]}"; do
  printf "%-24s " "${t}"
  if "${KV_BIN}/${t}" >/tmp/"${t}".out 2>&1; then
    echo "PASS"
    PASS=$((PASS + 1))
  else
    echo "FAIL"
    cat /tmp/"${t}".out
    FAIL=$((FAIL + 1))
    FAILED+=("${t}")
  fi
done

echo ""
echo "----------------------------------------------"
echo " 通过: ${PASS}/${#NET_CORE_TESTS[@]}  失败: ${FAIL}"
if [[ ${FAIL} -gt 0 ]]; then
  echo " 失败项: ${FAILED[*]}"
  exit 1
fi

if [[ "${WITH_BENCH}" -eq 1 ]]; then
  echo ""
  echo ">>> 附加压测 bench_scheduler --quick"
  "${KV_BIN}/bench_scheduler" --quick
fi

echo ""
echo "=============================================="
echo " 全部通过"
echo "=============================================="
