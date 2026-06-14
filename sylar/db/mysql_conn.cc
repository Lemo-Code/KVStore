#include "./mysql_conn.h"
#include "./db_worker.h"
#include "sylar/log.h"
#include "sylar/config.h"
#include <cassert>
#include <cstring>
#include <chrono>

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("db");

namespace sylar {
namespace db {

// MySQL 多库配置（从 YAML 的 mysql 键加载）
static sylar::ConfigVar<MysqlConfMap>::ptr g_mysql_conf_map
    = sylar::Config::Lookup("mysql", MysqlConfMap(), "mysql config map");

// ==================== MysqlConn 类实现 ====================

// 构造函数
MysqlConn::MysqlConn()
    : mysql_(nullptr), result_(nullptr), row_(nullptr), fields_(nullptr),
      lengths_(nullptr), fieldCount_(0), connected_(false),
      lastErrorCode_(0) {
    mysql_ = mysql_init(nullptr);
    if (!mysql_) {
        lastError_ = "Failed to initialize MySQL connection";
    }
}

// 析构函数
MysqlConn::~MysqlConn() {
    Disconnect();
}

// 连接到MySQL服务器
bool MysqlConn::Connect(const std::string& host,
                        uint32_t port,
                        const std::string& user,
                        const std::string& passwd,
                        const std::string& dbname) {
    if (!mysql_) {
        mysql_ = mysql_init(nullptr);
        if (!mysql_) {
            lastError_ = "Failed to initialize MySQL connection";
            lastErrorCode_ = 0;
            return false;
        }
    }

    unsigned int timeout = 10;
    mysql_options(mysql_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(mysql_, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(mysql_, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
    mysql_options(mysql_, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    MYSQL* ret = mysql_real_connect(mysql_,
                                    host.empty() ? nullptr : host.c_str(),
                                    user.empty() ? nullptr : user.c_str(),
                                    passwd.empty() ? nullptr : passwd.c_str(),
                                    dbname.empty() ? nullptr : dbname.c_str(),
                                    port,
                                    nullptr,
                                    CLIENT_FOUND_ROWS | CLIENT_MULTI_STATEMENTS);

    if (!ret) {
        lastError_ = mysql_error(mysql_);
        lastErrorCode_ = mysql_errno(mysql_);
        SYLAR_LOG_ERROR(g_logger) << "mysql connect fail " << host << ":" << port << " " << lastError_;
        connected_ = false;
        return false;
    }

    connected_ = true;
    setCharset("utf8mb4");
    return true;
}

// 带超时与字符集的 Connect（供连接池使用）
bool MysqlConn::Connect(const std::string& host, uint32_t port,
                        const std::string& user, const std::string& passwd, const std::string& dbname,
                        unsigned int connect_timeout_sec, unsigned int read_timeout_sec, unsigned int write_timeout_sec,
                        const std::string& charset) {
    if (!mysql_) {
        mysql_ = mysql_init(nullptr);
        if (!mysql_) {
            lastError_ = "Failed to initialize MySQL connection";
            lastErrorCode_ = 0;
            return false;
        }
    }
    unsigned int cto = connect_timeout_sec;
    unsigned int rto = read_timeout_sec > 0 ? read_timeout_sec : connect_timeout_sec;
    unsigned int wto = write_timeout_sec > 0 ? write_timeout_sec : connect_timeout_sec;
    mysql_options(mysql_, MYSQL_OPT_CONNECT_TIMEOUT, &cto);
    mysql_options(mysql_, MYSQL_OPT_READ_TIMEOUT, &rto);
    mysql_options(mysql_, MYSQL_OPT_WRITE_TIMEOUT, &wto);
    mysql_options(mysql_, MYSQL_SET_CHARSET_NAME, charset.empty() ? "utf8mb4" : charset.c_str());

    MYSQL* ret = mysql_real_connect(mysql_,
                                    host.empty() ? nullptr : host.c_str(),
                                    user.empty() ? nullptr : user.c_str(),
                                    passwd.empty() ? nullptr : passwd.c_str(),
                                    dbname.empty() ? nullptr : dbname.c_str(),
                                    port, nullptr, CLIENT_FOUND_ROWS | CLIENT_MULTI_STATEMENTS);
    if (!ret) {
        lastError_ = mysql_error(mysql_);
        lastErrorCode_ = mysql_errno(mysql_);
        connected_ = false;
        return false;
    }
    connected_ = true;
    setCharset(charset.empty() ? "utf8mb4" : charset);
    return true;
}

// 断开连接
void MysqlConn::Disconnect() {
    if (result_) {
        mysql_free_result(result_);
        result_ = nullptr;
    }
    
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
    
    connected_ = false;
    row_ = nullptr;
    fields_ = nullptr;
    lengths_ = nullptr;
    fieldCount_ = 0;
}

// 选择数据库
bool MysqlConn::SelectDB(const std::string& dbname) {
    if (!isConnected()) {
        lastError_ = "Not connected to MySQL server";
        lastErrorCode_ = 0;
        return false;
    }

    if (mysql_select_db(mysql_, dbname.c_str()) != 0) {
        lastError_ = mysql_error(mysql_);
        lastErrorCode_ = mysql_errno(mysql_);
        SYLAR_LOG_ERROR(g_logger) << "mysql select_db fail " << dbname << " " << lastError_;
        return false;
    }

    return true;
}

// 执行SQL查询（返回结果集）
bool MysqlConn::Query(const std::string& sql) {
    if (!isConnected()) {
        lastError_ = "Not connected to MySQL server";
        lastErrorCode_ = 0;
        return false;
    }

    // 释放之前的结果集
    freeResult();

    if (mysql_real_query(mysql_, sql.c_str(), sql.length()) != 0) {
        lastError_ = mysql_error(mysql_);
        lastErrorCode_ = mysql_errno(mysql_);
        SYLAR_LOG_ERROR(g_logger) << "mysql query fail " << lastError_;
        return false;
    }

    result_ = mysql_store_result(mysql_);
    if (!result_) {
        // 可能是非SELECT语句，或者有错误
        if (mysql_errno(mysql_) != 0) {
            lastError_ = mysql_error(mysql_);
            lastErrorCode_ = mysql_errno(mysql_);
            SYLAR_LOG_ERROR(g_logger) << "mysql result fail " << lastError_;
            return false;
        }
        // 非SELECT语句，没有结果集，这是正常的
        return true;
    }

    // 获取字段信息
    fieldCount_ = mysql_num_fields(result_);
    fields_ = mysql_fetch_fields(result_);
    
    // 获取第一行
    row_ = mysql_fetch_row(result_);
    if (row_) {
        lengths_ = mysql_fetch_lengths(result_);
    }

    return true;
}

// 执行SQL更新（不返回结果集）
bool MysqlConn::Execute(const std::string& sql) {
    if (!isConnected()) {
        lastError_ = "Not connected to MySQL server";
        lastErrorCode_ = 0;
        return false;
    }

    // 释放之前的结果集
    freeResult();

    if (mysql_real_query(mysql_, sql.c_str(), sql.length()) != 0) {
        lastError_ = mysql_error(mysql_);
        lastErrorCode_ = mysql_errno(mysql_);
        SYLAR_LOG_ERROR(g_logger) << "mysql execute fail " << lastError_;
        return false;
    }

    // 对于非SELECT语句，可能有多个结果集，需要处理完所有结果
    do {
        if (result_) {
            mysql_free_result(result_);
            result_ = nullptr;
        }
        
        if ((result_ = mysql_store_result(mysql_)) != nullptr) {
            mysql_free_result(result_);
            result_ = nullptr;
        } else {
            if (mysql_errno(mysql_) != 0) {
                lastError_ = mysql_error(mysql_);
                lastErrorCode_ = mysql_errno(mysql_);
                return false;
            }
        }
    } while (mysql_next_result(mysql_) == 0);

    return true;
}

// 获取查询结果的行数
uint64_t MysqlConn::getRowCount() const {
    if (!result_) {
        return 0;
    }
    return mysql_num_rows(result_);
}

// 获取受影响的行数
uint64_t MysqlConn::getAffectedRows() const {
    if (!isConnected()) {
        return 0;
    }
    return mysql_affected_rows(mysql_);
}

// 获取最后插入的自增ID
uint64_t MysqlConn::getLastInsertId() const {
    if (!isConnected()) {
        return 0;
    }
    return mysql_insert_id(mysql_);
}

// 获取一行结果
MysqlRow MysqlConn::getRowResult() {
    MysqlRow row;
    
    if (!result_ || !row_) {
        return row;
    }

    for (uint32_t i = 0; i < fieldCount_; ++i) {
        std::string fieldName = fields_[i].name;
        std::string fieldValue;
        
        if (row_[i]) {
            fieldValue = std::string(row_[i], lengths_[i]);
        }
        
        row[fieldName] = fieldValue;
    }

    return row;
}

// 获取所有结果
MysqlRows MysqlConn::getAllRows() {
    MysqlRows rows;
    
    if (!result_) {
        return rows;
    }

    // 移动到第一行
    mysql_data_seek(result_, 0);
    
    MYSQL_ROW row;
    unsigned long* lengths;
    
    while ((row = mysql_fetch_row(result_)) != nullptr) {
        lengths = mysql_fetch_lengths(result_);
        MysqlRow rowMap;
        
        for (uint32_t i = 0; i < fieldCount_; ++i) {
            std::string fieldName = fields_[i].name;
            std::string fieldValue;
            
            if (row[i]) {
                fieldValue = std::string(row[i], lengths[i]);
            }
            
            rowMap[fieldName] = fieldValue;
        }
        
        rows.push_back(rowMap);
    }

    // 恢复当前行
    if (rows.empty()) {
        row_ = nullptr;
        lengths_ = nullptr;
    } else {
        mysql_data_seek(result_, 0);
        row_ = mysql_fetch_row(result_);
        if (row_) {
            lengths_ = mysql_fetch_lengths(result_);
        }
    }

    return rows;
}

// 获取单个字段值（通过字段名）
std::string MysqlConn::getFieldValue(const std::string& fieldName) {
    if (!result_ || !row_) {
        return "";
    }

    for (uint32_t i = 0; i < fieldCount_; ++i) {
        if (fieldName == fields_[i].name) {
            if (row_[i]) {
                return std::string(row_[i], lengths_[i]);
            }
            return "";
        }
    }

    return "";
}

// 获取单个字段值（通过索引）
std::string MysqlConn::getFieldValue(size_t index) {
    if (!result_ || !row_ || index >= fieldCount_) {
        return "";
    }

    if (row_[index]) {
        return std::string(row_[index], lengths_[index]);
    }

    return "";
}

// 移动到下一行
bool MysqlConn::next() {
    if (!result_) {
        return false;
    }

    row_ = mysql_fetch_row(result_);
    if (row_) {
        lengths_ = mysql_fetch_lengths(result_);
        return true;
    }

    return false;
}

// 移动到第一行
bool MysqlConn::first() {
    if (!result_) {
        return false;
    }

    mysql_data_seek(result_, 0);
    row_ = mysql_fetch_row(result_);
    if (row_) {
        lengths_ = mysql_fetch_lengths(result_);
        return true;
    }

    return false;
}

// 释放结果集
void MysqlConn::freeResult() {
    if (result_) {
        mysql_free_result(result_);
        result_ = nullptr;
    }
    row_ = nullptr;
    fields_ = nullptr;
    lengths_ = nullptr;
    fieldCount_ = 0;
}

// 获取列名
std::string MysqlConn::getFieldName(size_t index) const {
    if (!fields_ || index >= fieldCount_) return "";
    return fields_[index].name;
}

// 执行 SELECT 取第一行第一列
bool MysqlConn::queryValue(const std::string& sql, std::string& value) {
    value.clear();
    if (!Query(sql)) return false;
    if (!row_) return true;
    value = getFieldValue(0);
    freeResult();
    return true;
}

// 执行 SELECT 取第一行
bool MysqlConn::queryRow(const std::string& sql, MysqlRow& row) {
    row.clear();
    if (!Query(sql)) return false;
    if (!row_) return true;
    row = getRowResult();
    freeResult();
    return true;
}

// 执行 SELECT 取第一行第一列并转为整数
bool MysqlConn::queryInt64(const std::string& sql, int64_t& value) {
    std::string s;
    if (!queryValue(sql, s) || s.empty()) return false;
    try {
        value = std::stoll(s);
        return true;
    } catch (...) {
        return false;
    }
}

bool MysqlConn::queryUint64(const std::string& sql, uint64_t& value) {
    std::string s;
    if (!queryValue(sql, s) || s.empty()) return false;
    try {
        value = std::stoull(s);
        return true;
    } catch (...) {
        return false;
    }
}

// 获取错误信息
std::string MysqlConn::getError() const {
    if (lastError_.empty() && mysql_) {
        return mysql_error(mysql_);
    }
    return lastError_;
}

// 获取错误码
unsigned int MysqlConn::getErrorCode() const {
    if (lastErrorCode_ == 0 && mysql_) {
        return mysql_errno(mysql_);
    }
    return lastErrorCode_;
}

// 检查连接是否有效
bool MysqlConn::isConnected() const {
    return connected_ && mysql_ != nullptr;
}

// 开启事务
bool MysqlConn::beginTransaction() {
    return Execute("START TRANSACTION");
}

// 提交事务
bool MysqlConn::commit() {
    return Execute("COMMIT");
}

// 回滚事务
bool MysqlConn::rollback() {
    return Execute("ROLLBACK");
}

// 设置自动提交模式
bool MysqlConn::setAutoCommit(bool autoCommit) {
    if (!isConnected()) {
        lastError_ = "Not connected to MySQL server";
        lastErrorCode_ = 0;
        return false;
    }

    bool mode = autoCommit ? 1 : 0;
    if (mysql_autocommit(mysql_, mode) != 0) {
        lastError_ = mysql_error(mysql_);
        lastErrorCode_ = mysql_errno(mysql_);
        return false;
    }

    return true;
}

// 设置字符集
bool MysqlConn::setCharset(const std::string& charset) {
    if (!isConnected()) {
        lastError_ = "Not connected to MySQL server";
        lastErrorCode_ = 0;
        return false;
    }

    if (mysql_set_character_set(mysql_, charset.c_str()) != 0) {
        lastError_ = mysql_error(mysql_);
        lastErrorCode_ = mysql_errno(mysql_);
        return false;
    }

    return true;
}

// 转义字符串
std::string MysqlConn::escapeString(const std::string& str) {
    if (!isConnected()) {
        return str;
    }

    std::string escaped;
    escaped.resize(str.length() * 2 + 1);
    
    unsigned long len = mysql_real_escape_string(mysql_, &escaped[0], str.c_str(), str.length());
    escaped.resize(len);
    
    return escaped;
}

// 获取MySQL服务器信息
std::string MysqlConn::getServerInfo() const {
    if (!isConnected()) {
        return "";
    }
    return mysql_get_server_info(mysql_);
}

// 获取MySQL客户端信息
std::string MysqlConn::getClientInfo() const {
    return mysql_get_client_info();
}

// Ping服务器
bool MysqlConn::ping() {
    if (!isConnected()) {
        return false;
    }

    if (mysql_ping(mysql_) != 0) {
        lastError_ = mysql_error(mysql_);
        lastErrorCode_ = mysql_errno(mysql_);
        connected_ = false;
        return false;
    }

    return true;
}

// 获取当前数据库名
std::string MysqlConn::getCurrentDB() {
    if (!isConnected()) {
        return "";
    }

    // 执行查询获取当前数据库
    // 注意：这会释放当前结果集，调用者需要注意
    std::string dbname;
    if (Query("SELECT DATABASE()")) {
        if (next()) {
            dbname = getFieldValue(0);
        }
        freeResult();
    }

    return dbname;
}

// 执行多条SQL语句
bool MysqlConn::executeBatch(const std::string& sql) {
    if (!isConnected()) {
        lastError_ = "Not connected to MySQL server";
        lastErrorCode_ = 0;
        return false;
    }

    freeResult();

    if (mysql_real_query(mysql_, sql.c_str(), sql.length()) != 0) {
        lastError_ = mysql_error(mysql_);
        lastErrorCode_ = mysql_errno(mysql_);
        return false;
    }

    // 处理所有结果集
    do {
        if (result_) {
            mysql_free_result(result_);
            result_ = nullptr;
        }
        
        if ((result_ = mysql_store_result(mysql_)) != nullptr) {
            mysql_free_result(result_);
            result_ = nullptr;
        } else {
            if (mysql_errno(mysql_) != 0) {
                lastError_ = mysql_error(mysql_);
                lastErrorCode_ = mysql_errno(mysql_);
                return false;
            }
        }
    } while (mysql_next_result(mysql_) == 0);

    return true;
}

// ==================== MysqlConnPool 类实现 ====================

// 构造函数（按名称，从配置初始化）
MysqlConnPool::MysqlConnPool(const std::string& name)
    : name_(name), connSize_(0), heartBeatTime_(0), heartbeat_id_(0), isStop_(false), isInited_(false) {
    charset_ = "utf8mb4";
    connect_timeout_sec_ = 10;
    read_timeout_sec_ = 60;
    write_timeout_sec_ = 60;
    validate_before_borrow_ = true;
    get_conn_wait_ms_ = 3000;
}

// 析构函数
MysqlConnPool::~MysqlConnPool() {
    Close();
}

// 约定优于配置：有配置则仅使用配置项，按名称严格匹配，不使用函数参数或硬编码
bool MysqlConnPool::init() {
    if (isInited_) {
        return true;
    }
    isStop_ = false;
    MysqlConfMap confMap = g_mysql_conf_map->getValue();
    auto it = confMap.find(name_);
    if (it == confMap.end()) {
        SYLAR_LOG_ERROR(g_logger) << "mysql pool[" << name_ << "] config not found";
        return false;
    }
    const MysqlConf& conf = it->second;
    host_ = conf.host;
    port_ = conf.port;
    user_ = conf.user;
    passwd_ = conf.passwd;
    dbname_ = conf.dbname;
    connSize_ = conf.connSize;
    heartBeatTime_ = conf.heartBeatTime;
    charset_ = conf.charset.empty() ? "utf8mb4" : conf.charset;
    connect_timeout_sec_ = conf.connect_timeout_sec;
    read_timeout_sec_ = conf.read_timeout_sec;
    write_timeout_sec_ = conf.write_timeout_sec;
    init_sql_ = conf.init_sql;
    validate_before_borrow_ = conf.validate_before_borrow;
    get_conn_wait_ms_ = conf.get_conn_wait_ms;

    SYLAR_LOG_DEBUG(g_logger) << "mysql pool[" << name_ << "] init " << host_ << ":" << port_ << "/" << dbname_;

    auto worker = sylar::db::DBWorker::GetInstance();
    for (size_t i = 0; i < connSize_; ++i) {
        worker->addTask([this]() { CreateConn(); });
    }
    heartbeat_id_ = worker->addHeartbeat([this]() { heartbeatTask(); }, heartBeatTime_);

    const uint64_t init_wait_ms = 10000;
    if (!waitForConnectionsReady(init_wait_ms)) {
        if (heartbeat_id_) {
            worker->removeHeartbeat(heartbeat_id_);
            heartbeat_id_ = 0;
        }
        isStop_ = true;
        SYLAR_LOG_ERROR(g_logger) << "mysql pool[" << name_ << "] init wait for connections timeout " << init_wait_ms << "ms";
        return false;
    }
    isInited_ = true;
    return true;
}

// 使用指定参数初始化（兼容旧代码）
bool MysqlConnPool::init(const std::string& host,
                         uint32_t port,
                         const std::string& user,
                         const std::string& passwd,
                         const std::string& dbname,
                         size_t connSize,
                         uint64_t heartBeatTime) {
    if (isInited_) {
        return true;
    }
    isStop_ = false;
    host_ = host;
    port_ = port;
    user_ = user;
    passwd_ = passwd;
    dbname_ = dbname;
    connSize_ = connSize;
    heartBeatTime_ = heartBeatTime;
    charset_ = "utf8mb4";
    connect_timeout_sec_ = 10;
    read_timeout_sec_ = 60;
    write_timeout_sec_ = 60;
    init_sql_.clear();
    validate_before_borrow_ = true;
    get_conn_wait_ms_ = 3000;

    auto worker = sylar::db::DBWorker::GetInstance();
    for (size_t i = 0; i < connSize_; ++i) {
        worker->addTask([this]() { CreateConn(); });
    }
    heartbeat_id_ = worker->addHeartbeat([this]() { heartbeatTask(); }, heartBeatTime_);

    const uint64_t init_wait_ms = 10000;
    if (!waitForConnectionsReady(init_wait_ms)) {
        if (heartbeat_id_) {
            worker->removeHeartbeat(heartbeat_id_);
            heartbeat_id_ = 0;
        }
        isStop_ = true;
        SYLAR_LOG_ERROR(g_logger) << "mysql pool[" << name_ << "] init wait for connections timeout " << init_wait_ms << "ms";
        return false;
    }
    isInited_ = true;
    return true;
}

// 创建新连接（带重试机制）
void MysqlConnPool::CreateConn() {
    const int maxRetries = 3;
    const int retryDelayMs = 1000;
    std::shared_ptr<MysqlConn> conn;
    for (int retry = 0; retry < maxRetries; ++retry) {
        conn = std::make_shared<MysqlConn>();
        if (conn->Connect(host_, port_, user_, passwd_, dbname_,
                         connect_timeout_sec_, read_timeout_sec_, write_timeout_sec_, charset_)) {
            if (conn->ping()) {
                if (!init_sql_.empty() && !conn->Execute(init_sql_)) {
                    SYLAR_LOG_DEBUG(g_logger) << "mysql pool init_sql fail " << init_sql_;
                }
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    conns_.push(conn);
                }
                cond_.notify_one();
                return;
            }
        }
        if (retry < maxRetries - 1) {
            usleep(retryDelayMs * 1000);
        }
    }
    if (conn) {
        SYLAR_LOG_ERROR(g_logger) << "mysql pool create conn fail after " << maxRetries << " retries " << host_ << ":" << port_ << "/" << dbname_
            << " errcode=" << conn->getErrorCode() << " err=" << conn->getError();
    } else {
        SYLAR_LOG_ERROR(g_logger) << "mysql pool create conn fail after " << maxRetries << " retries " << host_ << ":" << port_ << "/" << dbname_;
    }
}

// 获取连接（自动归还）
std::shared_ptr<MysqlConn> MysqlConnPool::GetConn() {
    if (isStop_ || !isInited_) {
        SYLAR_LOG_ERROR(g_logger) << "mysql pool not inited or stopped";
        return nullptr;
    }
    std::shared_ptr<MysqlConn> conn;
    {
        std::unique_lock<std::mutex> lock(mtx_);
        if (!conns_.empty()) {
            conn = conns_.front();
            conns_.pop();
        } else if (get_conn_wait_ms_ > 0) {
            cond_.wait_for(lock, std::chrono::milliseconds(get_conn_wait_ms_),
                [this]() { return !conns_.empty() || isStop_; });
            if (!conns_.empty()) {
                conn = conns_.front();
                conns_.pop();
            }
        }
    }
    if (!conn) {
        SYLAR_LOG_DEBUG(g_logger) << "mysql pool[" << name_ << "] no conn";
        return nullptr;
    }
    if (validate_before_borrow_) {
        if (!conn->ping() && !conn->Connect(host_, port_, user_, passwd_, dbname_,
                connect_timeout_sec_, read_timeout_sec_, write_timeout_sec_, charset_)) {
            sylar::db::DBWorker::GetInstance()->addTask([this]() { CreateConn(); });
            std::lock_guard<std::mutex> lock(mtx_);
            if (!conns_.empty()) {
                conn = conns_.front();
                conns_.pop();
            } else {
                return nullptr;
            }
        }
    }
    std::weak_ptr<MysqlConnPool> weak_self = shared_from_this();
    return std::shared_ptr<MysqlConn>(conn.get(),
        [weak_self, conn](MysqlConn*) {
            try {
                if (auto self = weak_self.lock()) {
                    self->ReturnOwner(conn);
                }
            } catch (const std::exception& e) {
                SYLAR_LOG_ERROR(g_logger) << "mysql pool return conn exception " << e.what();
            } catch (...) {
                SYLAR_LOG_ERROR(g_logger) << "mysql pool return conn unknown exception";
            }
        });
}

// 主动归还连接
void MysqlConnPool::ReturnConn(std::shared_ptr<MysqlConn> conn) {
    if (!conn) return;
    // 通过 reset 触发自定义 deleter，实现自动归还
    // 或者直接调用 ReturnOwner
    ReturnOwner(conn);
}

// 内部归还连接
void MysqlConnPool::ReturnOwner(const std::shared_ptr<MysqlConn>& conn) {
    if (!conn || isStop_) return;
    if (!conn->isConnected()) return;
    conn->freeResult();
    conn->rollback();
    conn->setAutoCommit(true);
    if (!conn->ping()) return;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        conns_.push(conn);
    }
    cond_.notify_one();
}

// 获取连接池大小
size_t MysqlConnPool::getSize() {
    std::lock_guard<std::mutex> lock(mtx_);
    return conns_.size();
}

// 内部使用：等待至少 1 条连接就绪（不要求 isInited_）
bool MysqlConnPool::waitForConnectionsReady(uint64_t timeout_ms) {
    uint64_t start_time = sylar::GetCurrentMS();
    const uint64_t check_interval = 200;
    while (true) {
        std::shared_ptr<MysqlConn> testConn;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (isStop_) return false;
            if (!conns_.empty()) testConn = conns_.front();
        }
        if (testConn && testConn->ping()) return true;
        if (sylar::GetCurrentMS() - start_time >= timeout_ms) return false;
        usleep(check_interval * 1000);
    }
}

// 等待连接池就绪（至少有一个可用连接）
bool MysqlConnPool::waitForReady(uint64_t timeout_ms) {
    if (isStop_ || !isInited_) {
        SYLAR_LOG_ERROR(g_logger) << "mysql pool not inited or stopped";
        return false;
    }
    uint64_t start_time = sylar::GetCurrentMS();
    uint64_t check_interval = 200;
    int retryCount = 0;
    const int maxSyncRetries = 5;
    while (true) {
        std::shared_ptr<MysqlConn> testConn;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!conns_.empty()) testConn = conns_.front();
        }
        if (testConn && testConn->ping()) {
            return true;
        }
        if (testConn) {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!conns_.empty()) conns_.pop();
        }
        if (retryCount < maxSyncRetries) {
            retryCount++;
            sylar::db::DBWorker::GetInstance()->addTask([this]() { CreateConn(); });
        }
        uint64_t elapsed = sylar::GetCurrentMS() - start_time;
        if (elapsed >= timeout_ms) {
            SYLAR_LOG_ERROR(g_logger) << "mysql pool waitForReady timeout " << elapsed << "ms";
            return false;
        }
        usleep(check_interval * 1000);
    }
}

