# LemoNettyCore — 高性能协程网络栈 v0.4

整合 netCore / netLemo 性能最优实现，提供可安装的动态库 `liblemo_nettycore.so`。

## 模块架构

```
L0  utils / thread / memory(StackPool)
L1  fiber(Scheduler/Timer/Sync) + io(Runtime/Reactor/IOManager/Hook)
L2  buffer(RingBuffer/ChainBuffer/ByteArray) + socket(Address/Socket/Stream)
L4  server(Acceptor/TcpServer/WorkerGroup)
伴生  log + config（YAML，依赖 yaml-cpp，可选 NETTYCORE_BUILD_CONFIG）
```

### Buffer 选型

| 类型 | 适用场景 |
|------|----------|
| `ChainBuffer` | **长连接流式协议**（Redis RESP、Pipeline）；`append`/`consume` 释放已读 chunk |
| `RingBuffer` | 单块环形缓冲、echo、定长 IO |
| `ByteArray` | RPC 序列化、链式节点 + 读写游标 |

Ledis 协议解析约定使用 **`lemo::buffer::ChainBuffer`**（见 `docs/ledis/protocol.md` §1.2）。

## 构建

```bash
cmake -B build -DLEMONETTYCORE_BUILD=ON -DNETCORE_BUILD=OFF
cmake --build build -j
```

产物：
- `build/LemoNettyCore/liblemo_nettycore.so` — 动态库
- `build/LemoNettyCore/libnettycore.a` — 静态库
- `build/LemoNettyCore/libnettycore_config.a` — 配置伴生库（默认开启）

## 安装

```bash
cmake --install build --prefix /usr/local
```

## 使用示例

```cpp
#include "lemo/nettycore.h"

void onConn(lemo::socket::Socket::ptr sock) { /* ... */ }

int main() {
  lemo::io::Runtime rt(4, false, "main");
  lemo::server::TcpServer server("app", &rt);
  server.bind("0.0.0.0", 9000);
  server.setConnectionHandler(onConn);
  server.start();
  // worker 线程已在 Runtime 构造时启动
  pause();
  server.stop();
  rt.stop();
}
```

## 配置（伴生库，YAML）

核心库保持轻量；配置与日志通过 `libnettycore_config.a` 提供，**使用 YAML 格式**（依赖 `libyaml-cpp-dev`）。

完整示例见 `tests/lemo/config/fixtures/nettycore_test.yaml`：

```yaml
server:
  name: echo
  host: 0.0.0.0
  port: 9000
io:
  threads: 4
  use_caller: false
fiber:
  stackpool:
    max_tls_cached: 32
log:
  level: INFO
  appender: console
  logger:
    net:
      io:
        level: DEBUG
```

嵌套结构会打平为 `server.port`、`log.logger.net.io.level` 等 key，仅代码中 `Lookup` 声明过的项会生效。

代码示例：

```cpp
#include "lemo/nettycore_config.h"
#include "lemo/nettycore.h"

int main() {
  lemo::Init("nettycore.yaml");  // 加载 log + 网络栈配置

  lemo::config::NettySettings cfg = lemo::config::GetNettySettings();
  lemo::io::Runtime::ptr rt = lemo::config::CreateRuntimeFromConfig();

  lemo::server::TcpServer server(cfg.server_name, rt.get());
  server.bind(cfg.server_host, cfg.server_port);
  server.setConnectionHandler([](lemo::socket::Socket::ptr sock) { /* ... */ });
  server.start();
  pause();
  server.stop();
  rt->stop();
}
```

运行内置演示：

```bash
cmake --build build --target nettycore_config_demo_run
# 或指定配置路径
./bin/LemoNettyCore/config/nettycore_config_demo path/to/nettycore.yaml
```

## 测试

```bash
cmake --build build --target run_tests_nettycore
cmake --build build --target run_tests_nettycore_config
ctest -R '^LemoNettyCore\.' --output-on-failure
```

## 性能

标准压测（8T/256C/128B/1000）：~340k QPS（aarch64 4核）

## 后续路线

- fcontext 替换 ucontext（突破 ~290k 上限）
- SSL / HTTP（L5，独立模块）
