#include "./redis_conn.h"
#include "./db_worker.h"
#include "sylar/log.h"
#include "sylar/config.h"
#include "sylar/util.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <chrono>
#include <cstdlib>

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("db");

namespace sylar {
namespace db {

static sylar::ConfigVar<RedisConfMap>::ptr g_redis_conf_map
    = sylar::Config::Lookup("redis", RedisConfMap(), "redis config map");

// ==================== RedisConn 实现 ====================

RedisConn::RedisConn()
    : client_(std::unique_ptr<RedisClient>(new RedisClient())) {
}

RedisConn::~RedisConn() {
    Disconnect();
}

bool RedisConn::Connect(const std::string& host, uint32_t port, const std::string& password,
                        int db_index) {
    if (!client_->Connect(host, port, password, 5000, 0)) return false;
    currentDb_ = 0;
    if (db_index != 0 && !SelectDb(db_index)) return false;
    currentDb_ = db_index;
    return true;
}

bool RedisConn::Connect(const std::string& host, uint32_t port, const std::string& password,
                        uint64_t timeout_ms, uint64_t connect_timeout_ms, int db_index) {
    if (!client_->Connect(host, port, password, timeout_ms, connect_timeout_ms)) return false;
    currentDb_ = 0;
    if (db_index != 0 && !SelectDb(db_index)) return false;
    currentDb_ = db_index;
    return true;
}

bool RedisConn::SelectDb(int db_index) {
    if (!isConnected()) return false;
    if (!Command("SELECT %d", db_index)) return false;
    currentDb_ = db_index;
    return true;
}

bool RedisConn::ensureDb(int db) {
    if (db < 0 || db == currentDb_) return true;
    if (!SelectDb(db)) return false;
    return true;
}

void RedisConn::Disconnect() {
    client_->Disconnect();
}

bool RedisConn::isConnected() const {
    return client_->isConnected();
}

bool RedisConn::Ping() {
    return client_->Ping();
}

bool RedisConn::Command(const std::string& cmd) {
    if (!isConnected()) {
        return false;
    }

    std::pair<std::string, std::vector<std::string> > parsed = parseCommand(cmd);
    return executeAndStoreResult(parsed.first, parsed.second);
}

bool RedisConn::Command(const char* format, ...) {
    if (!isConnected()) {
        return false;
    }

    // 格式化字符串
    va_list args1, args2;
    va_start(args1, format);
    
    // 第一次调用，计算需要的缓冲区大小
    va_copy(args2, args1);
    int size = vsnprintf(nullptr, 0, format, args1);
    va_end(args1);
    
    if (size < 0) {
        va_end(args2);
        return false;
    }
    
    // 分配缓冲区并格式化
    std::vector<char> buffer(size + 1);
    vsnprintf(buffer.data(), buffer.size(), format, args2);
    va_end(args2);
    
    std::string cmd(buffer.data());
    std::pair<std::string, std::vector<std::string> > parsed = parseCommand(cmd);
    return executeAndStoreResult(parsed.first, parsed.second);
}

bool RedisConn::CommandArgv(int argc, const char** argv) {
    if (!isConnected() || argc <= 0 || !argv) {
        return false;
    }

    std::string command;
    std::vector<std::string> args;
    
    for (int i = 0; i < argc; ++i) {
        if (!argv[i]) {
            return false;
        }
        
        if (i == 0) {
            // 第一个参数是命令，转换为大写
            command = argv[i];
            std::transform(command.begin(), command.end(), command.begin(),
                         [](unsigned char c) { return std::toupper(c); });
        } else {
            args.push_back(argv[i]);
        }
    }
    
    return executeAndStoreResult(command, args);
}

std::string RedisConn::getStringResult() {
    switch (lastReply_.type) {
        case RedisClient::ReplyType::STATUS:
        case RedisClient::ReplyType::ERROR:
        case RedisClient::ReplyType::BULK_STRING:
            return lastReply_.str;
        case RedisClient::ReplyType::INTEGER:
            return std::to_string(lastReply_.integer);
        case RedisClient::ReplyType::NIL:
            return "";
        case RedisClient::ReplyType::ARRAY:
            // 数组类型，返回第一个元素的字符串表示
            if (!lastReply_.array.empty()) {
                if (lastReply_.array[0].type == RedisClient::ReplyType::BULK_STRING ||
                    lastReply_.array[0].type == RedisClient::ReplyType::STATUS ||
                    lastReply_.array[0].type == RedisClient::ReplyType::ERROR) {
                    return lastReply_.array[0].str;
                }
            }
            return "";
        default:
            return "";
    }
}

std::vector<std::string> RedisConn::getArrayResult() {
    std::vector<std::string> result;
    
    if (lastReply_.type == RedisClient::ReplyType::ARRAY) {
        for (const auto& item : lastReply_.array) {
            switch (item.type) {
                case RedisClient::ReplyType::STATUS:
                case RedisClient::ReplyType::ERROR:
                case RedisClient::ReplyType::BULK_STRING:
                    result.push_back(item.str);
                    break;
                case RedisClient::ReplyType::INTEGER:
                    result.push_back(std::to_string(item.integer));
                    break;
                case RedisClient::ReplyType::NIL:
                    result.push_back("");
                    break;
                default:
                    result.push_back("");
                    break;
            }
        }
    }
    
    return result;
}

std::string RedisConn::getError() const {
    return client_->getError();
}

int RedisConn::getErrorCode() const {
    return client_->getErrorCode();
}

// ==================== 字符串操作 ====================

bool RedisConn::Set(const std::string& key, const std::string& value, int64_t expireTime) {
    return client_->Set(key, value, expireTime);
}

bool RedisConn::Set(const std::string& key, const std::string& value, int64_t expireTime, int db) {
    if (!ensureDb(db)) return false;
    return client_->Set(key, value, expireTime);
}

bool RedisConn::Get(const std::string& key, std::string& value) {
    return client_->Get(key, value);
}

bool RedisConn::Get(const std::string& key, std::string& value, int db) {
    if (!ensureDb(db)) return false;
    return client_->Get(key, value);
}

int64_t RedisConn::Del(const std::string& key) {
    return client_->Del(key);
}

int64_t RedisConn::Del(const std::vector<std::string>& keys) {
    return client_->Del(keys);
}

int64_t RedisConn::Exists(const std::string& key) {
    return client_->Exists(key);
}

bool RedisConn::Expire(const std::string& key, int64_t seconds) {
    return client_->Expire(key, seconds);
}

bool RedisConn::Pexpire(const std::string& key, int64_t milliseconds) {
    return client_->Pexpire(key, milliseconds);
}

int64_t RedisConn::TTL(const std::string& key) {
    return client_->TTL(key);
}

bool RedisConn::Keys(const std::string& pattern, std::vector<std::string>& keys) {
    return client_->Keys(pattern, keys);
}

bool RedisConn::MGet(const std::vector<std::string>& keys, std::vector<std::string>& values) {
    return client_->MGet(keys, values);
}

bool RedisConn::MSet(const std::map<std::string, std::string>& kvs) {
    return client_->MSet(kvs);
}

int64_t RedisConn::Incr(const std::string& key) {
    return client_->Incr(key);
}

int64_t RedisConn::IncrBy(const std::string& key, int64_t delta) {
    return client_->IncrBy(key, delta);
}

int64_t RedisConn::Decr(const std::string& key) {
    return client_->Decr(key);
}

int64_t RedisConn::DecrBy(const std::string& key, int64_t delta) {
    return client_->DecrBy(key, delta);
}

bool RedisConn::GetSet(const std::string& key, const std::string& value, std::string& oldValue) {
    return client_->GetSet(key, value, oldValue);
}

// ==================== Hash 操作 ====================

bool RedisConn::HSet(const std::string& key, const std::string& field, 
                     const std::string& value, int64_t& changed) {
    return client_->HSet(key, field, value, changed);
}

bool RedisConn::HSet(const std::string& key, const std::string& field, const std::string& value) {
    return client_->HSet(key, field, value);
}

bool RedisConn::HGet(const std::string& key, const std::string& field, std::string& value) {
    return client_->HGet(key, field, value);
}

int64_t RedisConn::HDel(const std::string& key, const std::string& field) {
    return client_->HDel(key, field);
}

int64_t RedisConn::HExists(const std::string& key, const std::string& field) {
    return client_->HExists(key, field);
}

bool RedisConn::HGetAll(const std::string& key, std::map<std::string, std::string>& result) {
    return client_->HGetAll(key, result);
}

bool RedisConn::HKeys(const std::string& key, std::vector<std::string>& fields) {
    return client_->HKeys(key, fields);
}

bool RedisConn::HVals(const std::string& key, std::vector<std::string>& values) {
    return client_->HVals(key, values);
}

int64_t RedisConn::HLen(const std::string& key) {
    return client_->HLen(key);
}

// ==================== List 操作 ====================

bool RedisConn::LPush(const std::string& key, const std::string& element, int64_t& len) {
    return client_->LPush(key, element, len);
}

bool RedisConn::RPush(const std::string& key, const std::string& element, int64_t& len) {
    return client_->RPush(key, element, len);
}

bool RedisConn::LPop(const std::string& key, std::string& element) {
    return client_->LPop(key, element);
}

bool RedisConn::RPop(const std::string& key, std::string& element) {
    return client_->RPop(key, element);
}

int64_t RedisConn::LLen(const std::string& key) {
    return client_->LLen(key);
}

bool RedisConn::LRange(const std::string& key, int64_t start, int64_t stop, 
                       std::vector<std::string>& elements) {
    return client_->LRange(key, start, stop, elements);
}

bool RedisConn::LIndex(const std::string& key, int64_t index, std::string& element) {
    return client_->LIndex(key, index, element);
}

// ==================== Set 操作 ====================

bool RedisConn::SAdd(const std::string& key, const std::string& member, int64_t& added) {
    return client_->SAdd(key, member, added);
}

int64_t RedisConn::SRem(const std::string& key, const std::string& member) {
    return client_->SRem(key, member);
}

int64_t RedisConn::SIsMember(const std::string& key, const std::string& member) {
    return client_->SIsMember(key, member);
}

bool RedisConn::SMembers(const std::string& key, std::vector<std::string>& members) {
    return client_->SMembers(key, members);
}

int64_t RedisConn::SCard(const std::string& key) {
    return client_->SCard(key);
}

// ==================== ZSet 操作 ====================

bool RedisConn::ZAdd(const std::string& key, double score, const std::string& member, int64_t& added) {
    return client_->ZAdd(key, score, member, added);
}

int64_t RedisConn::ZRem(const std::string& key, const std::string& member) {
    return client_->ZRem(key, member);
}

bool RedisConn::ZScore(const std::string& key, const std::string& member, double& score) {
    return client_->ZScore(key, member, score);
}

bool RedisConn::ZRange(const std::string& key, int64_t start, int64_t stop, 
                       std::vector<std::string>& members) {
    return client_->ZRange(key, start, stop, members);
}

bool RedisConn::ZRangeWithScores(const std::string& key, int64_t start, int64_t stop,
                                  std::vector<std::pair<double, std::string>>& out) {
    return client_->ZRangeWithScores(key, start, stop, out);
}

bool RedisConn::ZRevRange(const std::string& key, int64_t start, int64_t stop, 
                          std::vector<std::string>& members) {
    return client_->ZRevRange(key, start, stop, members);
}

bool RedisConn::ZRevRangeWithScores(const std::string& key, int64_t start, int64_t stop,
                                    std::vector<std::pair<double, std::string>>& out) {
    return client_->ZRevRangeWithScores(key, start, stop, out);
}

bool RedisConn::ZIncrBy(const std::string& key, double delta, const std::string& member, double& newScore) {
    return client_->ZIncrBy(key, delta, member, newScore);
}

int64_t RedisConn::ZCard(const std::string& key) {
    return client_->ZCard(key);
}

// ==================== 私有方法 ====================

std::pair<std::string, std::vector<std::string>> RedisConn::parseCommand(const std::string& cmd) {
    std::string command;
    std::vector<std::string> args;
    
    std::istringstream iss(cmd);
    std::string token;
    bool first = true;
    
    while (iss >> token) {
        // 处理引号包围的字符串
        if (token.front() == '"' || token.front() == '\'') {
            char quote = token.front();
            std::string quoted = token.substr(1);
            
            // 如果引号没有闭合
            if (token.back() != quote) {
                // 继续读取直到找到闭合引号
                std::string next;
                while (iss >> next) {
                    quoted += " " + next;
                    if (next.back() == quote) {
                        quoted = quoted.substr(0, quoted.length() - 1);
                        break;
                    }
                }
            } else {
                quoted = token.substr(1, token.length() - 2);
            }
            
            token = quoted;
        }
        
        if (first) {
            // 转换为大写（Redis命令不区分大小写，但我们统一使用大写）
            command = token;
            std::transform(command.begin(), command.end(), command.begin(), 
                         [](unsigned char c) { return std::toupper(c); });
            first = false;
        } else {
            args.push_back(token);
        }
    }
    
    return {command, args};
}

bool RedisConn::executeAndStoreResult(const std::string& cmd, const std::vector<std::string>& args) {
    lastReply_ = client_->ExecuteCommand(cmd, args);
    
    // 如果命令执行出错，返回 false
    if (lastReply_.isError()) {
        return false;
    }
    
    return true;
}

// ==================== RedisConnPool 实现 ====================

RedisConnPool::RedisConnPool(const std::string& name)
    : name_(name), connect_timeout_ms_(5000), db_index_(0),
      validate_before_borrow_(false), get_conn_wait_ms_(0),
      heartbeat_id_(0), heartBeatTime_(30 * 1000) {
}

RedisConnPool::~RedisConnPool() {
    Close();
}

// 约定优于配置：有配置则仅使用配置项，按名称严格匹配，不使用环境变量或函数参数
bool RedisConnPool::init() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) {
            Close();
        }
        RedisConfMap confMap = g_redis_conf_map->getValue();
    auto it = confMap.find(name_);
    if (it == confMap.end()) {
        SYLAR_LOG_ERROR(g_logger) << "redis pool[" << name_ << "] config not found";
        return false;
    }
    const RedisConf& conf = it->second;
    isStop_ = false;
    host_ = conf.host;
    port_ = conf.port;
    password_ = conf.password;
    timeout_ms_ = conf.timeout_ms;
    connect_timeout_ms_ = conf.connect_timeout_ms > 0 ? conf.connect_timeout_ms : conf.timeout_ms;
    db_index_ = conf.db_index;
    validate_before_borrow_ = conf.validate_before_borrow;
    get_conn_wait_ms_ = conf.get_conn_wait_ms;
    maxSize_ = conf.poolSize;
    heartBeatTime_ = conf.heartBeatTime;

