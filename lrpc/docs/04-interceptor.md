# 04 — Interceptor 拦截器链

## 设计动机

gRPC 的拦截器是实现 AOP (面向切面编程) 的核心机制。
通过拦截器链可以无侵入地为所有 RPC 调用添加:
- 鉴权 (auth)
- 日志 (logging)
- 监控 (metrics/tracing)
- 限流 (rate limiting)
- 重试 (retry)
- 超时 (timeout/deadline)

## 洋葱模型

```
                  ┌─────────────────────────┐
                  │   interceptor-3 (Recovery)  ← 最外层
                  │  ┌───────────────────┐  │
                  │  │ interceptor-2 (Log)  │  │
                  │  │ ┌───────────────┐ │  │
                  │  │ │ interceptor-1  │ │  │
                  │  │ │  (Auth)       │ │  │
                  │  │ │ ┌───────────┐ │ │  │
                  │  │ │ │  handler  │ │ │  │  ← 最内层 (业务逻辑)
                  │  │ │ └───────────┘ │ │  │
                  │  │ └───────────────┘ │  │
                  │  └───────────────────┘  │
                  └─────────────────────────┘

请求 →→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→ 响应
      外层先执行，内层后执行 (栈式调用)
```

## 核心类型

### 服务端

```go
type UnaryServerInfo struct {
    Service    string  // 服务名
    Method     string  // 方法名
    FullMethod string  // "Service.Method"
}

type UnaryHandler func(req any) (any, error)

type UnaryServerInterceptor func(req any, info *UnaryServerInfo, handler UnaryHandler) (any, error)
```

### 客户端

```go
type UnaryInvoker func(method string, req, reply any) error

type UnaryClientInterceptor func(method string, req, reply any, invoker UnaryInvoker) error
```

## 链构建算法

```
从最内层逐步向外包裹:

输入: interceptors = [I1, I2, I3], handler = businessLogic

Step 1: chained = businessLogic
Step 2: chained = func(req) { I3(req, info, chained) }  // I3 是最外层
Step 3: chained = func(req) { I2(req, info, chained) }
Step 4: chained = func(req) { I1(req, info, chained) }  // I1 是最内层

最终: chained = I1(req, I2(req, I3(req, businessLogic(req))))
```

## 内置拦截器

| 拦截器 | 位置 | 功能 |
|--------|------|------|
| LoggingInterceptor | 服务端 | 记录方法名和耗时 |
| RecoveryInterceptor | 服务端 | 捕获 panic，防止连接崩溃 |
| TimeoutInterceptor | 服务端 | 为请求设置截止时间 |
| ClientLoggingInterceptor | 客户端 | 记录调用耗时 |
| ClientRetryInterceptor | 客户端 | 失败后指数退避重试 |

## 使用示例

```go
server := lrpc.NewServer()

// 先注册的在外面
server.Use(interceptor.RecoveryInterceptor())   // 最外层
server.Use(interceptor.LoggingInterceptor())    // 中间
server.Use(authInterceptor)                     // 最内层

server.Register(&MyService{})
server.Serve(":8080")
```

## 与 gRPC 拦截器对比

| 特性 | gRPC | lrpc |
|------|------|------|
| 服务端 Unary | ctx + req + info + handler | req + info + handler |
| 客户端 Unary | ctx + method + req + reply + cc + invoker | method + req + reply + invoker |
| 流式拦截器 | StreamServerInterceptor | 后续版本 |
| context 传递 | ctx 穿透所有层 | 后续版本加入 |
| 链顺序 | 先注册在外层 | 先注册在外层 (同 gRPC) |
