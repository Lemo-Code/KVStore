#ifndef __SYLAR_DB_REDIS_CONN_H__
#define __SYLAR_DB_REDIS_CONN_H__

#include "redis_client.h"
#include "sylar/mutex.h"
#include "sylar/iomanager.h"
#include "sylar/timer.h"
#include "sylar/lexicalcast.h"
#include "sylar/config.h"
#include <condition_variable>
#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <sstream>

namespace sylar {
namespace db {

// Redis 单实例/连接池配置（支持多实例，每实例一个连接池）
struct RedisConf {
    std::string host = "127.0.0.1";
    uint32_t port = 6379;
    std::string password = "";
    size_t poolSize = 10;
    uint64_t timeout_ms = 5000;             // 命令读写超时(毫秒)
    uint64_t connect_timeout_ms = 5000;     // 建连超时(毫秒)，0 时用 timeout_ms
    uint64_t heartBeatTime = 30 * 1000;     // 心跳间隔(毫秒)
    int db_index = 0;                       // 建连后 SELECT 的库索引

    bool validate_before_borrow = false;    // 取连接前是否 Ping 校验
    uint64_t get_conn_wait_ms = 0;          // 池空时最大等待(毫秒)，0=立即失败

    bool operator==(const RedisConf& other) const {
        return host == other.host && port == other.port && password == other.password
            && poolSize == other.poolSize && timeout_ms == other.timeout_ms
            && connect_timeout_ms == other.connect_timeout_ms && heartBeatTime == other.heartBeatTime
            && db_index == other.db_index && validate_before_borrow == other.validate_before_borrow
            && get_conn_wait_ms == other.get_conn_wait_ms;
    }
    bool isValid() const { return !host.empty(); }
};

typedef std::map<std::string, RedisConf> RedisConfMap;

} // namespace db

// LexicalCast 特化（sylar 命名空间）
template<>
class LexicalCast<std::string, sylar::db::RedisConf> {
public:
    sylar::db::RedisConf operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        sylar::db::RedisConf conf;
        if (node["host"].IsDefined()) conf.host = node["host"].as<std::string>();
        if (node["port"].IsDefined()) conf.port = node["port"].as<uint32_t>();
        if (node["password"].IsDefined()) conf.password = node["password"].as<std::string>();
        if (node["poolSize"].IsDefined()) conf.poolSize = node["poolSize"].as<size_t>();
        if (node["timeout_ms"].IsDefined()) conf.timeout_ms = node["timeout_ms"].as<uint64_t>();
        if (node["connect_timeout_ms"].IsDefined()) conf.connect_timeout_ms = node["connect_timeout_ms"].as<uint64_t>();
        if (node["heartBeatTime"].IsDefined()) conf.heartBeatTime = node["heartBeatTime"].as<uint64_t>();
        if (node["db_index"].IsDefined()) conf.db_index = node["db_index"].as<int>();
        if (node["validate_before_borrow"].IsDefined()) conf.validate_before_borrow = node["validate_before_borrow"].as<bool>();
        if (node["get_conn_wait_ms"].IsDefined()) conf.get_conn_wait_ms = node["get_conn_wait_ms"].as<uint64_t>();
        if (conf.connect_timeout_ms == 0) conf.connect_timeout_ms = conf.timeout_ms;
        return conf;
    }
};

