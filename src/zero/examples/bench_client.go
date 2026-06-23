// Benchmark 客户端 — 并发连接压测 echo server
//
// 用法: go run bench_client.go [host:port] [connections] [requests_per_conn]
//       ./bench_client :8080 100 10

package main

import (
	"fmt"
	"net"
	"os"
	"strconv"
	"sync"
	"time"
)

func main() {
	addr := ":8080"
	concurrency := 100
	requests := 10

	if len(os.Args) > 1 {
		addr = os.Args[1]
	}
	if len(os.Args) > 2 {
		concurrency, _ = strconv.Atoi(os.Args[2])
	}
	if len(os.Args) > 3 {
		requests, _ = strconv.Atoi(os.Args[3])
	}

	msg := "Hello, Echo Server! Benchmark Message 12345678\n"  // 行分隔
	expected := len(msg)

	fmt.Printf("Benchmark: %s  concurrency=%d  requests/conn=%d\n", addr, concurrency, requests)

	var wg sync.WaitGroup
	var mu sync.Mutex
	totalBytes := int64(0)
	totalLatency := time.Duration(0)
	errors := 0
	connects := 0

	start := time.Now()

	for i := 0; i < concurrency; i++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()

			conn, err := net.DialTimeout("tcp", addr, 3*time.Second)
			if err != nil {
				mu.Lock()
				errors++
				mu.Unlock()
				return
			}
			defer conn.Close()

			mu.Lock()
			connects++
			mu.Unlock()

			buf := make([]byte, 4096)

			for j := 0; j < requests; j++ {
				t0 := time.Now()

				// Send
				if _, err := conn.Write([]byte(msg)); err != nil {
					mu.Lock()
					errors++
					mu.Unlock()
					return
				}

				// Read echo
				total := 0
				for total < expected {
					n, err := conn.Read(buf[total:])
					if err != nil {
						mu.Lock()
						errors++
						mu.Unlock()
						return
					}
					total += n
				}

				elapsed := time.Since(t0)

				mu.Lock()
				totalBytes += int64(len(msg) * 2) // send + recv
				totalLatency += elapsed
				mu.Unlock()
			}
		}(i)
	}

	wg.Wait()
	elapsed := time.Since(start)

	totalReqs := concurrency * requests
	successReqs := totalReqs - errors
	throughput := float64(totalBytes) / elapsed.Seconds() / 1024 / 1024
	avgLatency := time.Duration(0)
	if successReqs > 0 {
		avgLatency = totalLatency / time.Duration(successReqs)
	}
	rps := float64(successReqs) / elapsed.Seconds()

	fmt.Printf("  Time:        %v\n", elapsed.Round(time.Millisecond))
	fmt.Printf("  Connections: %d/%d\n", connects, concurrency)
	fmt.Printf("  Requests:    %d/%d (errors=%d)\n", successReqs, totalReqs, errors)
	fmt.Printf("  Throughput:  %.2f MB/s\n", throughput)
	fmt.Printf("  RPS:         %.0f req/s\n", rps)
	fmt.Printf("  Avg Latency: %v\n", avgLatency.Round(time.Microsecond))
}
