#!/usr/bin/env bash
# 固定参数复测 echo 基线，输出可对比的一行结果 + 环境信息。
# 用法: scripts/bench_baseline.sh [label]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LABEL="${1:-current}"
BIN="${ROOT}/bin/lemo/socket/bench_echo_server"
OUT="${ROOT}/bench_results/echo_baseline.tsv"
RUNS="${RUNS:-5}"

if [[ ! -x "$BIN" ]]; then
  echo "missing $BIN — build: cmake --build build-lemo-test --target bench_echo_server"
  exit 1
fi

mkdir -p "${ROOT}/bench_results"

# 标准场景（与截图一致）
THREADS=4
CONNECTIONS=64
MESSAGES=1000
PAYLOAD=128

{
  echo "# date=$(date -Iseconds) label=$LABEL host=$(uname -n) arch=$(uname -m) nproc=$(nproc)"
  echo "# threads=$THREADS connections=$CONNECTIONS messages=$MESSAGES payload=${PAYLOAD}B"
} >>"$OUT"

parse_throughput() {
  grep -o 'throughput=[0-9.]*' | head -1 | cut -d= -f2
}

for mode in normal fixed128; do
  extra=()
  tag="$mode"
  if [[ "$mode" == "fixed128" ]]; then
    extra=(--fixed128)
    tag="fixed128"
  fi
  sum=0
  count=0
  for ((i = 1; i <= RUNS; ++i)); do
  line=$("$BIN" --mode local --threads "$THREADS" --connections "$CONNECTIONS" \
    --messages "$MESSAGES" --payload "$PAYLOAD" "${extra[@]}" 2>&1 | tail -1)
    qps=$(echo "$line" | parse_throughput)
    if [[ -n "$qps" ]]; then
      echo "  run$i $tag qps=$qps  $line"
      sum=$(awk "BEGIN{print $sum + $qps}")
      count=$((count + 1))
    else
      echo "  run$i $tag FAILED"
    fi
  done
  if [[ "$count" -gt 0 ]]; then
    avg=$(awk "BEGIN{printf \"%.0f\", $sum / $count}")
    echo "$(date -Iseconds)	$LABEL	$tag	$avg	$count	runs" >>"$OUT"
    echo "[$tag] avg=${avg} req/s (${count} runs)"
  fi
done

echo "results appended to $OUT"
