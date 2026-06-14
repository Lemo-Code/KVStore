# Ledis 协议约定（RESP2）

> **状态**：已约定（Phase 0）  
> **版本**：RESP2（Redis 2.x 起通用协议）  
> **参考**：[Redis Protocol spec](https://redis.io/docs/reference/protocol-spec/)（官方，Ledis 对齐其子集）

本文是 Ledis **线上字节的唯一权威约定**。实现 `RespReader` / `RespWriter`、集成测试、`redis-cli` 对接均以本文为准。

---

## 1. 总览

| 项 | Ledis 约定 |
|----|------------|
| 传输 | TCP，默认端口 **6379** |
| 连接模型 | **长连接**（见 §1.1） |
| 读缓冲 | **ChainBuffer**（LemoNettyCore，见 §1.2） |
| 字节序 | 无多字节整数；数字均为 **ASCII 十进制** |
| 行结束 | 每行以 `\r\n`（CRLF）结尾 |
| 协议版本 | **RESP2**（Phase 1–7）；RESP3 列为 Phase 9+ |
| 客户端请求 | **数组帧**（`*` 开头）；Inline 命令 Phase 3 可选 |
| 字符串 | **二进制安全**；长度由协议字段给出，不依赖 `\0` |
| 编码 | 不做 UTF-8 校验；按字节透明传输 |

### 1.1 长连接（Persistent Connection）

Redis 客户端与服务端之间是 **TCP 长连接**，不是「一发一收即断」的 HTTP 短连接：

| 行为 | 约定 |
|------|------|
| 连接寿命 | 建立 TCP 后保持，直到客户端 `QUIT`、超时、错误或服务器关闭 |
| 命令复用 | 同一连接上顺序发送 **多条** 命令（Pipeline 无需特殊握手） |
| 半包 / 粘包 | 读侧累积字节流，**增量解析**；不得假设一次 `recv` 等于一帧 |
| 已解析前缀 | 完整帧解析成功后 **`consume`**，释放已读 chunk，避免长连接内存膨胀 |
| Session | 每 TCP 连接 **一个** Session 协程 + **一个** ChainBuffer 实例 |

Ledis Session 主循环（概念）：

```
loop:
  chain.readFd(sock)           // 追加到链尾
  while TryParseOne(chain):    // 从链头窥视/解析
    dispatch(cmd)
    send(response)
  // 未收齐的半包留在 chain 中，等待下次 read
```

### 1.2 读缓冲：ChainBuffer（网络库提供）

协议解析 **不使用** RingBuffer 作为主读缓冲，而使用 LemoNettyCore 的 **`lemo::buffer::ChainBuffer`**：

| 对比 | RingBuffer | ChainBuffer |
|------|------------|-------------|
| 结构 | 单块环形 | 固定大小 chunk 链表 |
| consume 已解析前缀 | head 前移，可能 compact | 丢弃空 head chunk，**无整段 memmove** |
| 长连接 + Pipeline | 可用 | **更适合**（半包挂链尾、前缀释放） |
| RESP 按行 `\r\n` | `find("\r\n")` | `find("\r\n")` 跨 chunk |

`RespReader` **持有** `ChainBuffer&`（或内部成员），接口：

- `append()` / 由 Session 调用 `chain.readFd()`
- `TryParseOne()` 在 chain 上窥视；成功则 `chain.consume(consumed_len)`

`RespWriter` 产出 `std::string` 或 `Sds`，经 `Socket::send` 写出；**写侧不必** ChainBuffer（响应通常短且完整）。

---

## 2. RESP 数据类型

### 2.1 类型一览

| 前缀 | 类型 | 说明 | Ledis 角色 |
|------|------|------|------------|
| `+` | Simple String | 单行，不可含 `\r`/`\n` | OK、PONG、部分状态 |
| `-` | Error | 单行错误 | 所有 `-ERR …` |
| `:` | Integer | 有符号 64 位十进制 | DEL 返回值、TTL、INCR 结果 |
| `$` | Bulk String | 带长度前缀的字节块 | key、value、错误体（少见） |
| `*` | Array | 嵌套 RESP 值 | **客户端命令**、MGET 结果 |
| （特殊） | Null Bulk | `$-1\r\n` | key 不存在时的 GET |

RESP3 类型（`%`、`~`、`|` 等）**不在本文范围**。

---

## 3. 各类型线格式

### 3.1 Simple String `+`

```
+<payload>\r\n
```

- `payload` 不含 `\r` 或 `\n`。
- 示例：`+OK\r\n`、`+PONG\r\n`

**Ledis 使用场景**：`SET` → `+OK\r\n`；`PING` → `+PONG\r\n`（无参数时）。

### 3.2 Error `-`

```
-<error message>\r\n
```

- 与 Simple String 格式相同，语义为错误。
- 消息 **建议** 以 `ERR `、`WRONGTYPE `、`NOAUTH ` 等 Redis 惯用前缀开头（见 §7）。

示例：

```
-ERR unknown command 'FOO'\r\n
-WRONGTYPE Operation against a key holding the wrong kind of value\r\n
```

### 3.3 Integer `:`

```
:<number>\r\n
```

- `<number>` 为有符号十进制整数，对应 C++ `int64_t` 范围。
- 示例：`:0\r\n`、`:1\r\n`、 `:-1\r\n`

**Ledis 使用场景**：`DEL` 删除个数、`EXISTS` 计数、`TTL` 秒数、`DBSIZE` 等。

### 3.4 Bulk String `$`

```
$<len>\r\n
<len bytes of data>\r\n
```

- `<len>` 为 **字节数**（≥0 的十进制整数）。
- 数据后 **必须** 紧跟 `\r\n`（不属于数据内容）。
- **Null bulk**：`$-1\r\n` 表示「空 / 不存在」，**无后续字节**。

示例（`"foo"`，长度 3）：

```
$3\r\n
foo\r\n
```

示例（空串，长度 0）：

```
$0\r\n
\r\n
```

示例（key 不存在）：

```
$-1\r\n
```

**Ledis 使用场景**：`GET` 命中返回值；`GET` 未命中返回 Null bulk；命令参数中的 key/value。

### 3.5 Array `*`

```
*<count>\r\n
<count 个 nested RESP 元素，顺序拼接>
```

- `<count>` ≥ 0：后跟 `count` 个完整 RESP 值（任意类型，可嵌套）。
- `<count>` = 0：空数组，仅 `*0\r\n`。
- **Null array**（RESP2）： `*-1\r\n`（Ledis 服务端 **一般不主动返回**；客户端命令不应发送）。

示例（`GET foo` 命令，2 个 bulk 参数）：

```
*2\r\n
$3\r\n
GET\r\n
$3\r\n
foo\r\n
```

---

## 4. 客户端 → 服务端：命令格式

### 4.1 标准格式（必须支持）

每条命令 **必须** 编码为一个 **Array**，且 **元素全部为 Bulk String**：

```
*<argc>\r\n
$<len0>\r\n
<command name bytes>\r\n
$<len1>\r\n
<arg1 bytes>\r\n
...
```

约定：

| 规则 | 说明 |
|------|------|
| `argv[0]` | 命令名，**大小写不敏感**；Ledis 内部统一转为 **大写** 再查表 |
| `argv[1..]` | 命令参数，二进制安全 |
| `argc` | 等于 bulk 元素个数，≥ 1 |
| 命令名中的空格 | **不允许**（应作为参数分隔到 argv） |

**示例：`SET mykey myvalue`**

```
*3\r\n
$3\r\n
SET\r\n
$5\r\n
mykey\r\n
$7\r\n
myvalue\r\n
```

**示例：`GET mykey`**

```
*2\r\n
$3\r\n
GET\r\n
$5\r\n
mykey\r\n
```

### 4.2 Inline 命令（可选，Phase 3+）

单行文本，空格分隔，以 `\r\n` 或 `\n` 结束，例如：

```
PING\r\n
GET foo\r\n
```

Ledis **Phase 1–2 不实现**；收到非 `*` 首字节时可返回 `-ERR unknown command` 或 `-ERR inline commands not supported`。

### 4.3 Pipeline（必须支持，Phase 2 起）

客户端 **无需等待响应** 即可连续发送多条命令帧。服务端 **按接收顺序** 逐条解析、执行、写回响应。

```
发送:  cmd1_frame + cmd2_frame + cmd3_frame
接收:  resp1       + resp2       + resp3
```

解析器必须在同一读缓冲内 **循环 TryParseOne** 直到数据不足。

### 4.4 非法请求处理

| 情况 | Ledis 行为 |
|------|------------|
| 首字节非 `*`（且非 Inline 阶段） | 返回 Error，不断开（Phase 1）；或断开（配置项，默认不断开） |
| Array 内混用非 Bulk 类型 | `-ERR protocol error` |
| Bulk 声明长度 < 0 且 ≠ -1 | `-ERR protocol error` |
| Bulk 数据未收齐 | 等待更多字节，**不** 部分执行 |
| 整帧超过 `query_buffer_limit` | 关闭连接，可选先发 `-ERR max query buffer size exceeded` |

---

## 5. 服务端 → 客户端：响应映射

每种命令对应 **一种** RESP 响应类型（与 Redis 常用行为对齐）。

### 5.1 按命令分类（Phase 1–3 子集）

| 命令 | 成功响应 | 失败响应 |
|------|----------|----------|
| PING | `+PONG\r\n` | `-ERR …` |
| SET | `+OK\r\n` | `-ERR …` |
| GET（存在） | Bulk string | `-ERR …` |
| GET（不存在） | `$-1\r\n` | `-ERR …` |
| DEL | Integer（删除个数） | `-ERR …` |
| EXISTS | Integer（0/1 或计数） | `-ERR …` |
| SELECT | `+OK\r\n` | `-ERR invalid DB index` |
| DBSIZE | Integer | `-ERR …` |
| EXPIRE / PERSIST | Integer（0/1） | `-ERR …` |
| TTL | Integer（-2 不存在，-1 无过期，≥0 秒） | `-ERR …` |
| FLUSHDB | `+OK\r\n` | `-ERR …` |
| MGET | **Array of bulk**（每项 `$-1` 或 bulk） | `-ERR …` |
| INCR / DECR 等 | Integer | `-ERR not an integer` 等 |

### 5.2 TTL 返回值约定（与 Redis 一致）

| 值 | 含义 |
|----|------|
| `-2` | key 不存在 |
| `-1` | key 存在但未设置过期 |
| `≥ 0` | 剩余生存时间（秒） |

### 5.3 Null 与空串

| 语义 | RESP |
|------|------|
| key 不存在（GET） | `$-1\r\n` |
| 空字符串 value | `$0\r\n\r\n` |

二者 **不可** 混淆。

---

## 6. 解析器约定（实现契约）

### 6.1 增量解析

TCP 流可能 **拆包 / 粘包**。解析器在 **ChainBuffer** 上增量读取（见 §1.2）：

```
chain.readFd(sock)                    // 长连接：追加新字节
loop:
  if TryParseOne(chain, out) == NeedMore: break
  if TryParseOne(chain, out) == Ok:       dispatch(out); continue
  if TryParseOne(chain, out) == ProtocolError: reply error
```

### 6.2 TryParseOne 返回值（建议枚举）

| 值 | 含义 |
|----|------|
| `kNeedMore` | 缓冲区内无完整一帧，等待更多数据 |
| `kOk` | 成功解析一帧，`out` 有效 |
| `kProtocolError` | 格式非法，应回复 `-ERR protocol error` 或断开 |

### 6.3 命令帧 → 内部结构

解析 Array 后，若满足：

- `count >= 1`
- 每个元素均为 Bulk String（含 `$-1` 的 null bulk **不允许** 出现在 argv 中）

则映射为：

```cpp
Command {
  name = uppercase(argv[0])
  args = argv[1..]
}
```

若 `argv[0]` 为 null bulk → `-ERR protocol error`。

### 6.4 编码器约定

| 函数 | 输出 |
|------|------|
| `encodeOk()` | `+OK\r\n` |
| `encodePong()` | `+PONG\r\n` |
| `encodeError(msg)` | `-` + msg + `\r\n`（msg 不含 CRLF） |
| `encodeInteger(n)` | `:` + decimal(n) + `\r\n` |
| `encodeBulk(sds)` | `$` + len + `\r\n` + data + `\r\n` |
| `encodeNullBulk()` | `$-1\r\n` |
| `encodeArray(values)` | `*` + n + `\r\n` + 各元素编码拼接 |

**Bulk 编码**：长度必须是 **字节数**，不是字符数。

---

## 7. 错误消息约定

格式：`-` + `<message>` + `\r\n`，**不加** 第二个 `-`。

### 7.1 标准前缀

| 前缀 | 场景 |
|------|------|
| `ERR ` | 通用错误（未知命令、参数个数、内部错误） |
| `WRONGTYPE ` | 对错误类型 key 执行命令 |
| `NOAUTH ` | 需要 AUTH 但未认证 |
| `LOADING ` | （预留）持久化加载中 |
| `BUSY ` | （预留）阻塞操作冲突 |

### 7.2 常见文案（与 Redis 对齐）

```
-ERR unknown command 'CMD'
-ERR wrong number of arguments for 'get' command
-ERR invalid DB index
-ERR no such key
-ERR not an integer or out of range
-WRONGTYPE Operation against a key holding the wrong kind of value
-NOAUTH Authentication required.
```

Ledis **不强制** 每个标点与 Redis 完全一致，但集成测试以 `redis-cli` 可接受为准；**前缀** 必须一致。

---

## 8. 限制与配置

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `query_buffer_limit` | 1 MiB | 单连接读缓冲 + 未完成帧上限 |
| `bulk_string_max` | 512 MiB | 单个 bulk 声明长度上限（可配置） |
| `argc_max` | 1024 * 1024 | 单命令 argv 个数上限（防 `*99999999`） |

超过限制：**关闭连接**（与 Redis 类似），并尽量先返回 Error。

---

## 9. 认证与多 DB（协议层可见行为）

### 9.1 AUTH

- 客户端：`AUTH <password>` → `*2` bulk array。
- 成功：`+OK\r\n`
- 失败：`-ERR invalid password`（或 `-WRONGPASS invalid username-password pair`，Phase 6 可选对齐 Redis 6）

未 AUTH 时，除 `AUTH`、`PING`（配置可选）外，返回 `-NOAUTH Authentication required.`

### 9.2 SELECT

- 成功：`+OK\r\n`
- 失败：`-ERR invalid DB index`（index < 0 或 ≥ 16）

**注意**：SELECT 不改变协议格式，仅改变后续命令的 DB 上下文。

---

## 10. 测试向量（实现必须通过）

### 10.1 解析：单命令 GET

**输入字节**（十六进制仅作说明，实现用字面量）：

```
*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n
```

**期望 Command**：`name=GET`, `args=["foo"]`

### 10.2 解析：半包

| 步骤 | 输入 | TryParseOne |
|------|------|-------------|
| 1 | `*2\r\n$3\r\n` | kNeedMore |
| 2 | `GET\r\n$3\r\nfo` | kNeedMore |
| 3 | `o\r\n` | kOk |

### 10.3 解析：粘包（Pipeline 两命令）

输入：`GET foo` 帧 + `PING` 帧拼接。

- 第 1 次 TryParseOne → GET
- 第 2 次 TryParseOne → PING
- 第 3 次 → kNeedMore

### 10.4 编码：GET 命中 / 未命中

| 场景 | 期望输出 |
|------|----------|
| value = `"bar"` | `$3\r\nbar\r\n` |
| key 不存在 | `$-1\r\n` |

### 10.5 编码：DEL 删除 2 个 key

```
:2\r\n
```

---

## 11. 分阶段支持范围

| 阶段 | 协议能力 |
|------|----------|
| Phase 1b | 解析/编码 §2–§3；测试向量 §10 |
| Phase 2 | §4.1 命令帧 + §5.1 PING/GET/SET/DEL/EXISTS |
| Phase 3 | §4.3 Pipeline + §5 扩展命令 + §8 限制 |
| Phase 6 | §9 AUTH |
| Phase 9+ | RESP3（另文约定） |

**Phase 1 实现顺序调整**：**先** 按本文实现 `RespReader` / `RespWriter` 与单测；**再** 实现 Sds（Bulk 可先用 `std::string` 过渡，但必须在 Phase 1b 结束前替换为 Sds）。

---

## 12. 与代码模块的对应

| 文档章节 | 代码 |
|----------|------|
| §2–§3 | `ledis/protocol/resp_value.h` |
| §4、§6 | `ledis/protocol/resp_reader.*` |
| §5、§6.4 | `ledis/protocol/resp_writer.*` |
| §7 | `ledis/command/` 产生 Error 文案 |
| §10 | `tests/ledis/protocol/` |

Store / Command **不得** 自行拼接 RESP 字符串；统一经 `RespWriter`。

---

## 13. 参考

- [Redis Protocol specification](https://redis.io/docs/reference/protocol-spec/)
- [layer_contracts.md](./layer_contracts.md) — C++ API
- [implementation_order.md](./implementation_order.md) — 实施顺序