template<>
class LexicalCast<sylar::db::RedisConf, std::string> {
public:
    std::string operator()(const sylar::db::RedisConf& v) {
        YAML::Node node;
        node["host"] = v.host;
        node["port"] = v.port;
        node["password"] = v.password;
        node["poolSize"] = v.poolSize;
        node["timeout_ms"] = v.timeout_ms;
        node["connect_timeout_ms"] = v.connect_timeout_ms;
        node["heartBeatTime"] = v.heartBeatTime;
        node["db_index"] = v.db_index;
        node["validate_before_borrow"] = v.validate_before_borrow;
        node["get_conn_wait_ms"] = v.get_conn_wait_ms;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<>
class LexicalCast<std::string, sylar::db::RedisConfMap> {
public:
    sylar::db::RedisConfMap operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        sylar::db::RedisConfMap confMap;
        if (node.IsMap()) {
            for (auto it = node.begin(); it != node.end(); ++it) {
                std::string name = it->first.as<std::string>();
                std::stringstream ss;
                ss << it->second;
                confMap[name] = LexicalCast<std::string, sylar::db::RedisConf>()(ss.str());
            }
        } else {
            std::stringstream ss;
            ss << node;
            confMap["default"] = LexicalCast<std::string, sylar::db::RedisConf>()(ss.str());
        }
        return confMap;
    }
};

template<>
class LexicalCast<sylar::db::RedisConfMap, std::string> {
public:
    std::string operator()(const sylar::db::RedisConfMap& v) {
        YAML::Node node;
        for (const auto& pair : v) {
            node[pair.first] = YAML::Load(LexicalCast<sylar::db::RedisConf, std::string>()(pair.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

namespace db {

/**
 * @brief Redis 连接类（基于 RedisClient 封装）
 * 提供更简单的接口，支持原始命令执行和结果获取
 */
class RedisConn {
public:
    RedisConn();
    ~RedisConn();

    /**
     * @brief 连接到 Redis 服务器（使用默认超时）
     * @param db_index 建连后自动 SELECT 的库，必须显式传入，避免误用默认库
     */
    bool Connect(const std::string& host, uint32_t port, const std::string& password,
                 int db_index);

    /**
     * @brief 连接到 Redis 服务器（指定超时，供连接池使用）
     * @param timeout_ms 命令读写超时(毫秒)
     * @param connect_timeout_ms 建连超时(毫秒)，0 表示使用 timeout_ms
     * @param db_index 建连后自动 SELECT 的库，必须显式传入
     */
    bool Connect(const std::string& host, uint32_t port, const std::string& password,
                 uint64_t timeout_ms, uint64_t connect_timeout_ms,
                 int db_index);

    /**
     * @brief 选择 DB 索引（建连后调用）
     */
    bool SelectDb(int db_index);

    /**
     * @brief 断开连接
     */
    void Disconnect();

    /**
     * @brief 检查连接是否有效
     */
    bool isConnected() const;

    /**
     * @brief Ping 服务器
     */
    bool Ping();

    /**
     * @brief 执行原始 Redis 命令（字符串格式，如 "SET key value"）
     * @param cmd 命令字符串
     * @return 是否执行成功（不检查结果，只检查命令是否发送成功）
     */
    bool Command(const std::string& cmd);

    /**
     * @brief 执行原始 Redis 命令（格式化字符串，类似 printf）
     * @param format 格式化字符串
     * @param ... 参数
     * @return 是否执行成功
     */
    bool Command(const char* format, ...) __attribute__((format(printf, 2, 3)));

    /**
     * @brief 执行原始 Redis 命令（参数数组）
     * @param argc 参数数量
     * @param argv 参数数组
     * @return 是否执行成功
     */
    bool CommandArgv(int argc, const char** argv);

    /**
     * @brief 获取上一次命令的字符串结果
     * @return 字符串结果（如果是错误则返回错误信息）
     */
    std::string getStringResult();

    /**
     * @brief 获取上一次命令的数组结果
     * @return 字符串数组结果
     */
    std::vector<std::string> getArrayResult();

    /**
     * @brief 获取错误信息
     */
    std::string getError() const;

    /**
     * @brief 获取错误信息（别名）
     */
    std::string error() const { return getError(); }

    /**
     * @brief 获取错误码
     */
    int getErrorCode() const;

    // ==================== 字符串操作 ====================
    /** 使用连接当前库（Connect 时指定的 db_index） */
    bool Set(const std::string& key, const std::string& value, int64_t expireTime = 0);
    /** 指定本次命令使用的库，必须显式传入 db */
    bool Set(const std::string& key, const std::string& value, int64_t expireTime, int db);
    /** 使用连接当前库 */
    bool Get(const std::string& key, std::string& value);
    /** 指定本次命令使用的库，必须显式传入 db */
    bool Get(const std::string& key, std::string& value, int db);
    int64_t Del(const std::string& key);
    int64_t Del(const std::vector<std::string>& keys);
    int64_t Exists(const std::string& key);
    bool Expire(const std::string& key, int64_t seconds);
    bool Pexpire(const std::string& key, int64_t milliseconds);
    int64_t TTL(const std::string& key);
    bool Keys(const std::string& pattern, std::vector<std::string>& keys);
    bool MGet(const std::vector<std::string>& keys, std::vector<std::string>& values);
    bool MSet(const std::map<std::string, std::string>& kvs);
    int64_t Incr(const std::string& key);
    int64_t IncrBy(const std::string& key, int64_t delta);
    int64_t Decr(const std::string& key);
    int64_t DecrBy(const std::string& key, int64_t delta);
    bool GetSet(const std::string& key, const std::string& value, std::string& oldValue);

    // ==================== Hash 操作 ====================
    bool HSet(const std::string& key, const std::string& field, 
              const std::string& value, int64_t& changed);
    bool HSet(const std::string& key, const std::string& field, const std::string& value);
    bool HGet(const std::string& key, const std::string& field, std::string& value);
    int64_t HDel(const std::string& key, const std::string& field);
    int64_t HExists(const std::string& key, const std::string& field);
    bool HGetAll(const std::string& key, std::map<std::string, std::string>& result);
    bool HKeys(const std::string& key, std::vector<std::string>& fields);
    bool HVals(const std::string& key, std::vector<std::string>& values);
    int64_t HLen(const std::string& key);

    // ==================== List 操作 ====================
    bool LPush(const std::string& key, const std::string& element, int64_t& len);
    bool RPush(const std::string& key, const std::string& element, int64_t& len);
    bool LPop(const std::string& key, std::string& element);
    bool RPop(const std::string& key, std::string& element);
    int64_t LLen(const std::string& key);
    bool LRange(const std::string& key, int64_t start, int64_t stop, 
                std::vector<std::string>& elements);
    bool LIndex(const std::string& key, int64_t index, std::string& element);

    // ==================== Set 操作 ====================
    bool SAdd(const std::string& key, const std::string& member, int64_t& added);
    int64_t SRem(const std::string& key, const std::string& member);
    int64_t SIsMember(const std::string& key, const std::string& member);
    bool SMembers(const std::string& key, std::vector<std::string>& members);
    int64_t SCard(const std::string& key);

    // ==================== ZSet 操作 ====================
    bool ZAdd(const std::string& key, double score, const std::string& member, int64_t& added);
    int64_t ZRem(const std::string& key, const std::string& member);
    bool ZScore(const std::string& key, const std::string& member, double& score);
    bool ZRange(const std::string& key, int64_t start, int64_t stop, 
                std::vector<std::string>& members);
    bool ZRangeWithScores(const std::string& key, int64_t start, int64_t stop,
                          std::vector<std::pair<double, std::string>>& out);
    bool ZRevRange(const std::string& key, int64_t start, int64_t stop, 
                   std::vector<std::string>& members);
    bool ZRevRangeWithScores(const std::string& key, int64_t start, int64_t stop,
                             std::vector<std::pair<double, std::string>>& out);
    bool ZIncrBy(const std::string& key, double delta, const std::string& member, double& newScore);
    int64_t ZCard(const std::string& key);

private:
    /**
     * @brief 解析命令字符串为命令和参数
     */
    std::pair<std::string, std::vector<std::string>> parseCommand(const std::string& cmd);

    /**
     * @brief 执行命令并保存结果
     */
    bool executeAndStoreResult(const std::string& cmd, const std::vector<std::string>& args);

    /** 若 db >= 0 且与当前库不同则先 SELECT，保证后续命令在该库执行 */
    bool ensureDb(int db);

private:
    std::unique_ptr<RedisClient> client_;      // Redis 客户端
    RedisClient::Reply lastReply_;              // 最后一次命令的响应
    int currentDb_ = 0;                         // 当前库，Connect/SelectDb 后更新
};

/**
 * @brief Redis 连接池（一池对应一个 DB 实例，仅由 RedisConnPoolMgr 管理）
 * 外部应通过 RedisConnPoolMgr::GetInstance()->getPool(name) 或 getConn(name) 获取池/连接，不要直接构造池。
 */
class RedisConnPool : public std::enable_shared_from_this<RedisConnPool> {
public:
    /**
     * @brief 连接包装类（RAII），出作用域自动归还
     */
    class Connection {
    public:
        Connection(std::shared_ptr<RedisConn> conn, RedisConnPool* pool)
            : conn_(conn), pool_(pool) {}
        ~Connection() {
            if (pool_ && conn_) pool_->ReturnConn(conn_);
        }
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;
        Connection(Connection&&) = default;
        Connection& operator=(Connection&&) = default;

        RedisConn* operator->() { return conn_.get(); }
        RedisConn& operator*() { return *conn_; }
        std::shared_ptr<RedisConn> get() { return conn_; }
        operator bool() const { return conn_ != nullptr; }

    private:
        std::shared_ptr<RedisConn> conn_;
        RedisConnPool* pool_;
    };

    RedisConnPool(const std::string& name = "default");
    ~RedisConnPool();

    /** 从配置初始化（根据 name 读取 redis 配置） */
    bool init();
    /** 使用指定参数初始化（兼容旧代码） */
    bool init(const std::string& host, uint32_t port, const std::string& password,
              size_t poolSize = 10, uint64_t timeout_ms = 5000, uint64_t heartBeatTime = 30 * 1000);

    const std::string& getName() const { return name_; }

    /** 从池获取连接（RAII）；若配置了 get_conn_wait_ms 则等待，validate_before_borrow 则取前 Ping */
    Connection GetConn();

    /** 归还连接（内部使用） */
    void ReturnConn(std::shared_ptr<RedisConn> conn);

    size_t getSize() const;

    /** 等待池就绪。min_conn_count 为至少需要的可用连接数（0 表示至少 1 个），超时返回 false */
    bool waitForReady(uint64_t timeout_ms = 30000, size_t min_conn_count = 0);

    void Close();

private:
    void createConn();
    void heartbeatTask();
    /** 内部使用：等待至少 min_conn_count 条连接就绪（不要求 initialized_），用于 init 完成前同步 */
    bool waitForConnectionsReady(uint64_t timeout_ms, size_t min_conn_count);

private:
    std::string name_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    std::vector<std::shared_ptr<RedisConn>> available_;
    std::string host_;
    uint32_t port_;
    std::string password_;
    uint64_t timeout_ms_;
    uint64_t connect_timeout_ms_;
    int db_index_;
    bool validate_before_borrow_;
    uint64_t get_conn_wait_ms_;
    bool initialized_ = false;
    size_t maxSize_ = 0;
    uint64_t heartbeat_id_;  // DBWorker 心跳句柄，Close 时取消
    uint64_t heartBeatTime_ = 30 * 1000;
    bool isStop_ = false;
};

/** Redis 连接池管理器（外部唯一入口：按 db 名称操作池、获取连接） */
class RedisConnPoolMgr {
public:
    typedef std::shared_ptr<RedisConnPool> RedisConnPoolPtr;
    typedef RedisConnPool::Connection ConnGuard;

    static RedisConnPoolMgr* GetInstance();

    /** 按 db 名称获取连接池（一池对应一个 db，不存在则从配置创建） */
    RedisConnPoolPtr getPool(const std::string& name = "default");

    /** 按 db 名称获取连接（RAII），等价于 getPool(name)->GetConn() */
    ConnGuard getConn(const std::string& name = "default");

    bool initAll();
    bool initPool(const std::string& name);
    void closeAll();
    void closePool(const std::string& name);
    std::vector<std::string> getAllPoolNames() const;

private:
    RedisConnPoolMgr() = default;
    ~RedisConnPoolMgr() { closeAll(); }
    RedisConnPoolMgr(const RedisConnPoolMgr&) = delete;
    RedisConnPoolMgr& operator=(const RedisConnPoolMgr&) = delete;

    mutable sylar::RWMutex mutex_;
    std::map<std::string, RedisConnPoolPtr> pools_;
};

} // namespace db
} // namespace sylar

#endif // __SYLAR_DB_REDIS_CONN_H__

