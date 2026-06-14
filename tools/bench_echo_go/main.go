// bench_echo_go: TCP echo QPS 压测（Go 标准库 net）
//
//	go run ./tools/bench_echo_go --quick
//	go run ./tools/bench_echo_go --threads 4 --connections 256 --messages 2000 --payload 128
//	go run ./tools/bench_echo_go --mode server --port 19001 --threads 4
//	go run ./tools/bench_echo_go --sweep
package main

import (
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"os/signal"
	"sync"
	"sync/atomic"
	"syscall"
	"time"
)

type config struct {
	mode        string
	host        string
	port        int
	threads     int
	connections int
	messages    int
	payload     int
	duration    time.Duration
	quick       bool
	sweep       bool
}

func parseConfig() config {
	cfg := config{
		mode:        "bench",
		host:        "127.0.0.1",
		port:        19001,
		threads:     4,
		connections: 64,
		messages:    1000,
		payload:     128,
	}
	flag.StringVar(&cfg.mode, "mode", cfg.mode, "bench: loopback; server: echo server only")
	flag.StringVar(&cfg.host, "host", cfg.host, "client target host")
	flag.IntVar(&cfg.port, "port", cfg.port, "server port / client target port")
	flag.IntVar(&cfg.threads, "threads", cfg.threads, "client worker goroutines / server accept workers")
	flag.IntVar(&cfg.connections, "connections", cfg.connections, "concurrent connections")
	flag.IntVar(&cfg.messages, "messages", cfg.messages, "roundtrips per connection")
	flag.IntVar(&cfg.payload, "payload", cfg.payload, "bytes per roundtrip")
	dur := flag.Duration("duration", 0, "fixed duration test (wrk -d), e.g. 10s")
	flag.BoolVar(&cfg.quick, "quick", false, "quick smoke test")
	flag.BoolVar(&cfg.sweep, "sweep", false, "sweep connection counts")
	flag.Parse()
	if dur != nil && *dur > 0 {
		cfg.duration = *dur
	}
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
	deadline := start.Add(cfg.duration)
	useDuration := cfg.duration > 0

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
				if useDuration {
					for time.Now().Before(deadline) {
						if _, err := conn.Write(payload); err != nil {
							break
						}
						if _, err := io.ReadFull(conn, recv); err != nil {
							break
						}
						done.Add(1)
					}
				} else {
					for m := 0; m < cfg.messages; m++ {
						if _, err := conn.Write(payload); err != nil {
							break
						}
						if _, err := io.ReadFull(conn, recv); err != nil {
							break
						}
						done.Add(1)
					}
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
	mode := fmt.Sprintf("msg/conn=%d", cfg.messages)
	if cfg.duration > 0 {
		mode = fmt.Sprintf("duration=%s", cfg.duration)
	}
	fmt.Printf("[%s] conn=%-4d threads=%d %s payload=%dB  roundtrips=%d wall=%.2fms  QPS=%.0f  bw=%.2fMiB/s  lat=%.2fus\n",
		tag, r.connections, cfg.threads, mode, r.payload, r.roundtrips, r.wallMs, r.qps, r.mibps, r.usPerReq)
	fmt.Printf("SUMMARY qps=%.0f requests=%d duration=%.2fs payload=%dB threads=%d connections=%d\n",
		r.qps, r.roundtrips, r.wallMs/1000.0, r.payload, cfg.threads, cfg.connections)
}

func runServer(cfg config) {
	ln, err := net.Listen("tcp4", fmt.Sprintf("0.0.0.0:%d", cfg.port))
	if err != nil {
		fmt.Fprintf(os.Stderr, "listen failed: %v\n", err)
		os.Exit(1)
	}
	payload := make([]byte, cfg.payload)
	for i := range payload {
		payload[i] = 'G'
	}
	stop := make(chan struct{})
	go func() {
		sig := make(chan os.Signal, 1)
		signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)
		<-sig
		close(stop)
		ln.Close()
	}()

	fmt.Printf("go echo server listening on %s threads=%d payload=%dB (Ctrl+C to stop)\n",
		ln.Addr().String(), cfg.threads, cfg.payload)
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
		go echoSession(conn, payload)
	}
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
	if cfg.mode == "server" {
		runServer(cfg)
		return
	}
	if cfg.mode != "bench" {
		fmt.Fprintf(os.Stderr, "unknown mode %q (use bench or server)\n", cfg.mode)
		os.Exit(1)
	}
	if cfg.sweep {
		runSweep(cfg)
		return
	}
	fmt.Printf("go echo bench threads=%d connections=%d", cfg.threads, cfg.connections)
	if cfg.duration > 0 {
		fmt.Printf(" duration=%s", cfg.duration)
	} else {
		fmt.Printf(" messages=%d", cfg.messages)
	}
	fmt.Printf(" payload=%dB", cfg.payload)
	if cfg.quick {
		fmt.Print(" (quick)")
	}
	fmt.Println()
	printResult("go", cfg, runOnce(cfg))
}