// 心跳检测任务
void MysqlConnPool::heartbeatTask() {
    if (isStop_) return;
    std::vector<std::shared_ptr<MysqlConn>> conns_copy;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        while (!conns_.empty()) {
            conns_copy.push_back(conns_.front());
            conns_.pop();
        }
    }
    for (auto& conn : conns_copy) {
        if (!conn) continue;
        if (!conn->ping()) {
            if (!conn->Connect(host_, port_, user_, passwd_, dbname_,
                    connect_timeout_sec_, read_timeout_sec_, write_timeout_sec_, charset_)) {
                CreateConn();
            } else {
                ReturnOwner(conn);
            }
        } else {
            ReturnOwner(conn);
        }
    }
}

// 关闭连接池
void MysqlConnPool::Close() {
    if (isStop_) return;
    isStop_ = true;
    if (heartbeat_id_) {
        sylar::db::DBWorker::GetInstance()->removeHeartbeat(heartbeat_id_);
        heartbeat_id_ = 0;
    }
    {
        std::lock_guard<std::mutex> lock(mtx_);
        while (!conns_.empty()) conns_.pop();
        cond_.notify_all();
    }
}

// ==================== MysqlConnPoolMgr 实现 ====================

MysqlConnPoolMgr* MysqlConnPoolMgr::GetInstance() {
    static MysqlConnPoolMgr instance;
    return &instance;
}