    SYLAR_LOG_DEBUG(g_logger) << "redis pool[" << name_ << "] init " << host_ << ":" << port_ << " db=" << db_index_;

    auto worker = sylar::db::DBWorker::GetInstance();
    for (size_t i = 0; i < maxSize_; ++i) {
        worker->addTask([this]() { createConn(); });
    }
    heartbeat_id_ = worker->addHeartbeat([this]() { heartbeatTask(); }, heartBeatTime_);
    }
    // 等待至少 1 条连接就绪后再算 init 完成，避免非 worker 线程过早 GetConn 拿到空
    const uint64_t init_wait_ms = 10000;
    const size_t init_min_conn = 1;
    if (!waitForConnectionsReady(init_wait_ms, init_min_conn)) {
        std::lock_guard<std::mutex> lock2(mutex_);
        if (heartbeat_id_) {
            sylar::db::DBWorker::GetInstance()->removeHeartbeat(heartbeat_id_);
            heartbeat_id_ = 0;
        }
        isStop_ = true;
        SYLAR_LOG_ERROR(g_logger) << "redis pool[" << name_ << "] init wait for connections timeout " << init_wait_ms << "ms";
        return false;
    }
    {
        std::lock_guard<std::mutex> lock2(mutex_);
        initialized_ = true;
    }
    return true;
}

