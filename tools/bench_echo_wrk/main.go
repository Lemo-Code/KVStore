// bench_echo_wrk: wrk 风格的 TCP echo 压测客户端
//
//	用法（对标 wrk -t4 -c256 -d10s）:
//	  go run ./tools/bench_echo_wrk -t 4 -c 256 -d 10s --host 127.0.0.1 --port 19000
//	  go run ./tools/bench_echo_wrk -t 4 -c 256 -d 10s --host 127.0.0.1 --port 19000 --latency
package main

import (
	"flag"
	"fmt"
	"io"
	"math"
	"net"
	"os"
	"sort"
	"sync"
	"sync/atomic"
	"time"
)

type config struct {
	host     string
	port     int
	threads  int
	conns    int
	duration time.Duration
	payload  int
	latency  bool
}

func parseConfig() config {
	cfg := config{
		host:     "127.0.0.1",
		port:     19000,
		threads:  4,
		conns:    256,
		duration: 10 * time.Second,
		payload:  128,
	}
	flag.StringVar(&cfg.host, "host", cfg.host, "target host")
	flag.IntVar(&cfg.port, "port", cfg.port, "target port")
	flag.IntVar(&cfg.threads, "t", cfg.threads, "number of threads (wrk -t)")
	flag.IntVar(&cfg.conns, "c", cfg.conns, "connections to keep open (wrk -c)")
	dur := flag.Duration("d", cfg.duration, "duration of test (wrk -d)")
	flag.IntVar(&cfg.payload, "payload", cfg.payload, "bytes per roundtrip")
	flag.BoolVar(&cfg.latency, "latency", false, "print latency distribution (wrk --latency)")
	flag.Parse()
	if dur != nil {
		cfg.duration = *dur
	}
	if cfg.threads < 1 {
		cfg.threads = 1
	}
	if cfg.conns < 1 {
		cfg.conns = 1
	}
	if cfg.payload < 1 {
		cfg.payload = 1
	}
	return cfg
}

type stats struct {
	requests atomic.Uint64
}

func worker(addr string, payload, recv []byte, deadline time.Time,
	latBuf *[]float64, latMu *sync.Mutex, reqCount *stats) {
	conn, err := net.Dial("tcp4", addr)
	if err != nil {
		return
	}
	defer conn.Close()

	for time.Now().Before(deadline) {
		start := time.Now()
		if _, err := conn.Write(payload); err != nil {
			break
		}
		if _, err := io.ReadFull(conn, recv); err != nil {
			break
		}
		lat := time.Since(start)
		reqCount.requests.Add(1)
		if latBuf != nil {
			latMu.Lock()
			*latBuf = append(*latBuf, float64(lat.Microseconds()))
			latMu.Unlock()
		}
	}
}

func percentile(sorted []float64, p float64) float64 {
	if len(sorted) == 0 {
		return 0
	}
	idx := int(math.Ceil(p/100.0*float64(len(sorted)))) - 1
	if idx < 0 {
		idx = 0
	}
	if idx >= len(sorted) {
		idx = len(sorted) - 1
	}
	return sorted[idx]
}

func printLatency(sorted []float64) {
	if len(sorted) == 0 {
		return
	}
	fmt.Println("  Latency Distribution")
	for _, p := range []float64{50, 75, 90, 99, 99.9} {
		fmt.Printf("    %5.1f%%  %.2fms\n", p, percentile(sorted, p)/1000.0)
	}
}

func run(cfg config) {
	addr := fmt.Sprintf("%s:%d", cfg.host, cfg.port)
	payload := make([]byte, cfg.payload)
	for i := range payload {
		payload[i] = 'W'
	}
	recv := make([]byte, cfg.payload)

	fmt.Printf("Running %s test @ tcp://%s\n", cfg.duration, addr)
	fmt.Printf("  %d threads and %d connections\n", cfg.threads, cfg.conns)
	fmt.Printf("  payload=%dB\n", cfg.payload)

	var reqCount stats
	var latBuf []float64
	var latMu sync.Mutex
	collectLat := cfg.latency
	if collectLat {
		latBuf = make([]float64, 0, 1<<20)
	}

	start := time.Now()
	deadline := start.Add(cfg.duration)

	var wg sync.WaitGroup
	workers := cfg.threads
	if workers > cfg.conns {
		workers = cfg.conns
	}
	base := cfg.conns / workers
	extra := cfg.conns % workers
	for w := 0; w < workers; w++ {
		n := base
		if w < extra {
			n++
		}
		if n == 0 {
			continue
		}
		wg.Add(1)
		go func(count int) {
			defer wg.Done()
			var inner sync.WaitGroup
			inner.Add(count)
			for i := 0; i < count; i++ {
				go func() {
					defer inner.Done()
					var lb *[]float64
					if collectLat {
						lb = &latBuf
					}
					worker(addr, payload, recv, deadline, lb, &latMu, &reqCount)
				}()
			}
			inner.Wait()
		}(n)
	}
	wg.Wait()
	elapsed := time.Since(start)

	reqs := reqCount.requests.Load()
	qps := float64(reqs) / elapsed.Seconds()
	mib := float64(reqs*uint64(cfg.payload)*2) / 1024.0 / 1024.0 / elapsed.Seconds()

	fmt.Printf("  %d requests in %.2fs\n", reqs, elapsed.Seconds())
	fmt.Printf("Requests/sec: %12.2f\n", qps)
	fmt.Printf("Transfer/sec: %12.2fMiB\n", mib)

	if cfg.latency && len(latBuf) > 0 {
		sort.Float64s(latBuf)
		printLatency(latBuf)
	}

	// 机器可读摘要行，供脚本解析
	fmt.Printf("SUMMARY qps=%.0f requests=%d duration=%.2fs payload=%dB threads=%d connections=%d\n",
		qps, reqs, elapsed.Seconds(), cfg.payload, cfg.threads, cfg.conns)
}

func main() {
	cfg := parseConfig()
	run(cfg)
	if cfg.port == 0 {
		os.Exit(1)
	}
}