// 约定优于配置：有配置则仅用配置；仅当配置为空且 name 为 default 时才用默认参数
MysqlConnPoolMgr::MysqlConnPoolPtr MysqlConnPoolMgr::getPool(const std::string& name) {
    {
        sylar::RWMutex::ReadLock lock(mutex_);
        auto it = pools_.find(name);
        if (it != pools_.end()) {
            return it->second;
        }
    }
    sylar::WriteScopedLockImpl<sylar::RWMutex> writeLock(mutex_);
    auto it = pools_.find(name);
    if (it != pools_.end()) {
        return it->second;
    }
    MysqlConfMap confMap = g_mysql_conf_map->getValue();
    auto pool = std::make_shared<MysqlConnPool>(name);
    if (pool->init()) {
        pools_[name] = pool;
        SYLAR_LOG_DEBUG(g_logger) << "mysql mgr pool[" << name << "] created";
        return pool;
    }
    if (confMap.empty() && name == "default") {
        if (pool->init("localhost", 3306, "root", "123456789", "", 256, 5 * 60 * 1000)) {
            pools_[name] = pool;
            SYLAR_LOG_DEBUG(g_logger) << "mysql mgr pool[default] created (no config)";
            return pool;
        }
    }
    SYLAR_LOG_ERROR(g_logger) << "mysql mgr pool[" << name << "] init fail";
    return nullptr;
}