bool RedisConnPool::init(const std::string& host, uint32_t port, const std::string& password,
                         size_t poolSize, uint64_t timeout_ms, uint64_t heartBeatTime) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) {
            Close();
        }
        isStop_ = false;
        host_ = host;
        port_ = port;
        password_ = password;
        timeout_ms_ = timeout_ms;
        connect_timeout_ms_ = timeout_ms;
        db_index_ = 0;
        validate_before_borrow_ = false;
        get_conn_wait_ms_ = 0;
        maxSize_ = poolSize;
        heartBeatTime_ = heartBeatTime;
        auto worker = sylar::db::DBWorker::GetInstance();
        for (size_t i = 0; i < poolSize; ++i) {
            worker->addTask([this]() { createConn(); });
        }
        heartbeat_id_ = worker->addHeartbeat([this]() { heartbeatTask(); }, heartBeatTime_);
    }
    const uint64_t init_wait_ms = 10000;
    const size_t init_min_conn = 1;
    if (!waitForConnectionsReady(init_wait_ms, init_min_conn)) {
        std::lock_guard<std::mutex> lock2(mutex_);
        if (heartbeat_id_) {
            sylar::db::DBWorker::GetInstance()->removeHeartbeat(heartbeat_id_);
            heartbeat_id_ = 0;
        }
        isStop_ = true;
        SYLAR_LOG_ERROR(g_logger) << "redis pool[" << name_ << "] init wait for connections timeout " << init_wait_ms << "ms";
        return false;
    }
    {
        std::lock_guard<std::mutex> lock2(mutex_);
        initialized_ = true;
    }
    return true;
}

