// Package interceptor 实现 RPC 拦截器链。
//
// 拦截器是 gRPC 中最优雅的设计之一，采用洋葱模型的责任链:
//
//	                    ┌─────────────────────────┐
//	                    │   interceptor-3         │
//	                    │  ┌───────────────────┐  │
//	                    │  │ interceptor-2     │  │
//	                    │  │ ┌───────────────┐ │  │
//	                    │  │ │ interceptor-1 │ │  │
//	                    │  │ │   handler     │ │  │
//	                    │  │ └───────────────┘ │  │
//	                    │  └───────────────────┘  │
//	                    └─────────────────────────┘
//
//	请求 →→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→ 响应
//	       外层先执行，内层后执行 (类似栈)
//
// 使用示例:
//
//	// 定义拦截器
//	func LogInterceptor() interceptor.UnaryServerInterceptor {
//	    return func(req any, info *interceptor.UnaryServerInfo, handler interceptor.UnaryHandler) (any, error) {
//	        start := time.Now()
//	        log.Printf("→ %s", info.Method)
//	        resp, err := handler(req)  // 调用下一层
//	        log.Printf("← %s (%v) err=%v", info.Method, time.Since(start), err)
//	        return resp, err
//	    }
//	}
//
//	// 注册到 Server
//	server.Use(LogInterceptor())
//	server.Use(interceptor.RecoveryInterceptor())
package interceptor

import (
	"fmt"
	"log"
	"runtime"
	"time"
)

// UnaryServerInfo 服务端一元 RPC 方法的信息
type UnaryServerInfo struct {
	// Service 服务名
	Service string
	// Method 方法名 (不含服务名)
	Method string
	// FullMethod 完整方法路径: "Service.Method"
	FullMethod string
}

// UnaryHandler 一元 RPC 的处理函数
// 拦截器调用 handler(req) 将请求传递给下一层 (内部拦截器或最终的业务方法)
type UnaryHandler func(req any) (any, error)

// UnaryServerInterceptor 服务端一元拦截器
// 签名:
//
//	func(req, info, handler) (resp, err)
//
// 拦截器在调用 handler(req) 前后可以执行逻辑:
//   - 调用前: 鉴权、参数校验、限流
//   - 调用后: 日志、metric、错误转换
type UnaryServerInterceptor func(req any, info *UnaryServerInfo, handler UnaryHandler) (any, error)

// UnaryInvoker 客户端一元调用的实际执行函数
// 最终发送请求到服务端并等待响应
type UnaryInvoker func(method string, req, reply any) error

// UnaryClientInterceptor 客户端一元拦截器
// 签名:
//
//	func(method, req, reply, invoker) error
//
// 可以在调用前修改请求、调用后修改响应或重试
type UnaryClientInterceptor func(method string, req, reply any, invoker UnaryInvoker) error

// ChainUnaryServer 将多个服务端拦截器链成一个 UnaryHandler
// info 会透传给每一层拦截器
//
//	chain := ChainUnaryServer(interceptors, info, handler)
//	resp, err := chain(req)  // interceptor0 → interceptor1 → ... → handler
func ChainUnaryServer(interceptors []UnaryServerInterceptor, info *UnaryServerInfo, handler UnaryHandler) UnaryHandler {
	chained := handler
	for i := len(interceptors) - 1; i >= 0; i-- {
		inter := interceptors[i]
		next := chained
		chained = func(req any) (any, error) {
			return inter(req, info, next)
		}
	}
	return chained
}

// ChainUnaryClient 将多个客户端拦截器链成一个 UnaryInvoker
func ChainUnaryClient(interceptors []UnaryClientInterceptor, invoker UnaryInvoker) UnaryInvoker {
	chained := invoker
	for i := len(interceptors) - 1; i >= 0; i-- {
		interceptor := interceptors[i]
		next := chained
		chained = func(method string, req, reply any) error {
			return interceptor(method, req, reply, next)
		}
	}
	return chained
}

// ============================================
// 内置拦截器示例
// ============================================

// LoggingInterceptor 日志拦截器 — 记录每次 RPC 调用的方法名和耗时
func LoggingInterceptor() UnaryServerInterceptor {
	return func(req any, info *UnaryServerInfo, handler UnaryHandler) (any, error) {
		start := time.Now()
		method := "unknown"
		if info != nil {
			method = info.FullMethod
		}
		log.Printf("[lrpc] → %s", method)
		resp, err := handler(req)
		log.Printf("[lrpc] ← %s (%v) err=%v", method, time.Since(start), err)
		return resp, err
	}
}

// RecoveryInterceptor 恢复拦截器 — 捕获 handler 中的 panic
// 防止单个请求的 panic 导致整个连接崩溃
func RecoveryInterceptor() UnaryServerInterceptor {
	return func(req any, info *UnaryServerInfo, handler UnaryHandler) (resp any, err error) {
		defer func() {
			if r := recover(); r != nil {
				// 获取堆栈信息
				buf := make([]byte, 4096)
				n := runtime.Stack(buf, false)
				log.Printf("[lrpc] PANIC in %s: %v\n%s", info.FullMethod, r, buf[:n])
				err = fmt.Errorf("lrpc: panic in handler: %v", r)
			}
		}()
		return handler(req)
	}
}

// TimeoutInterceptor 超时拦截器 — 为每个请求设置截止时间
func TimeoutInterceptor(timeout time.Duration) UnaryServerInterceptor {
	return func(req any, info *UnaryServerInfo, handler UnaryHandler) (any, error) {
		type result struct {
			resp any
			err  error
		}

		done := make(chan result, 1)
		go func() {
			resp, err := handler(req)
			done <- result{resp, err}
		}()

		select {
		case r := <-done:
			return r.resp, r.err
		case <-time.After(timeout):
			return nil, fmt.Errorf("lrpc: request timeout after %v", timeout)
		}
	}
}

// ClientLoggingInterceptor 客户端日志拦截器
func ClientLoggingInterceptor() UnaryClientInterceptor {
	return func(method string, req, reply any, invoker UnaryInvoker) error {
		start := time.Now()
		log.Printf("[lrpc-client] → %s", method)
		err := invoker(method, req, reply)
		log.Printf("[lrpc-client] ← %s (%v) err=%v", method, time.Since(start), err)
		return err
	}
}

// ClientRetryInterceptor 客户端重试拦截器 — 失败后最多重试 maxRetries 次
func ClientRetryInterceptor(maxRetries int) UnaryClientInterceptor {
	return func(method string, req, reply any, invoker UnaryInvoker) error {
		var err error
		for i := 0; i <= maxRetries; i++ {
			err = invoker(method, req, reply)
			if err == nil {
				return nil
			}
			if i < maxRetries {
				backoff := time.Duration(1<<uint(i)) * 100 * time.Millisecond
				log.Printf("[lrpc-client] retry %d/%d for %s after %v", i+1, maxRetries, method, backoff)
				time.Sleep(backoff)
			}
		}
		return fmt.Errorf("lrpc: %s failed after %d retries: %w", method, maxRetries, err)
	}
}
