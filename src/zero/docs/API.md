# Zero 网络库 API 参考

> 高性能 C++17 协程网络库 | ARM64/x86_64 | header-only lstl + 静态库 zero

---

## 快速开始

```cpp
#include "zero/zero.h"

int main() {
    zero::Scheduler sched(4, false, "main");  // 4 worker 线程
    sched.start();

    // 调度一个协程
    sched.schedule(std::make_shared<zero::Fiber>([]() {
        printf("Hello from fiber!\n");
    }));

    sched.stop();
}
```

---

## 模块总览

```
zero/
├── fiber/      协程 — 有栈非对称, ucontext 切换 ~200ns
├── scheduler/  M:N 调度 — work-stealing + per-thread reactor
├── net/        Socket / Address / Buffer(链式) / Stream / TcpServer
├── log/        日志 — 7级/层级/Color/MDC/限流/Rolling/Async/YAML
├── config/     配置 — ConfigVar<T> + YAML + 热加载回调
├── thread/     线程 — SpinLock/Mutex/RWMutex/Semaphore/Thread
├── base/       基础 — noncopyable/singleton/macro/endian
└── util/       工具
```

---

## Fiber (协程)

### 创建与调度

```cpp
// 创建 Fiber
auto f = std::make_shared<zero::Fiber>([]() {
    printf("fiber running\n");
    zero::Fiber::YieldToReady();  // 让出
    printf("fiber resumed\n");
});

// 调度到 Scheduler
scheduler.schedule(f);
```

### API

| 方法 | 说明 |
|------|------|
| `Fiber(Callback, stack_size=128KB, name="")` | 创建协程 |
| `Fiber::YieldToReady()` | 让出, 回到 READY 队列 |
| `Fiber::YieldToHold()` | 让出, 进入 HOLD 状态 (等待 IO 唤醒) |
| `Fiber::GetId()` | 获取协程 ID |
| `Fiber::GetTotalCount()` | 全局协程总数 |
| `Fiber::GetCurrent()` | 获取当前协程 (仅本线程) |

---

## Scheduler (调度器)

### 创建

```cpp
zero::Scheduler sched(
    4,           // worker 线程数 (0 = 使用调用线程)
    false,       // use_caller (false = 纯 worker)
    "my_sched"   // 名称
);
sched.start();
// ... 调度 fibers ...
sched.stop();
```

### API

| 方法 | 说明 |
|------|------|
| `start()` | 启动 worker 线程 |
| `stop()` | 停止, 等待所有 fiber 完成 |
| `schedule(Fiber::ptr, thread=-1)` | 调度 fiber (-1 = 任意线程) |
| `schedule(Callback)` | 用 callback 创建 fiber 并调度 |

---

## Network (Socket/Buffer/Stream/TcpServer)

### TcpServer — 一行启动

```cpp
auto addr = zero::IPv4Address::Create("0.0.0.0", 8888);
auto server = std::make_shared<zero::TcpServer>(&sched, addr, "echo");

server->setConnectionCallback([](zero::Socket::ptr sock) {
    zero::SocketStream stream(sock);
    char buf[4096];
    while (true) {
        ssize_t n = stream.read(buf, sizeof(buf));  // fiber 自动 yield
        if (n <= 0) break;
        stream.writeFixed(buf, n);
    }
});

server->start();
```

### Socket

```cpp
auto sock = zero::Socket::CreateTCPSocket();
sock->bind(zero::IPv4Address::Create("0.0.0.0", 0));
sock->connect(remote_addr);   // fiber 自动 yield
sock->setTimeout(SO_RCVTIMEO, 5000);  // 5s 超时
```

### Address

```cpp
auto ipv4 = zero::IPv4Address::Create("192.168.1.1", 8080);
ipv4->getIPString();      // "192.168.1.1"
ipv4->getPort();          // 8080
ipv4->networkAddress(24); // 192.168.1.0/24

auto ipv6 = zero::IPv6Address::Create("::1", 9090);
```