bool RedisConnPool::waitForConnectionsReady(uint64_t timeout_ms, size_t min_conn_count) {
    if (min_conn_count == 0) min_conn_count = 1;
    uint64_t start = sylar::GetCurrentMS();
    const uint64_t interval_ms = 200;
    while (true) {
        size_t avail = 0;
        std::shared_ptr<RedisConn> c;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (isStop_) return false;
            avail = available_.size();
            if (!available_.empty()) c = available_.back();
        }
        if (avail >= min_conn_count && c && c->Ping()) return true;
        if (sylar::GetCurrentMS() - start >= timeout_ms) return false;
        usleep(interval_ms * 1000);
    }
}

void RedisConnPool::createConn() {
    auto conn = std::make_shared<RedisConn>();
    if (!conn->Connect(host_, port_, password_, timeout_ms_, connect_timeout_ms_, db_index_)) {
        SYLAR_LOG_ERROR(g_logger) << "redis conn fail " << host_ << ":" << port_ << " " << conn->getError();
        return;
    }
    if (conn->Ping()) {
        std::lock_guard<std::mutex> lock(mutex_);
        available_.push_back(conn);
        cond_.notify_one();
    }
}

RedisConnPool::Connection RedisConnPool::GetConn() {
    std::shared_ptr<RedisConn> conn;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!initialized_) {
            SYLAR_LOG_ERROR(g_logger) << "redis pool not inited";
            return Connection(nullptr, nullptr);
        }
        if (!available_.empty()) {
            conn = available_.back();
            available_.pop_back();
        } else if (get_conn_wait_ms_ > 0) {
            cond_.wait_for(lock, std::chrono::milliseconds(get_conn_wait_ms_),
                [this]() { return !available_.empty() || isStop_; });
            if (!available_.empty()) {
                conn = available_.back();
                available_.pop_back();
            }
        }
    }
    if (!conn) {
        SYLAR_LOG_DEBUG(g_logger) << "redis pool[" << name_ << "] no conn";
        return Connection(nullptr, nullptr);
    }
    if (validate_before_borrow_) {
        if (!conn->Ping()) {
            if (!conn->Connect(host_, port_, password_, timeout_ms_, connect_timeout_ms_, db_index_)) {
                sylar::db::DBWorker::GetInstance()->addTask([this]() { createConn(); });
                std::lock_guard<std::mutex> lock(mutex_);
                if (!available_.empty()) {
                    conn = available_.back();
                    available_.pop_back();
                } else {
                    return Connection(nullptr, nullptr);
                }
            }
        }
    }
    return Connection(conn, this);
}

