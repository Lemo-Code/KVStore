#ifndef __SYLAR_DB_MYSQL_CONN_H__
#define __SYLAR_DB_MYSQL_CONN_H__

#include <memory>
#include <queue>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <mysql/mysql.h>
#include "sylar/mutex.h"
#include "sylar/iomanager.h"
#include "sylar/lexicalcast.h"
#include "sylar/config.h"

namespace sylar {
namespace db {

// MySQL 单个数据库/连接池配置结构体（支持多库，每库一个连接池）
struct MysqlConf {
    std::string host = "localhost";
    uint32_t port = 3306;
    std::string user = "root";
    std::string passwd = "";
    std::string dbname = "";
    size_t connSize = 256;
    uint64_t heartBeatTime = 5 * 60 * 1000;  // 心跳间隔(毫秒)，默认5分钟

    std::string charset = "utf8mb4";
    uint32_t connect_timeout_sec = 10;
    uint32_t read_timeout_sec = 60;
    uint32_t write_timeout_sec = 60;
    std::string init_sql = "";  // 建连后执行，如 "SET SESSION sql_mode='STRICT_TRANS_TABLES'"

    bool validate_before_borrow = true;   // 取连接前是否 ping 校验
    uint64_t get_conn_wait_ms = 3000;     // 池空时最大等待时间(毫秒)，0=立即失败

    bool operator==(const MysqlConf& other) const {
        return host == other.host && port == other.port && user == other.user
            && passwd == other.passwd && dbname == other.dbname
            && connSize == other.connSize && heartBeatTime == other.heartBeatTime
            && charset == other.charset && connect_timeout_sec == other.connect_timeout_sec
            && read_timeout_sec == other.read_timeout_sec && write_timeout_sec == other.write_timeout_sec
            && init_sql == other.init_sql && validate_before_borrow == other.validate_before_borrow
            && get_conn_wait_ms == other.get_conn_wait_ms;
    }

    bool isValid() const {
        return !host.empty() && !user.empty();
    }
};

// MySQL 多数据库配置（支持分库）：map<名称, MysqlConf>
typedef std::map<std::string, MysqlConf> MysqlConfMap;

} // namespace db

// LexicalCast 特化在 sylar 命名空间
template<>
class LexicalCast<std::string, sylar::db::MysqlConf> {
public:
    sylar::db::MysqlConf operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        sylar::db::MysqlConf conf;
        if (node["host"].IsDefined()) conf.host = node["host"].as<std::string>();
        if (node["port"].IsDefined()) conf.port = node["port"].as<uint32_t>();
        if (node["user"].IsDefined()) conf.user = node["user"].as<std::string>();
        if (node["passwd"].IsDefined()) conf.passwd = node["passwd"].as<std::string>();
        if (node["dbname"].IsDefined()) conf.dbname = node["dbname"].as<std::string>();
        if (node["connSize"].IsDefined()) conf.connSize = node["connSize"].as<size_t>();
        if (node["heartBeatTime"].IsDefined()) conf.heartBeatTime = node["heartBeatTime"].as<uint64_t>();
        if (node["charset"].IsDefined()) conf.charset = node["charset"].as<std::string>();
        if (node["connect_timeout_sec"].IsDefined()) conf.connect_timeout_sec = node["connect_timeout_sec"].as<uint32_t>();
        if (node["read_timeout_sec"].IsDefined()) conf.read_timeout_sec = node["read_timeout_sec"].as<uint32_t>();
        if (node["write_timeout_sec"].IsDefined()) conf.write_timeout_sec = node["write_timeout_sec"].as<uint32_t>();
        if (node["init_sql"].IsDefined()) conf.init_sql = node["init_sql"].as<std::string>();
        if (node["validate_before_borrow"].IsDefined()) conf.validate_before_borrow = node["validate_before_borrow"].as<bool>();
        if (node["get_conn_wait_ms"].IsDefined()) conf.get_conn_wait_ms = node["get_conn_wait_ms"].as<uint64_t>();
        return conf;
    }
};

