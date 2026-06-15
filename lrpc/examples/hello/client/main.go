// 示例: lrpc 客户端 (含拦截器演示)
//
// 运行: go run examples/hello/client/main.go
package main

import (
	"fmt"
	"log"

	"lrpc"
	"lrpc/interceptor"
)

type Args struct {
	A, B int
}

func main() {
	client, err := lrpc.Dial("localhost:8080")
	if err != nil {
		log.Fatalf("dial: %v", err)
	}
	defer client.Close()

	// 注册客户端拦截器
	client.Use(interceptor.ClientLoggingInterceptor())

	fmt.Println("=== lrpc 客户端测试 ===")
	fmt.Println()

	// 测试不同方法
	tests := []struct {
		method string
		a, b   int
	}{
		{"Arith.Add", 10, 5},
		{"Arith.Subtract", 10, 3},
		{"Arith.Multiply", 6, 7},
		{"Arith.Divide", 100, 4},
	}

	for _, tc := range tests {
		var reply int
		err := client.Call(tc.method, &Args{A: tc.a, B: tc.b}, &reply)
		if err != nil {
			fmt.Printf("  ✗ %s(%d, %d) = error: %v\n", tc.method, tc.a, tc.b, err)
		} else {
			fmt.Printf("  ✓ %s(%d, %d) = %d\n", tc.method, tc.a, tc.b, reply)
		}
	}

	// 测试除零错误
	fmt.Println()
	fmt.Println("--- 错误处理测试 ---")
	{
		var reply int
		err := client.Call("Arith.Divide", &Args{A: 100, B: 0}, &reply)
		if err != nil {
			fmt.Printf("  ✓ Divide(100, 0) correctly returned error: %v\n", err)
		} else {
			fmt.Printf("  ✗ Divide(100, 0) unexpectedly succeeded: %d\n", reply)
		}
	}

	fmt.Println()
	fmt.Println("All tests completed!")
}
