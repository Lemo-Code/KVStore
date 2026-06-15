// Go echo server — 原生 goroutine 实现，用于对比 Zero 网络库
//
// 用法: go run echo_go.go [port]
//       ./echo_go [port]

package main

import (
	"bufio"
	"fmt"
	"net"
	"os"
)

func handleConn(conn net.Conn) {
	defer conn.Close()
	// 行模式 — 避免 TCP 粘包问题
	scanner := bufio.NewScanner(conn)
	for scanner.Scan() {
		line := scanner.Text() + "\n"
		conn.Write([]byte(line))
	}
}

func main() {
	port := "8080"
	if len(os.Args) > 1 {
		port = os.Args[1]
	}

	ln, err := net.Listen("tcp", ":"+port)
	if err != nil {
		fmt.Fprintf(os.Stderr, "listen error: %v\n", err)
		os.Exit(1)
	}
	defer ln.Close()

	fmt.Printf("Go echo server on :%s\n", port)

	for {
		conn, err := ln.Accept()
		if err != nil {
			continue
		}
		go handleConn(conn)
	}
}
