// bench_http_go: 标准库 HTTP echo 压测（对标 wrk -t4 -c64 -d10s）
//
//	go run . --mode server --port 19002 --payload 128
//	go run . --threads 4 --connections 64 --duration 10s --payload 128
package main

import (
	"bytes"
	"context"
	"flag"
	"fmt"
	"io"
	"net"
	"net/http"
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
}

func parseConfig() config {
	cfg := config{
		mode:        "bench",
		host:        "127.0.0.1",
		port:        19002,
		threads:     4,
		connections: 64,
		messages:    1000,
		payload:     128,
	}
	flag.StringVar(&cfg.mode, "mode", cfg.mode, "bench: loopback; server: HTTP server only")
	flag.StringVar(&cfg.host, "host", cfg.host, "client target host")
	flag.IntVar(&cfg.port, "port", cfg.port, "server port")
	flag.IntVar(&cfg.threads, "threads", cfg.threads, "client worker goroutines")
	flag.IntVar(&cfg.connections, "connections", cfg.connections, "concurrent connections")
	flag.IntVar(&cfg.messages, "messages", cfg.messages, "roundtrips per connection (messages mode)")
	flag.IntVar(&cfg.payload, "payload", cfg.payload, "bytes per roundtrip")
	dur := flag.Duration("duration", 0, "fixed duration (wrk -d), e.g. 10s")
	flag.BoolVar(&cfg.quick, "quick", false, "quick smoke test")
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

func makePayload(n int, fill byte) []byte {
	b := make([]byte, n)
	for i := range b {
		b[i] = fill
	}
	return b
}

func newHandler(payload int) http.Handler {
	body := makePayload(payload, 'G')
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path == "/ping" {
			w.WriteHeader(http.StatusOK)
			_, _ = w.Write([]byte("ok"))
			return
		}
		if r.URL.Path != "/echo" {
			http.NotFound(w, r)
			return
		}
		reqBody, err := io.ReadAll(r.Body)
		if err != nil {
			http.Error(w, "bad body", http.StatusBadRequest)
			return
		}
		if len(reqBody) == payload {
			w.Header().Set("Content-Type", "application/octet-stream")
			w.Header().Set("Content-Length", fmt.Sprintf("%d", payload))
			_, _ = w.Write(reqBody)
			return
		}
		w.Header().Set("Content-Type", "application/octet-stream")
		w.Header().Set("Content-Length", fmt.Sprintf("%d", len(body)))
		_, _ = w.Write(body)
	})
}

func startServer(port, payload int) (*http.Server, string, error) {
	ln, err := net.Listen("tcp4", fmt.Sprintf("127.0.0.1:%d", port))
	if err != nil {
		return nil, "", err
	}
	addr := ln.Addr().String()
	srv := &http.Server{
		Handler:           newHandler(payload),
		ReadHeaderTimeout: 5 * time.Second,
	}
	go func() { _ = srv.Serve(ln) }()
	deadline := time.Now().Add(5 * time.Second)
	url := fmt.Sprintf("http://%s/ping", addr)
	for time.Now().Before(deadline) {
		resp, err := http.Get(url)
		if err == nil {
			io.Copy(io.Discard, resp.Body)
			resp.Body.Close()
			if resp.StatusCode == http.StatusOK {
				return srv, addr, nil
			}
		}
		time.Sleep(5 * time.Millisecond)
	}
	_ = srv.Close()
	return nil, "", fmt.Errorf("server not ready on %s", addr)
}

func newClient() *http.Client {
	return &http.Client{
		Transport: &http.Transport{
			DisableKeepAlives:     false,
			MaxIdleConns:          1,
			MaxIdleConnsPerHost:   1,
			MaxConnsPerHost:       1,
			ResponseHeaderTimeout: 30 * time.Second,
		},
	}
}

func echoOnce(client *http.Client, url string, payload, recv []byte) bool {
	req, err := http.NewRequest(http.MethodPost, url, bytes.NewReader(payload))
	if err != nil {
		return false
	}
	req.ContentLength = int64(len(payload))
	resp, err := client.Do(req)
	if err != nil {
		return false
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return false
	}
	n, err := io.ReadFull(resp.Body, recv)
	return err == nil && n == len(recv)
}

func runBench(cfg config) {
	srv, addr, err := startServer(0, cfg.payload)
	if err != nil {
		fmt.Fprintf(os.Stderr, "start server failed: %v\n", err)
		os.Exit(1)
	}
	defer func() {
		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()
		_ = srv.Shutdown(ctx)
	}()

	echoURL := fmt.Sprintf("http://%s/echo", addr)
	payload := makePayload(cfg.payload, 'G')
	recv := make([]byte, cfg.payload)
	useDuration := cfg.duration > 0
	deadline := time.Now().Add(cfg.duration)

	start := time.Now()
	connCh := make(chan int, cfg.connections)
	for i := 0; i < cfg.connections; i++ {
		connCh <- i
	}
	close(connCh)

	var done atomic.Uint64
	var wg sync.WaitGroup
	workers := cfg.threads
	if workers > cfg.connections {
		workers = cfg.connections
	}
	for w := 0; w < workers; w++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			client := newClient()
			defer client.CloseIdleConnections()
			var local uint64
			for range connCh {
				if useDuration {
					for time.Now().Before(deadline) {
						if echoOnce(client, echoURL, payload, recv) {
							local++
						} else {
							break
						}
					}
				} else {
					for m := 0; m < cfg.messages; m++ {
						if echoOnce(client, echoURL, payload, recv) {
							local++
						} else {
							break
						}
					}
				}
			}
			if local > 0 {
				done.Add(local)
			}
		}()
	}
	wg.Wait()
	wall := time.Since(start)

	roundtrips := done.Load()
	qps := float64(roundtrips) / wall.Seconds()
	mib := float64(roundtrips*uint64(cfg.payload)*2) / 1024.0 / 1024.0 / wall.Seconds()
	lat := float64(wall.Microseconds()) / float64(max64(roundtrips, 1))

	mode := fmt.Sprintf("messages=%d", cfg.messages)
	if useDuration {
		mode = fmt.Sprintf("duration=%s", cfg.duration)
	}
	fmt.Printf("go http bench threads=%d connections=%d %s payload=%dB\n", cfg.threads, cfg.connections, mode, cfg.payload)
	fmt.Printf("[go] conn=%-4d threads=%d %s payload=%dB  roundtrips=%d wall=%.2fms  QPS=%.0f  bw=%.2fMiB/s  lat=%.2fus\n",
		cfg.connections, cfg.threads, mode, cfg.payload, roundtrips, float64(wall.Microseconds())/1000.0, qps, mib, lat)
	fmt.Printf("SUMMARY qps=%.0f requests=%d duration=%.2fs payload=%dB threads=%d connections=%d\n",
		qps, roundtrips, wall.Seconds(), cfg.payload, cfg.threads, cfg.connections)
}

func max64(a, b uint64) uint64 {
	if a > b {
		return a
	}
	return b
}

func runServer(cfg config) {
	srv, addr, err := startServer(cfg.port, cfg.payload)
	if err != nil {
		fmt.Fprintf(os.Stderr, "start server failed: %v\n", err)
		os.Exit(1)
	}
	fmt.Printf("go http echo server listening %s payload=%dB (POST /echo)\n", addr, cfg.payload)
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)
	<-sig
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	_ = srv.Shutdown(ctx)
}

func main() {
	cfg := parseConfig()
	if cfg.mode == "server" {
		runServer(cfg)
		return
	}
	runBench(cfg)
}
