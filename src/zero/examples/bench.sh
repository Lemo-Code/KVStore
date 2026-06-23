#!/bin/bash
# Zero vs Go 并发性能对比

set -e
cd "$(dirname "$0")/.."
BUILD_DIR="build"
ZERO_SERVER="$BUILD_DIR/examples/echo_minimal"
GO_SERVER="$BUILD_DIR/examples/echo_go"
BENCH_CLIENT="examples/bench_client.go"
PORT=18090
BENCH_PORT=18091

# Build
echo "=== Building ==="
/usr/bin/cmake --build "$BUILD_DIR" --target echo_minimal -j4 2>&1 | tail -1
go build -o "$GO_SERVER" examples/echo_go.go
echo "Build done"

bench_one() {
    local name="$1"
    local server_cmd="$2"
    local results=()

    echo ""
    echo "=== $name ==="

    for conn in 10 50 100 500; do
        local port=$((BENCH_PORT + conn))
        # Start server with specific port
        $server_cmd $port &
        local pid=$!
        sleep 1

        echo "--- concurrency=$conn ---"
        go run "$BENCH_CLIENT" ":$port" "$conn" 10 2>&1 | grep -E "(Time|Requests|Throughput|RPS|Latency)"

        kill $pid 2>/dev/null
        wait $pid 2>/dev/null
        sleep 0.5
    done
}

# Run Zero
bench_one "Zero (C++ 网络库)" "$ZERO_SERVER"

# Run Go
bench_one "Go (原生 net/http)" "$GO_SERVER"

echo ""
echo "=== Benchmark Complete ==="