template<>
class LexicalCast<sylar::db::MysqlConf, std::string> {
public:
    std::string operator()(const sylar::db::MysqlConf& v) {
        YAML::Node node;
        node["host"] = v.host;
        node["port"] = v.port;
        node["user"] = v.user;
        node["passwd"] = v.passwd;
        node["dbname"] = v.dbname;
        node["connSize"] = v.connSize;
        node["heartBeatTime"] = v.heartBeatTime;
        node["charset"] = v.charset;
        node["connect_timeout_sec"] = v.connect_timeout_sec;
        node["read_timeout_sec"] = v.read_timeout_sec;
        node["write_timeout_sec"] = v.write_timeout_sec;
        if (!v.init_sql.empty()) node["init_sql"] = v.init_sql;
        node["validate_before_borrow"] = v.validate_before_borrow;
        node["get_conn_wait_ms"] = v.get_conn_wait_ms;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<>
class LexicalCast<std::string, sylar::db::MysqlConfMap> {
public:
    sylar::db::MysqlConfMap operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        sylar::db::MysqlConfMap confMap;
        if (node.IsMap()) {
            for (auto it = node.begin(); it != node.end(); ++it) {
                std::string name = it->first.as<std::string>();
                std::stringstream ss;
                ss << it->second;
                confMap[name] = LexicalCast<std::string, sylar::db::MysqlConf>()(ss.str());
            }
        } else {
            std::stringstream ss;
            ss << node;
            confMap["default"] = LexicalCast<std::string, sylar::db::MysqlConf>()(ss.str());
        }
        return confMap;
    }
};

template<>
class LexicalCast<sylar::db::MysqlConfMap, std::string> {
public:
    std::string operator()(const sylar::db::MysqlConfMap& v) {
        YAML::Node node;
        for (const auto& pair : v) {
            node[pair.first] = YAML::Load(LexicalCast<sylar::db::MysqlConf, std::string>()(pair.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

namespace db {

// MySQL 查询结果行（字段名到字段值的映射）
typedef std::map<std::string, std::string> MysqlRow;
// MySQL 查询结果集（多行）
typedef std::vector<MysqlRow> MysqlRows;

// MySQL 单个连接类
class MysqlConn {
public:
    MysqlConn();
    ~MysqlConn();

    // 连接到MySQL服务器
    bool Connect(const std::string& host = "localhost",
                 uint32_t port = 3306,
                 const std::string& user = "root",
                 const std::string& passwd = "123456789",
                 const std::string& dbname = "");

    // 连接到MySQL服务器（可指定超时与字符集，供连接池使用）
    bool Connect(const std::string& host, uint32_t port,
                 const std::string& user, const std::string& passwd, const std::string& dbname,
                 unsigned int connect_timeout_sec, unsigned int read_timeout_sec, unsigned int write_timeout_sec,
                 const std::string& charset = "utf8mb4");

    // 断开连接
    void Disconnect();

    // 选择数据库
    bool SelectDB(const std::string& dbname);

    // 执行SQL查询（SELECT等返回结果集的查询）
    bool Query(const std::string& sql);

    // 执行SQL更新（INSERT, UPDATE, DELETE等不返回结果集的查询）
    bool Execute(const std::string& sql);

    // 获取查询结果的行数
    uint64_t getRowCount() const;

    // 获取受影响的行数（INSERT, UPDATE, DELETE）
    uint64_t getAffectedRows() const;

    // 获取最后插入的自增ID
    uint64_t getLastInsertId() const;

    // 获取一行结果（字段名到字段值的映射）
    // 需要先调用 Query() 执行查询
    MysqlRow getRowResult();

    // 获取所有结果（返回所有行的结果集）
    // 需要先调用 Query() 执行查询
    MysqlRows getAllRows();

    // 获取单个字段值（当前行的指定字段）
    std::string getFieldValue(const std::string& fieldName);

    // 获取单个字段值（通过索引）
    std::string getFieldValue(size_t index);

    // 移动到结果集的下一行
    bool next();

    // 移动到结果集的第一行
    bool first();

    // 释放结果集
    void freeResult();

    // 获取错误信息
    std::string getError() const;

    // 获取错误码
    unsigned int getErrorCode() const;

    // 检查连接是否有效
    bool isConnected() const;

    // 开启事务
    bool beginTransaction();

    // 提交事务
    bool commit();

    // 回滚事务
    bool rollback();

    // 设置自动提交模式
    bool setAutoCommit(bool autoCommit);

    // 设置字符集
    bool setCharset(const std::string& charset = "utf8mb4");

    // 转义字符串（防止SQL注入）
    std::string escapeString(const std::string& str);

    // 获取MySQL服务器信息
    std::string getServerInfo() const;

    // 获取MySQL客户端信息
    std::string getClientInfo() const;

    // Ping服务器（检查连接是否活跃）
    bool ping();

    // 获取当前数据库名（注意：此方法会执行查询，建议缓存结果）
    std::string getCurrentDB();

    // 执行多条SQL语句（用分号分隔）
    bool executeBatch(const std::string& sql);

    // ==================== 结果集元信息 ====================
    // 获取列数（需在 Query 之后调用）
    uint32_t getFieldCount() const { return fieldCount_; }
    // 获取列名（index 从 0 开始）
    std::string getFieldName(size_t index) const;

    // ==================== 便捷查询（执行 SQL 并取结果） ====================
    // 执行 SELECT 取第一行第一列，成功返回 true
    bool queryValue(const std::string& sql, std::string& value);
    // 执行 SELECT 取第一行，成功返回 true
    bool queryRow(const std::string& sql, MysqlRow& row);
    // 执行 SELECT 取第一行第一列并转为整数
    bool queryInt64(const std::string& sql, int64_t& value);
    bool queryUint64(const std::string& sql, uint64_t& value);

private:
    MYSQL* mysql_;              // MySQL连接句柄
    MYSQL_RES* result_;         // 查询结果集
    MYSQL_ROW row_;             // 当前行
    MYSQL_FIELD* fields_;       // 字段信息
    unsigned long* lengths_;    // 字段长度
    uint32_t fieldCount_;       // 字段数量
    bool connected_;            // 是否已连接
    std::string lastError_;     // 最后错误信息
    unsigned int lastErrorCode_; // 最后错误码
};

// MySQL 连接池类（一池对应一个 DB，仅由 MysqlConnPoolMgr 管理）
// 外部应通过 MysqlConnPoolMgr::GetInstance()->getPool(name) 或 getConn(name) 获取池/连接
class MysqlConnPool: public std::enable_shared_from_this<MysqlConnPool> {
public:
    typedef std::queue<std::shared_ptr<MysqlConn>> MysqlConnQueue;

    // 构造函数（按名称从配置加载，name 对应 db.yml 中 mysql.xxx）
    MysqlConnPool(const std::string& name = "default");

    ~MysqlConnPool();

    // 从配置系统初始化连接池（根据 name_ 读取 mysql 配置）
    bool init();

    // 使用指定参数初始化连接池（兼容旧代码）
    bool init(const std::string& host,
              uint32_t port,
              const std::string& user,
              const std::string& passwd,
              const std::string& dbname,
              size_t connSize = 256,
              uint64_t heartBeatTime = 60 * 1000);

    const std::string& getName() const { return name_; }

    // 获取连接（自动归还，通过智能指针释放器）
    std::shared_ptr<MysqlConn> GetConn();
    
    // 主动归还连接
    void ReturnConn(std::shared_ptr<MysqlConn> conn);

    // 获取当前连接池中的连接数
    size_t getSize();

    // 等待连接池就绪（至少有一个可用连接）
    // @param timeout_ms 超时时间（毫秒），默认30秒
    // @return 是否就绪
    bool waitForReady(uint64_t timeout_ms = 30000);

    // 关闭连接池
    void Close();

private:
    // 创建新连接
    void CreateConn();
    
    // 心跳检测任务（在协程中执行）
    void heartbeatTask();

    // 归还连接到池（内部使用）
    void ReturnOwner(const std::shared_ptr<MysqlConn>& conn);

    /** 内部使用：等待至少 1 条连接就绪（不要求 isInited_），用于 init 完成前同步 */
    bool waitForConnectionsReady(uint64_t timeout_ms);

private:
    std::string name_;
    std::string host_;
    uint32_t port_;
    std::string user_;
    std::string passwd_;
    std::string dbname_;
    std::string charset_;
    unsigned int connect_timeout_sec_;
    unsigned int read_timeout_sec_;
    unsigned int write_timeout_sec_;
    std::string init_sql_;
    bool validate_before_borrow_;
    uint64_t get_conn_wait_ms_;

    MysqlConnQueue conns_;
    size_t connSize_;
    std::mutex mtx_;
    std::condition_variable cond_;

    uint64_t heartBeatTime_;
    uint64_t heartbeat_id_;  // DBWorker 心跳句柄，Close 时取消
    bool isStop_;
    bool isInited_;
};

// MySQL 连接池管理器（外部唯一入口：按 db 名称操作池、获取连接）
class MysqlConnPoolMgr {
public:
    typedef std::shared_ptr<MysqlConnPool> MysqlConnPoolPtr;

    static MysqlConnPoolMgr* GetInstance();

    /** 按 db 名称获取连接池（一池对应一个 db，不存在则从配置创建），name 对应 db.yml 中 mysql.xxx */
    MysqlConnPoolPtr getPool(const std::string& name = "default");

    /** 按 db 名称获取连接，等价于 getPool(name)->GetConn() */
    std::shared_ptr<MysqlConn> getConn(const std::string& name = "default");

    // 根据配置初始化所有连接池
    bool initAll();

    bool initPool(const std::string& name);
    void closeAll();
    void closePool(const std::string& name);
    std::vector<std::string> getAllPoolNames() const;

private:
    MysqlConnPoolMgr() = default;
    ~MysqlConnPoolMgr() { closeAll(); }
    MysqlConnPoolMgr(const MysqlConnPoolMgr&) = delete;
    MysqlConnPoolMgr& operator=(const MysqlConnPoolMgr&) = delete;

    mutable sylar::RWMutex mutex_;
    std::map<std::string, MysqlConnPoolPtr> pools_;
};

} // namespace db
} // namespace sylar

#endif

