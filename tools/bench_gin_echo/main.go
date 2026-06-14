// bench_gin_echo: Gin HTTP echo QPS 压测
//
//	go run . --mode bench --quick
//	go run . --threads 4 --connections 256 --messages 2000 --payload 128
//	go run . --sweep --threads 4 --messages 2000 --payload 128
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
	"sync"
	"sync/atomic"
	"time"

	"github.com/gin-gonic/gin"
)

type config struct {
	mode        string
	threads     int
	connections int
	messages    int
	payload     int
	quick       bool
	sweep       bool
}

func parseConfig() config {
	cfg := config{
		mode:        "bench",
		threads:     4,
		connections: 64,
		messages:    1000,
		payload:     128,
	}
	flag.StringVar(&cfg.mode, "mode", cfg.mode, "bench: server+client load test; server: HTTP only")
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

func newRouter() *gin.Engine {
	gin.SetMode(gin.ReleaseMode)
	r := gin.New()
	r.GET("/ping", func(c *gin.Context) {
		c.String(http.StatusOK, "ok")
	})
	r.POST("/echo", func(c *gin.Context) {
		body, err := io.ReadAll(c.Request.Body)
		if err != nil {
			c.Status(http.StatusBadRequest)
			return
		}
		c.Data(http.StatusOK, "application/octet-stream", body)
	})
	return r
}

func startServer() (*http.Server, string, error) {
	ln, err := net.Listen("tcp4", "127.0.0.1:0")
	if err != nil {
		return nil, "", err
	}
	srv := &http.Server{Handler: newRouter()}
	go func() {
		_ = srv.Serve(ln)
	}()
	baseURL := "http://" + ln.Addr().String()
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		resp, err := http.Get(baseURL + "/ping")
		if err == nil {
			io.Copy(io.Discard, resp.Body)
			resp.Body.Close()
			if resp.StatusCode == http.StatusOK {
				return srv, baseURL, nil
			}
		}
		time.Sleep(5 * time.Millisecond)
	}
	srv.Close()
	return nil, "", fmt.Errorf("server not ready")
}

func makePayload(n int) []byte {
	b := make([]byte, n)
	for i := range b {
		b[i] = 'G'
	}
	return b
}

func newKeepAliveClient() *http.Client {
	return &http.Client{
		Transport: &http.Transport{
			DisableKeepAlives:   false,
			MaxIdleConns:        1,
			MaxIdleConnsPerHost: 1,
			MaxConnsPerHost:     1,
		},
		Timeout: 0,
	}
}

func echoOnClient(client *http.Client, baseURL string, payload []byte, messages int) uint64 {
	var ok uint64
	echoURL := baseURL + "/echo"
	for m := 0; m < messages; m++ {
		req, err := http.NewRequest(http.MethodPost, echoURL, bytes.NewReader(payload))
		if err != nil {
			break
		}
		req.ContentLength = int64(len(payload))
		resp, err := client.Do(req)
		if err != nil {
			break
		}
		body, err := io.ReadAll(resp.Body)
		resp.Body.Close()
		if err != nil || resp.StatusCode != http.StatusOK || len(body) != len(payload) {
			break
		}
		ok++
	}
	return ok
}

func runOnce(cfg config) result {
	srv, baseURL, err := startServer()
	if err != nil {
		fmt.Fprintf(os.Stderr, "start server failed: %v\n", err)
		os.Exit(1)
	}
	defer func() {
		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()
		_ = srv.Shutdown(ctx)
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
	payload := makePayload(cfg.payload)
	for w := 0; w < workers; w++ {
		clientWg.Add(1)
		go func() {
			defer clientWg.Done()
			for range connCh {
				client := newKeepAliveClient()
				done.Add(echoOnClient(client, baseURL, payload, cfg.messages))
				client.CloseIdleConnections()
			}
		}()
	}
	clientWg.Wait()
	wall := time.Since(start)

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
	fmt.Printf("gin echo sweep (threads=%d messages=%d payload=%dB)\n", base.threads, base.messages, base.payload)
	for _, c := range []int{16, 64, 128, 256, 512} {
		cfg := base
		cfg.connections = c
		printResult("gin", cfg, runOnce(cfg))
	}
}

func runServerOnly() {
	srv, baseURL, err := startServer()
	_ = srv
	if err != nil {
		fmt.Fprintf(os.Stderr, "start server failed: %v\n", err)
		os.Exit(1)
	}
	fmt.Printf("gin echo server listening %s (GET /ping POST /echo)\n", baseURL)
	select {}
}

func main() {
	cfg := parseConfig()
	if cfg.mode == "server" {
		runServerOnly()
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
	fmt.Printf("gin echo bench threads=%d connections=%d messages=%d payload=%dB", cfg.threads, cfg.connections, cfg.messages, cfg.payload)
	if cfg.quick {
		fmt.Print(" (quick)")
	}
	fmt.Println()
	printResult("gin", cfg, runOnce(cfg))
}
