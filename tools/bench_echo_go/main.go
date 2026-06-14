// bench_echo_go: loopback echo QPS 压测（Go 标准库 net）
//
//	go run ./tools/bench_echo_go --quick
//	go run ./tools/bench_echo_go --threads 4 --connections 256 --messages 2000 --payload 128
//	go run ./tools/bench_echo_go --sweep
package main

import (
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"sync"
	"sync/atomic"
	"time"
)

type config struct {
	threads     int
	connections int
	messages    int
	payload     int
	quick       bool
	sweep       bool
}

func parseConfig() config {
	cfg := config{
		threads:     4,
		connections: 64,
		messages:    1000,
		payload:     128,
	}
	flag.IntVar(&cfg.threads, "threads", cfg.threads, "client worker goroutines")
	flag.IntVar(&cfg.connections, "connections", cfg.connections, "concurrent connections")
	flag.IntVar(&cfg.messages, "messages", cfg.messages, "roundtrips per connection")
	flag.IntVar(&cfg.payload, "payload", cfg.payload, "bytes per roundtrip")
	flag.BoolVar(&cfg.quick, "quick", false, "quick smoke test")
	flag.BoolVar(&cfg.sweep, "sweep", false, "sweep connection counts")
	flag.Parse()
	if cfg.quick {
		cfg.threads = 2
		cfg.connections = 16
		cfg.messages = 200
		cfg.payload = 64
	}
	if cfg.threads < 1 {
		cfg.threads = 1
	}
	if cfg.connections < 1 {
		cfg.connections = 1
	}
	if cfg.messages < 1 {
		cfg.messages = 1
	}
	if cfg.payload < 1 {
		cfg.payload = 1
	}
	return cfg
}

type result struct {
	connections int
	roundtrips  uint64
	payload     int
	wallMs      float64
	qps         float64
	mibps       float64
	usPerReq    float64
}

func echoSession(conn net.Conn, payload []byte) {
	buf := make([]byte, len(payload))
	for {
		if _, err := io.ReadFull(conn, buf); err != nil {
			return
		}
		if _, err := conn.Write(buf); err != nil {
			return
		}
	}
}

func runOnce(cfg config) result {
	ln, err := net.Listen("tcp4", "127.0.0.1:0")
	if err != nil {
		fmt.Fprintf(os.Stderr, "listen failed: %v\n", err)
		os.Exit(1)
	}
	addr := ln.Addr().String()
	stop := make(chan struct{})
	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		for {
			conn, err := ln.Accept()
			if err != nil {
				select {
				case <-stop:
					return
				default:
					continue
				}
			}
			payload := make([]byte, cfg.payload)
			for i := range payload {
				payload[i] = 'G'
			}
			go echoSession(conn, payload)
		}
	}()

	start := time.Now()
	connCh := make(chan int, cfg.connections)
	for i := 0; i < cfg.connections; i++ {
		connCh <- i
	}
	close(connCh)

	var done atomic.Uint64
	var clientWg sync.WaitGroup
	workers := cfg.threads
	if workers > cfg.connections {
		workers = cfg.connections
	}
	for w := 0; w < workers; w++ {
		clientWg.Add(1)
		go func() {
			defer clientWg.Done()
			payload := make([]byte, cfg.payload)
			for i := range payload {
				payload[i] = 'G'
			}
			recv := make([]byte, cfg.payload)
			for range connCh {
				conn, err := net.Dial("tcp4", addr)
				if err != nil {
					continue
				}
				for m := 0; m < cfg.messages; m++ {
					if _, err := conn.Write(payload); err != nil {
						break
					}
					if _, err := io.ReadFull(conn, recv); err != nil {
						break
					}
					done.Add(1)
				}
				conn.Close()
			}
		}()
	}
	clientWg.Wait()
	wall := time.Since(start)

	close(stop)
	ln.Close()
	wg.Wait()

	r := result{
		connections: cfg.connections,
		roundtrips:  done.Load(),
		payload:     cfg.payload,
		wallMs:      float64(wall.Microseconds()) / 1000.0,
	}
	if wall > 0 && r.roundtrips > 0 {
		sec := wall.Seconds()
		r.qps = float64(r.roundtrips) / sec
		r.usPerReq = float64(wall.Microseconds()) / float64(r.roundtrips)
		r.mibps = float64(r.roundtrips*uint64(cfg.payload)*2) / 1024.0 / 1024.0 / sec
	}
	return r
}

func printResult(tag string, cfg config, r result) {
	fmt.Printf("[%s] conn=%-4d threads=%d msg/conn=%d payload=%dB  roundtrips=%d wall=%.2fms  QPS=%.0f  bw=%.2fMiB/s  lat=%.2fus\n",
		tag, r.connections, cfg.threads, cfg.messages, r.payload, r.roundtrips, r.wallMs, r.qps, r.mibps, r.usPerReq)
}

func runSweep(base config) {
	fmt.Printf("go echo sweep (threads=%d messages=%d payload=%dB)\n", base.threads, base.messages, base.payload)
	for _, c := range []int{16, 64, 128, 256, 512} {
		cfg := base
		cfg.connections = c
		printResult("go", cfg, runOnce(cfg))
	}
}

func main() {
	cfg := parseConfig()
	if cfg.sweep {
		runSweep(cfg)
		return
	}
	fmt.Printf("go echo bench threads=%d connections=%d messages=%d payload=%dB", cfg.threads, cfg.connections, cfg.messages, cfg.payload)
	if cfg.quick {
		fmt.Print(" (quick)")
	}
	fmt.Println()
	printResult("go", cfg, runOnce(cfg))
}