std::shared_ptr<MysqlConn> MysqlConnPoolMgr::getConn(const std::string& name) {
    MysqlConnPoolPtr pool = getPool(name);
    return pool ? pool->GetConn() : nullptr;
}

bool MysqlConnPoolMgr::initAll() {
    MysqlConfMap confMap = g_mysql_conf_map->getValue();
    if (confMap.empty()) {
        SYLAR_LOG_DEBUG(g_logger) << "mysql mgr config empty, use default pool";
        return initPool("default");
    }
    bool allSuccess = true;
    sylar::WriteScopedLockImpl<sylar::RWMutex> lock(mutex_);
    for (const auto& pair : confMap) {
        const std::string& name = pair.first;
        if (pools_.find(name) != pools_.end()) continue;
        auto pool = std::make_shared<MysqlConnPool>(name);
        if (pool->init()) {
            pools_[name] = pool;
            SYLAR_LOG_DEBUG(g_logger) << "mysql mgr pool[" << name << "] inited";
        } else {
            SYLAR_LOG_ERROR(g_logger) << "mysql mgr pool[" << name << "] init fail";
            allSuccess = false;
        }
    }
    return allSuccess;
}

bool MysqlConnPoolMgr::initPool(const std::string& name) {
    return getPool(name) != nullptr;
}

void MysqlConnPoolMgr::closeAll() {
    sylar::WriteScopedLockImpl<sylar::RWMutex> lock(mutex_);
    for (auto& pair : pools_) {
        if (pair.second) pair.second->Close();
    }
    pools_.clear();
    SYLAR_LOG_DEBUG(g_logger) << "mysql mgr closeAll";
}

void MysqlConnPoolMgr::closePool(const std::string& name) {
    sylar::WriteScopedLockImpl<sylar::RWMutex> lock(mutex_);
    auto it = pools_.find(name);
    if (it != pools_.end()) {
        if (it->second) it->second->Close();
        pools_.erase(it);
        SYLAR_LOG_DEBUG(g_logger) << "mysql mgr close pool[" << name << "]";
    }
}

std::vector<std::string> MysqlConnPoolMgr::getAllPoolNames() const {
    sylar::RWMutex::ReadLock lock(mutex_);
    std::vector<std::string> names;
    for (const auto& pair : pools_) {
        names.push_back(pair.first);
    }
    return names;
}

} // namespace db
} // namespace sylar