### ByteBuffer — 链式零拷贝

```cpp
zero::ByteBuffer buf(4096);

// 写入
buf.writeFInt32(42);
buf.writeStringF16("hello");
buf.writeDouble(3.14);

// 读取
buf.setPosition(0);
int v = buf.readFInt32();     // 42
auto s = buf.readStringF16(); // "hello"

// 零拷贝 iovec
std::vector<iovec> iovs;
buf.getReadBuffers(iovs);     // 直接获取内核可用的 iovec
```

### SocketStream

```cpp
// Fiber-aware 流: read/write 自动处理 EAGAIN → fiber yield → 恢复
zero::SocketStream stream(socket);
stream.read(buf, len);        // 阻塞式写法的异步实现
stream.writeFixed(buf, len);  // 循环写直到写完
```

### Hook — 透明异步化

```cpp
zero::SetHookEnabled(true);  // 启用 syscall 劫持

// 所有阻塞 syscall 自动转为 fiber yield + epoll
sleep(5);           // → fiber yield 5s
read(fd, buf, n);   // → fiber yield until readable
connect(...);       // → fiber yield until connected
```

---

## Log (日志)

### 快速使用

```cpp
#include "zero/log/log.h"

auto root = ZERO_LOG_ROOT();
ZERO_LOG_INFO(root) << "hello " << 42;
ZERO_LOG_ERROR(root) << "something went wrong";

// 层级 Logger
auto net = ZERO_LOG_NAME("zero.net.http");
ZERO_LOG_DEBUG(net) << "request path=" << path;
```

### YAML 配置

```yaml
log:
  root_level: DEBUG
  pattern: "%d [%p] [%N] %m%n"
  appenders:
    - type: console
      color: true
    - type: file
      file: /var/log/app.log
      max_size: 104857600
      max_files: 10
      level: ERROR
  loggers:
    zero.reactor: { level: TRACE }
    zero.scheduler: { level: INFO }
```

### MDC 上下文

```cpp
MDC::put("request_id", "abc-123");
MDC::put("user", "alice");
ZERO_LOG_INFO(logger) << "processing";  
// 输出: 2025-06-15 [INFO] request_id=abc-123 user=alice processing

// Pattern: %d [%p] rid=%X{request_id} user=%X{user} %m%n
```

### 性能

| 模式 | QPS | 延迟 |
|------|-----|------|
| Async (ringbuf) 单线程 | 273万/s | 366ns |
| Async (ringbuf) 4线程 | 550万/s | - |
| Sync console | 89万/s | 1126ns |
| Sync file | 48万/s | 2092ns |

---

## Config (配置)

```cpp
// 定义配置项
auto var = zero::Config::Lookup<int>("server.port", 8080, "listen port");

// 读取
int port = var->getValue();

// 监听变更 (热加载)
var->addListener([](int old_val, int new_val) {
    printf("port changed: %d → %d\n", old_val, new_val);
});

// 从 YAML 加载
zero::Config::LoadFromYaml("config.yaml");
```

---

## Thread (线程)

```cpp
// SpinLock — 极短临界区
zero::SpinLock sl;
sl.lock();
// ... 几行代码 ...
sl.unlock();

// Mutex — 通用互斥锁
zero::Mutex m;
{ zero::Mutex::Lock lock(m); /* 临界区 */ }

// RWMutex — 读多写少
zero::RWMutex rw;
{ zero::RWMutex::ReadLock lock(rw);  /* 读 */ }
{ zero::RWMutex::WriteLock lock(rw); /* 写 */ }
```

---

## 构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Echo server
./examples/echo_minimal 8888

# Benchmark
./examples/bench_echo 127.0.0.1 8888 50 10
```

**依赖**: C++17, CMake 3.14+, yaml-cpp, pthread, Linux (epoll)