void RedisConnPool::ReturnConn(std::shared_ptr<RedisConn> conn) {
    if (!conn || isStop_) return;
    if (!conn->isConnected()) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (available_.size() < maxSize_) {
            available_.push_back(conn);
        }
    }
    cond_.notify_one();
}

size_t RedisConnPool::getSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return available_.size();
}

bool RedisConnPool::waitForReady(uint64_t timeout_ms, size_t min_conn_count) {
    if (!initialized_ || isStop_) return false;
    if (min_conn_count == 0) min_conn_count = 1;
    uint64_t start = sylar::GetCurrentMS();
    const uint64_t interval = 200;
    while (true) {
        size_t avail = 0;
        std::shared_ptr<RedisConn> c;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            avail = available_.size();
            if (!available_.empty()) c = available_.back();
        }
        if (avail >= min_conn_count && c && c->Ping()) return true;
        if (sylar::GetCurrentMS() - start >= timeout_ms) return false;
        usleep(interval * 1000);
    }
}

void RedisConnPool::Close() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isStop_) return;
        isStop_ = true;
        if (heartbeat_id_) {
            sylar::db::DBWorker::GetInstance()->removeHeartbeat(heartbeat_id_);
            heartbeat_id_ = 0;
        }
        for (auto& c : available_) { if (c) c->Disconnect(); }
        available_.clear();
        initialized_ = false;
        cond_.notify_all();
    }
}

