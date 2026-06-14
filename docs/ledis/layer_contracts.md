# Ledis 各层接口契约

本文定义 Ledis 各子模块 **公开 API** 的最小契约。实现须满足这些签名与语义；Phase 0 可在 `ledis/include/` 放置骨架头文件。

---

## L0 Foundation

### `ledis::Sds`

二进制安全字符串，语义类似 Redis SDS。

| 方法 | 语义 |
|------|------|
| `Sds()` / `Sds(const char*, len)` | 构造 |
| `data()` / `size()` / `empty()` | 访问 |
| `operator==` | 按长度+memcmp 比较 |
| `hash()` | dict 用 |

- 允许 `\0`；**不是** NUL -terminated C 字符串依赖（可提供 `c_str()` 冗余 `\0` 便于调试）。

---

## L1 Store

### `ledis::LedisObject`

| 字段/方法 | 语义 |
|-----------|------|
| `type()` | `kString` / … |
| `encoding()` | `kRaw` / `kInt` |
| `asString()` | 类型检查，失败返回 nullopt |
| `setString(Sds)` | 仅 kString |

### `ledis::Keyspace`

| 方法 | 语义 |
|------|------|
| `get(key, out)` | 存在且未过期返回 true |
| `set(key, obj, ttl_ms=0)` | 0=永不过期 |
| `del(key)` | 返回删除个数 0/1 |
| `exists(key)` | 含过期判定 |
| `size()` | 键数量（不含已过期未清理） |
| `flush()` | 清空 |
| `activeExpireCycle(max_keys)` | 主动过期，返回清理数量 |

### `ledis::DBManager`

| 方法 | 语义 |
|------|------|
| `dbCount()` | 固定 16 |
| `select(ctx, index)` | 0..15，否则错误 |
| `keyspace(index)` | 返回 Keyspace& |
| `keyspaceOf(ctx)` | 当前 ctx.db_index |

**线程安全**：仅 **DB Worker 线程** 调用；无内部锁。

---

## L2 Protocol

### `ledis::RespValue`

```cpp
enum class RespType { kSimpleString, kError, kInteger, kBulkString, kNull, kArray };

struct RespValue {
  RespType type;
  int64_t integer;
  Sds bulk;                    // string / error body
  std::vector<RespValue> array;
};
```

### `ledis::RespReader`

| 方法 | 语义 |
|------|------|
| `explicit RespReader(lemo::buffer::ChainBuffer& chain)` | 绑定连接读缓冲（长连接生命周期内不变） |
| `TryParseOne(RespValue* out, Command* cmd)` | 从 chain **窥视** 解析一帧；成功则 **consume** 已读字节 |
| `chain()` | 底层 ChainBuffer 引用 |

- 半包时返回 `kNeedMore`，**不** consume。
- 成功解析后必须 `chain.consume(n)`，`n` 为整帧字节数。

### `ledis::RespWriter`

| 方法 | 语义 |
|------|------|
| `encode(const RespValue&)` | 返回完整 RESP 字节串 |
| `encodeSimpleString` / `encodeError` / … | 便捷方法 |
| `encodeCommand(const std::vector<Sds>& argv)` | 客户端测试用 |

---

## L3 Command

### `ledis::Command`

```cpp
struct Command {
  Sds name;                    // 大写，如 "GET"
  std::vector<Sds> args;
};
```

### `ledis::CommandResult`

```cpp
struct CommandResult {
  bool ok;
  RespValue value;             // 响应体
  // ok=false 时 value.type==kError
};
```

### `ledis::CommandRegistry`

| 方法 | 语义 |
|------|------|
| `registerHandler(name, fn)` | 注册命令 |
| `dispatch(ctx, cmd)` | 查找 handler；未知命令返回 `-ERR unknown command` |

Handler 签名：

```cpp
using Handler = std::function<CommandResult(ClientContext&, DBManager&, const Command&)>;
```

---

## L4 Session

### `ledis::CommandQueue`

跨 IO / DB 边界；Phase 1 单线程模式下可为同步调用。

| 方法 | 语义 |
|------|------|
| `submit(ctx, cmd)` | 阻塞直到 DB 执行完，返回 CommandResult |
| `submitAsync(...)` | Phase 2+：返回 future/promise，供 Pipeline 优化 |

### `ledis::Session`

| 方法 | 语义 |
|------|------|
| `Session(Socket::ptr, CommandQueue*, LedisSettings*)` | |
| `run()` | 协程主循环，直到 EOF 或错误 |
| `context()` | Session 级 ClientContext |

---

## L5 Server

### `ledis::LedisServer`

| 方法 | 语义 |
|------|------|
| `init(config_path)` | 加载 YAML，创建 Runtime(s)、TcpServer |
| `start()` | bind + listen |
| `stop()` | 优雅关闭：停 accept → 关连接 → 停 DB → 停 IO |
| `wait()` | 阻塞主线程（信号退出） |

### 配置 `ledis::LedisSettings`

与 [architecture.md](./architecture.md) §9 YAML 字段一一对应；提供 `InitLedisConfigVars` / `GetLedisSettings()`。

---

## 与 LemoNettyCore 的边界

Session 层 **允许** include：

- `lemo/server/tcp_server.h`
- `lemo/socket/socket.h`
- `lemo/buffer/chain_buffer.h`
- `lemo/io/runtime.h`

Store / Protocol / Command 层 **禁止** include 上述头文件。

---

## 测试契约

每个模块至少一个 `tests/ledis/<module>/test_*.cc`，CTest 名：

```
Ledis.protocol.test_resp_reader
Ledis.store.test_keyspace
Ledis.command.test_ping
Ledis.integration.redis_cli_ping
```
