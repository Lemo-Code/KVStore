// 示例: lrpc 服务端 (含拦截器演示)
//
// 运行: go run examples/hello/server/main.go
package main

import (
	"fmt"
	"log"
	"time"

	"lrpc"
	"lrpc/interceptor"
)

// Args 算术运算参数
type Args struct {
	A, B int
}

// Arith 算术运算服务
type Arith struct{}

func (a *Arith) Add(args *Args, reply *int) error {
	*reply = args.A + args.B
	return nil
}

func (a *Arith) Subtract(args *Args, reply *int) error {
	*reply = args.A - args.B
	return nil
}

func (a *Arith) Multiply(args *Args, reply *int) error {
	*reply = args.A * args.B
	return nil
}

func (a *Arith) Divide(args *Args, reply *int) error {
	if args.B == 0 {
		return fmt.Errorf("division by zero: %d / %d", args.A, args.B)
	}
	*reply = args.A / args.B
	return nil
}

// 自定义鉴权拦截器示例
func authInterceptor() interceptor.UnaryServerInterceptor {
	return func(req any, info *interceptor.UnaryServerInfo, handler interceptor.UnaryHandler) (any, error) {
		log.Printf("[auth] checking permission for %s", info.FullMethod)
		// 模拟鉴权逻辑 — 生产环境可检查 token/证书
		return handler(req)
	}
}

// 自定义耗时统计拦截器
func timingInterceptor() interceptor.UnaryServerInterceptor {
	return func(req any, info *interceptor.UnaryServerInfo, handler interceptor.UnaryHandler) (any, error) {
		start := time.Now()
		resp, err := handler(req)
		elapsed := time.Since(start)
		log.Printf("[timing] %s took %v", info.FullMethod, elapsed)
		return resp, err
	}
}

func main() {
	server := lrpc.NewServer()

	// 注册拦截器 — 按注册顺序执行
	// 最外层: panic 恢复
	server.Use(interceptor.RecoveryInterceptor())
	// 第二层: 日志
	server.Use(interceptor.LoggingInterceptor())
	// 第三层: 鉴权
	server.Use(authInterceptor())
	// 最内层: 耗时统计
	server.Use(timingInterceptor())

	// 注册服务
	if err := server.Register(&Arith{}); err != nil {
		log.Fatalf("register: %v", err)
	}

	// 启动服务
	log.Println("Starting lrpc server on :8080...")
	if err := server.Serve(":8080"); err != nil {
		log.Fatalf("serve: %v", err)
	}
}