void RedisConnPool::heartbeatTask() {
    if (isStop_) return;
    std::vector<std::shared_ptr<RedisConn>> copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        copy = available_;
        available_.clear();
    }
    for (auto& conn : copy) {
        if (!conn) continue;
        if (!conn->Ping()) {
            if (conn->Connect(host_, port_, password_, timeout_ms_, connect_timeout_ms_, db_index_))
                ReturnConn(conn);
            else
                sylar::db::DBWorker::GetInstance()->addTask([this]() { createConn(); });
        } else {
            ReturnConn(conn);
        }
    }
    auto worker = sylar::db::DBWorker::GetInstance();
    std::lock_guard<std::mutex> lock(mutex_);
    size_t need = maxSize_ - available_.size();
    for (size_t i = 0; i < need; ++i) {
        worker->addTask([this]() { createConn(); });
    }
}

// ==================== RedisConnPoolMgr 实现 ====================

RedisConnPoolMgr* RedisConnPoolMgr::GetInstance() {
    static RedisConnPoolMgr instance;
    return &instance;
}

// 约定优于配置：有配置则仅用配置；仅当配置为空且 name 为 default 时才用默认参数
RedisConnPoolMgr::RedisConnPoolPtr RedisConnPoolMgr::getPool(const std::string& name) {
    {
        sylar::RWMutex::ReadLock lock(mutex_);
        auto it = pools_.find(name);
        if (it != pools_.end()) return it->second;
    }
    sylar::WriteScopedLockImpl<sylar::RWMutex> wlock(mutex_);
    auto it = pools_.find(name);
    if (it != pools_.end()) return it->second;
    RedisConfMap confMap = g_redis_conf_map->getValue();
    auto pool = std::make_shared<RedisConnPool>(name);
    // default 池：若环境变量已设置（脚本/压测传入），优先用环境变量创建，确保密码等生效
    if (name == "default" && (getenv("REDIS_PASSWORD") || getenv("REDIS_HOST") || getenv("REDIS_PORT") || getenv("REDIS_POOL_SIZE"))) {
        std::string pass;
        if (getenv("REDIS_PASSWORD")) pass = getenv("REDIS_PASSWORD");
        else pass = "123456789";  // 脚本默认密码，与 run_redis_bench_compare.sh 一致
        std::string host = "127.0.0.1";
        if (const char* p = getenv("REDIS_HOST")) host = p;
        uint32_t port = 6379;
        if (const char* p = getenv("REDIS_PORT")) { int n = atoi(p); if (n > 0) port = (uint32_t)n; }
        size_t pool_size = 10;
        if (const char* p = getenv("REDIS_POOL_SIZE")) { int n = atoi(p); if (n > 0) pool_size = (size_t)n; }
        if (pool->init(host, port, pass, pool_size, 5000, 30 * 1000)) {
            pools_[name] = pool;
            SYLAR_LOG_DEBUG(g_logger) << "redis mgr pool[default] created (env)";
            return pool;
        }
    }
    if (pool->init()) {
        pools_[name] = pool;
        SYLAR_LOG_DEBUG(g_logger) << "redis mgr pool[" << name << "] created";
        return pool;
    }
    if (confMap.empty() && name == "default") {
        std::string pass;
        if (getenv("REDIS_PASSWORD")) pass = getenv("REDIS_PASSWORD");
        else pass = "123456789";
        std::string host = "127.0.0.1";
        if (const char* p = getenv("REDIS_HOST")) host = p;
        uint32_t port = 6379;
        if (const char* p = getenv("REDIS_PORT")) { int n = atoi(p); if (n > 0) port = (uint32_t)n; }
        size_t pool_size = 10;
        if (const char* p = getenv("REDIS_POOL_SIZE")) { int n = atoi(p); if (n > 0) pool_size = (size_t)n; }
        if (pool->init(host, port, pass, pool_size, 5000, 30 * 1000)) {
            pools_[name] = pool;
            SYLAR_LOG_DEBUG(g_logger) << "redis mgr pool[default] created (no config)";
            return pool;
        }
    }
    SYLAR_LOG_ERROR(g_logger) << "redis mgr pool[" << name << "] init fail";
    return nullptr;
}

