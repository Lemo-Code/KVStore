# MySQL / Redis 使用说明

统一通过 **PoolMgr** 按 db 名称获取连接池或连接，一池对应一个 DB。配置来自 `db.yml`（key 与 name 一一对应）。

**设计保证**：建连与心跳由进程内 **DBWorker** 单线程执行，不依赖 IOManager/协程；Hook 在无调度器时自动退化为同步（connect 用 `connect_f`，read/write 用循环重试）。因此 **getPool / getConn 可在任意线程、任意时刻调用**，不会因调度器上下文出错。

**阻塞语义**：  
- **建连**：仅在 DBWorker 后台线程中执行，不阻塞用户线程。  
- **取到连接后的 Query/Get/Set**：若在**有 IOManager 的协程**里调用，Redis 的读写仍是“协程挂起、线程不阻塞”；若在**主线程或无 Scheduler 的线程**里调用，则退化为**同步阻塞**（MySQL 本身为同步 API；Redis 经 hook 退化后也为同步阻塞），保证安全不崩。

---

## 一、配置（db.yml）

在 `bin/conf/db.yml` 中配置 `mysql` 和 `redis`，名称（如 `default`、`user_db`）即后续 `getPool(name)` / `getConn(name)` 的 `name`。

```yaml
mysql:
  default:
    host: "127.0.0.1"
    port: 3306
    user: root
    passwd: ""
    dbname: ""
    connSize: 256
    heartBeatTime: 300000
    charset: "utf8mb4"
    connect_timeout_sec: 10
    read_timeout_sec: 60
    write_timeout_sec: 60
    init_sql: ""
    validate_before_borrow: true
    get_conn_wait_ms: 3000

redis:
  default:
    host: "127.0.0.1"
    port: 6379
    password: ""
    poolSize: 10
    timeout_ms: 5000
    connect_timeout_ms: 5000
    heartBeatTime: 30000
    db_index: 0
    validate_before_borrow: false
    get_conn_wait_ms: 0
```

加载配置（通常在程序启动时）：

```cpp
#include "sylar/env.h"
#include "sylar/config.h"
sylar::EnvMgr::GetInstance()->init(argc, argv);
sylar::Config::LoadFromConfDir(sylar::EnvMgr::GetInstance()->getConfigPath());
```

---

## 二、MySQL 使用

### 2.1 头文件

```cpp
#include "sylar/db/mysql_conn.h"
```

### 2.2 按 db 名称获取连接（推荐）

```cpp
auto conn = sylar::db::MysqlConnPoolMgr::GetInstance()->getConn("default");
if (!conn) {
    // 无可用连接或池未就绪
    return;
}
// 使用 conn
conn->Query("SELECT 1");
sylar::db::MysqlRow row = conn->getRowResult();
// conn 为 shared_ptr，出作用域自动归还；也可手动提前置空
```

### 2.3 按 db 名称获取连接池（需多次取连接或访问池接口时）

```cpp
auto pool = sylar::db::MysqlConnPoolMgr::GetInstance()->getPool("default");
if (!pool) {
    return;
}
std::shared_ptr<sylar::db::MysqlConn> conn = pool->GetConn();
if (conn) {
    conn->Query("SELECT id, name FROM user LIMIT 1");
    // ...
}
// 归还：conn 析构时会通过 deleter 自动归还，或显式 pool->ReturnConn(conn);
```

### 2.4 常用连接接口

```cpp
conn->Query("SELECT ...");           // 查询，结果通过 getRowResult/getAllRows 等取
conn->Execute("INSERT/UPDATE...");   // 更新
conn->getRowCount();                 // 结果行数
conn->getAffectedRows();             // 影响行数
conn->getLastInsertId();             // 最后插入 ID
conn->getRowResult();                // 当前行（MysqlRow）
conn->getAllRows();                  // 所有行（MysqlRows）
conn->queryValue(sql, value);        // 取第一行第一列
conn->queryRow(sql, row);            // 取第一行
conn->queryInt64(sql, val);          // 取第一列转 int64
conn->beginTransaction();            // 事务
conn->commit(); conn->rollback();
conn->ping();                        // 检查连接
```

### 2.5 预初始化所有配置中的 MySQL 池（可选）

```cpp
sylar::db::MysqlConnPoolMgr::GetInstance()->initAll();
// 之后 getPool(name) / getConn(name) 不会触发懒创建，直接取已有池
```

---

## 三、Redis 使用

### 3.1 头文件

```cpp
#include "sylar/db/redis_conn.h"
```

### 3.2 按 db 名称获取连接（推荐，RAII 自动归还）

```cpp
auto conn = sylar::db::RedisConnPoolMgr::GetInstance()->getConn("default");
if (!conn) {
    return;
}
// conn 为 RedisConnPoolMgr::ConnGuard，可当 RedisConn* 用
conn->Set("key", "value");
std::string val;
conn->Get("key", val);
// 出作用域自动归还，无需手动 ReturnConn
```

### 3.3 按 db 名称获取连接池

```cpp
auto pool = sylar::db::RedisConnPoolMgr::GetInstance()->getPool("default");
if (!pool) return;
auto conn = pool->GetConn();   // RedisConnPool::Connection（RAII）
if (conn) {
    conn->Set("k", "v");
    conn->Get("k", val);
}
// Connection 析构时自动归还
```

### 3.4 常用连接接口

```cpp
conn->Set(key, value); conn->Get(key, value);
conn->Del(key); conn->Exists(key);
conn->Expire(key, sec); conn->Pexpire(key, ms);
conn->MGet(keys, values); conn->MSet(kvs);
conn->Incr(key); conn->IncrBy(key, delta); conn->Decr(key); conn->DecrBy(key, delta);
conn->GetSet(key, value, oldValue);
conn->HSet(key, field, value); conn->HGet(key, field, value);
conn->ZRangeWithScores(key, start, stop, out);
conn->ZIncrBy(key, score, member);
conn->Command("SET key value");   // 原始命令
conn->Ping();
```

### 3.5 预初始化所有配置中的 Redis 池（可选）

```cpp
sylar::db::RedisConnPoolMgr::GetInstance()->initAll();
```

---

## 四、小结

| 操作           | MySQL | Redis |
|----------------|--------|--------|
| 获取连接       | `MysqlConnPoolMgr::GetInstance()->getConn("default")` 得到 `shared_ptr<MysqlConn>` | `RedisConnPoolMgr::GetInstance()->getConn("default")` 得到 RAII `ConnGuard` |
| 获取池         | `MysqlConnPoolMgr::GetInstance()->getPool("default")` | `RedisConnPoolMgr::GetInstance()->getPool("default")` |
| 名称与配置对应 | `db.yml` 中 `mysql.default`、`mysql.user_db` 等 | `db.yml` 中 `redis.default`、`redis.cache` 等 |

约定：**仅通过 PoolMgr 按名称取池/取连接，一池对应一个 DB；配置存在时仅使用 db.yml 中的配置项。**