RedisConnPoolMgr::ConnGuard RedisConnPoolMgr::getConn(const std::string& name) {
    RedisConnPoolPtr pool = getPool(name);
    if (!pool) return ConnGuard(nullptr, nullptr);
    return pool->GetConn();
}

bool RedisConnPoolMgr::initAll() {
    RedisConfMap confMap = g_redis_conf_map->getValue();
    if (confMap.empty()) {
        SYLAR_LOG_DEBUG(g_logger) << "redis mgr config empty, use default pool";
        return initPool("default");
    }
    bool ok = true;
    sylar::WriteScopedLockImpl<sylar::RWMutex> lock(mutex_);
    for (const auto& p : confMap) {
        if (pools_.find(p.first) != pools_.end()) continue;
        auto pool = std::make_shared<RedisConnPool>(p.first);
        if (pool->init()) {
            pools_[p.first] = pool;
        } else {
            ok = false;
        }
    }
    return ok;
}

bool RedisConnPoolMgr::initPool(const std::string& name) {
    return getPool(name) != nullptr;
}

void RedisConnPoolMgr::closeAll() {
    sylar::WriteScopedLockImpl<sylar::RWMutex> lock(mutex_);
    for (auto& p : pools_) { if (p.second) p.second->Close(); }
    pools_.clear();
}

void RedisConnPoolMgr::closePool(const std::string& name) {
    sylar::WriteScopedLockImpl<sylar::RWMutex> lock(mutex_);
    auto it = pools_.find(name);
    if (it != pools_.end()) {
        if (it->second) it->second->Close();
        pools_.erase(it);
    }
}

std::vector<std::string> RedisConnPoolMgr::getAllPoolNames() const {
    sylar::RWMutex::ReadLock lock(mutex_);
    std::vector<std::string> names;
    for (const auto& p : pools_) names.push_back(p.first);
    return names;
}

} // namespace db
} // namespace sylar

