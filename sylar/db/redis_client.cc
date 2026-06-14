#include "./redis_client.h"
#include "sylar/log.h"
#include "sylar/iomanager.h"
#include "sylar/fdmanager.h"
#include "sylar/address.h"
#include "sylar/fiber.h"
#include <algorithm>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("db");

namespace sylar {
namespace db {

RedisClient::RedisClient() 
    : connected_(false)
    , lastErrorCode_(0) {
}

RedisClient::~RedisClient() {
    Disconnect();
}

bool RedisClient::Connect(const std::string& host, uint32_t port,
                         const std::string& password, uint64_t timeout_ms,
                         uint64_t connect_timeout_ms) {
    sylar::RWMutex::WriteLock lock(mutex_);

    if (connected_) {
        Disconnect();
    }

    try {
        auto addr = sylar::IPAddress::Create(host.c_str(), port);
        if (!addr) {
            setError("Invalid address: " + host + ":" + std::to_string(port));
            return false;
        }

        socket_ = sylar::Socket::CreateTCP(addr);
        if (!socket_) {
            setError("Failed to create socket");
            return false;
        }

        // 协程下先 bind 触发 fd 创建并注册到 FdMgr（非阻塞），避免 connect 时阻塞
        if (sylar::IOManager::GetThis()) {
            sylar::Address::ptr bindAddr = (addr->getFamily() == AF_INET)
                ? (sylar::Address::ptr)sylar::IPv4Address::Create("0.0.0.0", 0)
                : (sylar::Address::ptr)sylar::IPv6Address::Create("::", 0);
            if (bindAddr && !socket_->bind(bindAddr)) {
                setError("Failed to bind before connect");
                socket_.reset();
                return false;
            }
        }

        uint64_t cto = (connect_timeout_ms > 0 ? connect_timeout_ms : timeout_ms);
        if (cto == 0) cto = 5000;
        if (!socket_->connect(addr, cto)) {
            setError("Failed to connect to " + addr->toString() + 
                    " errno=" + std::to_string(errno) + 
                    " errstr=" + std::string(strerror(errno)));
            socket_.reset();
            return false;
        }

        // 确保 FdCtx 存在并设超时：无 IOManager 时 hook 靠 FdCtx 做 usleep 重试与超时，否则会 EAGAIN 忙等或阻塞
        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(socket_->getSocket(), true);
        if (timeout_ms > 0) {
            socket_->setRecvTimeout(timeout_ms);
            socket_->setSendTimeout(timeout_ms);
            ctx->setTimeout(SO_RCVTIMEO, timeout_ms);
            ctx->setTimeout(SO_SNDTIMEO, timeout_ms);
        }

        connected_ = true;

        // 如果提供了密码，进行认证
        if (!password.empty()) {
            if (!authenticate(password)) {
                SYLAR_LOG_ERROR(g_logger) << "redis auth fail " << getError();
                Disconnect();
                return false;
            }
        } else {
            // 如果没有提供密码，尝试执行 PING 命令来检测是否需要认证
            // 这样可以更早地发现问题，而不是等到第一次业务命令时才失败
            auto pingReply = ExecuteCommand("PING");
            if (pingReply.isError()) {
                // 如果是认证错误，给出明确的提示
                if (pingReply.str.find("NOAUTH") != std::string::npos || 
                    pingReply.str.find("Authentication required") != std::string::npos) {
                    std::string errorMsg = "Redis服务器需要密码认证，但未提供密码。"
                                         "请在Connect时提供密码参数，或设置环境变量REDIS_PASSWORD";
                    SYLAR_LOG_ERROR(g_logger) << "redis need password";
                    setError(errorMsg);
                    Disconnect();
                    return false;
                }
                // 其他错误，也断开连接
                SYLAR_LOG_ERROR(g_logger) << "redis ping after connect fail " << pingReply.str;
                setError("连接后验证失败: " + pingReply.str);
                Disconnect();
                return false;
            }
        }

        return true;

    } catch (const std::exception& e) {
        setError("Exception: " + std::string(e.what()));
        connected_ = false;
        socket_.reset();
        return false;
    }
}

void RedisClient::Disconnect() {
    sylar::RWMutex::WriteLock lock(mutex_);
    
    if (socket_) {
        socket_->close();
        socket_.reset();
    }
    connected_ = false;
    lastError_.clear();
    lastErrorCode_ = 0;
    recvLineBuf_.clear();
}

bool RedisClient::isConnected() const {
    sylar::RWMutex::ReadLock lock(mutex_);
    return connected_ && socket_ && socket_->isConnected();
}

bool RedisClient::Ping() {
    auto reply = ExecuteCommand("PING");
    if (reply.type == ReplyType::STATUS && reply.str == "PONG") {
        return true;
    }
    if (reply.type == ReplyType::STATUS && reply.str == "OK") {
        return true;
    }
    return false;
}

bool RedisClient::authenticate(const std::string& password) {
    auto reply = ExecuteCommand("AUTH", password);
    if (reply.type == ReplyType::STATUS && reply.str == "OK") {
        return true;
    }
    setError("Authentication failed: " + (reply.isError() ? reply.str : "Unknown error"));
    return false;
}

std::string RedisClient::buildCommand(const std::string& cmd, const std::vector<std::string>& args) {
    // RESP: *n\r\n$len\r\n<data>\r\n...  预估长度避免多次扩容
    size_t argc = 1 + args.size();
    size_t cap = 32 + cmd.size();
    for (const auto& arg : args)
        cap += 24 + arg.size();
    std::string out;
    out.reserve(cap);

    auto appendLine = [&out](const std::string& s) {
        out += s;
        out += "\r\n";
    };
    out += "*";
    out += std::to_string(argc);
    out += "\r\n";
    out += "$";
    out += std::to_string(cmd.length());
    out += "\r\n";
    appendLine(cmd);
    for (const auto& arg : args) {
        out += "$";
        out += std::to_string(arg.length());
        out += "\r\n";
        appendLine(arg);
    }
    return out;
}

bool RedisClient::sendDataUnlocked(const std::string& data) {
    if (!connected_ || !socket_ || !socket_->isConnected()) {
        setError("Not connected");
        return false;
    }
    size_t sent = 0;
    const char* ptr = data.c_str();
    size_t len = data.length();
    while (sent < len) {
        int n = socket_->send(ptr + sent, len - sent);
        if (n <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                sylar::IOManager* iom = sylar::IOManager::GetThis();
                if (iom && socket_ && socket_->isConnected()) {
                    int fd = socket_->getSocket();
                    if (fd >= 0 && iom->addEvent(fd, sylar::IOManager::WRITE) == 0) {
                        sylar::Fiber::YeildToHold();
                    } else {
                        usleep(1000);
                    }
                } else {
                    usleep(1000);
                }
                continue;
            }
            setError("Send failed: " + std::string(strerror(errno)), errno);
            connected_ = false;
            return false;
        }
        sent += n;
    }
    return true;
}

bool RedisClient::sendData(const std::string& data, bool needLock) {
    if (needLock) {
        sylar::RWMutex::ReadLock lock(mutex_);
        return sendDataUnlocked(data);
    }
    return sendDataUnlocked(data);
}

// 按块读取一行，减少 recv 次数和协程 yield，提升 QPS
static const size_t RECV_LINE_CHUNK = 512;

bool RedisClient::recvLineUnlocked(std::string& line) {
    if (!connected_ || !socket_ || !socket_->isConnected()) {
        setError("Not connected");
        return false;
    }
    line.clear();
    size_t pos;
    while ((pos = recvLineBuf_.find("\r\n")) == std::string::npos) {
        char buf[RECV_LINE_CHUNK];
        int n = socket_->recv(buf, sizeof(buf));
        if (n > 0) {
            recvLineBuf_.append(buf, (size_t)n);
            continue;
        }
        if (n <= 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // EPOLLET：先把内核里已有数据读光，再 yield，避免漏事件
            while ((n = socket_->recv(buf, sizeof(buf))) > 0) {
                recvLineBuf_.append(buf, (size_t)n);
            }
            if (recvLineBuf_.find("\r\n") != std::string::npos) continue;
            sylar::IOManager* iom = sylar::IOManager::GetThis();
            if (iom && socket_ && socket_->isConnected()) {
                int fd = socket_->getSocket();
                if (fd >= 0 && iom->addEvent(fd, sylar::IOManager::READ) == 0) {
                    sylar::Fiber::YeildToHold();
                } else {
                    usleep(1000);
                }
            } else {
                usleep(1000);
            }
            continue;
        }
        setError("Recv failed: " + std::string(strerror(errno)), errno);
        connected_ = false;
        return false;
    }
    line = recvLineBuf_.substr(0, pos);
    recvLineBuf_.erase(0, pos + 2);
    return true;
}

bool RedisClient::recvLine(std::string& line, bool needLock) {
    if (needLock) {
        sylar::RWMutex::ReadLock lock(mutex_);
        return recvLineUnlocked(line);
    }
    return recvLineUnlocked(line);
}

bool RedisClient::recvDataUnlocked(void* buffer, size_t length) {
    if (!connected_ || !socket_ || !socket_->isConnected()) {
        setError("Not connected");
        return false;
    }
    size_t received = 0;
    char* ptr = static_cast<char*>(buffer);
    while (received < length) {
        int n = socket_->recv(ptr + received, length - received);
        if (n > 0) {
            received += n;
            continue;
        }
        if (n <= 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // EPOLLET：先尽量读满当前可用数据再 yield
            while (received < length && (n = socket_->recv(ptr + received, length - received)) > 0) {
                received += n;
            }
            if (received >= length) continue;
            sylar::IOManager* iom = sylar::IOManager::GetThis();
            if (iom && socket_ && socket_->isConnected()) {
                int fd = socket_->getSocket();
                if (fd >= 0 && iom->addEvent(fd, sylar::IOManager::READ) == 0) {
                    sylar::Fiber::YeildToHold();
                } else {
                    usleep(1000);
                }
            } else {
                usleep(1000);
            }
            continue;
        }
        setError("Recv failed: " + std::string(strerror(errno)), errno);
        connected_ = false;
        return false;
    }
    return true;
}

bool RedisClient::recvData(void* buffer, size_t length, bool needLock) {
    if (needLock) {
        sylar::RWMutex::ReadLock lock(mutex_);
        return recvDataUnlocked(buffer, length);
    }
    return recvDataUnlocked(buffer, length);
}

bool RedisClient::ensureRecvBufUnlocked(size_t n) {
    if (!connected_ || !socket_ || !socket_->isConnected()) {
        setError("Not connected");
        return false;
    }
    while (recvLineBuf_.size() < n) {
        char buf[RECV_LINE_CHUNK];
        size_t to_read = std::min((size_t)RECV_LINE_CHUNK, n - recvLineBuf_.size());
        int nr = socket_->recv(buf, to_read);
        if (nr > 0) {
            recvLineBuf_.append(buf, (size_t)nr);
            continue;
        }
        if (nr <= 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            while ((nr = socket_->recv(buf, sizeof(buf))) > 0) {
                recvLineBuf_.append(buf, (size_t)nr);
            }
            if (recvLineBuf_.size() >= n) continue;
            sylar::IOManager* iom = sylar::IOManager::GetThis();
            if (iom && socket_ && socket_->isConnected()) {
                int fd = socket_->getSocket();
                if (fd >= 0 && iom->addEvent(fd, sylar::IOManager::READ) == 0) {
                    sylar::Fiber::YeildToHold();
                } else {
                    usleep(1000);
                }
            } else {
                usleep(1000);
            }
            continue;
        }
        setError("Recv failed: " + std::string(strerror(errno)), errno);
        connected_ = false;
        return false;
    }
    return true;
}

RedisClient::Reply RedisClient::parseBulkString(int64_t length, bool underLock) {
    Reply reply;
    if (length < 0) {
        reply.type = ReplyType::NIL;
        return reply;
    }
    reply.type = ReplyType::BULK_STRING;
    size_t need = (size_t)length + 2;  // 内容 + 尾部 \r\n
    if (!ensureRecvBufUnlocked(need)) {
        reply.type = ReplyType::ERROR;
        reply.str = lastError_;
        return reply;
    }
    reply.str.assign(recvLineBuf_.data(), (size_t)length);
    recvLineBuf_.erase(0, need);
    return reply;
}

RedisClient::Reply RedisClient::parseArray(int64_t length, bool underLock) {
    Reply reply;
    if (length < 0) {
        reply.type = ReplyType::NIL;
        return reply;
    }
    reply.type = ReplyType::ARRAY;
    reply.array.resize(length);
    for (int64_t i = 0; i < length; ++i) {
        reply.array[i] = parseReply(underLock);
        if (reply.array[i].isError())
            return reply.array[i];
    }
    return reply;
}

RedisClient::Reply RedisClient::parseReply(bool underLock) {
    std::string line;
    if (!recvLine(line, !underLock)) {
        Reply reply;
        reply.type = ReplyType::ERROR;
        reply.str = lastError_;
        return reply;
    }

    if (line.empty()) {
        Reply reply;
        reply.type = ReplyType::ERROR;
        reply.str = "Empty reply";
        return reply;
    }

    Reply reply;
    char prefix = line[0];

    switch (prefix) {
        case '+': // Status reply
            reply.type = ReplyType::STATUS;
            reply.str = line.substr(1);
            break;

        case '-': // Error reply
            reply.type = ReplyType::ERROR;
            reply.str = line.substr(1);
            break;

        case ':': // Integer reply
            reply.type = ReplyType::INTEGER;
            try {
                reply.integer = std::stoll(line.substr(1));
            } catch (...) {
                reply.type = ReplyType::ERROR;
                reply.str = "Invalid integer: " + line;
            }
            break;

        case '$': // Bulk string reply
            {
                try {
                    int64_t length = std::stoll(line.substr(1));
                    reply = parseBulkString(length, underLock);
                } catch (...) {
                    reply.type = ReplyType::ERROR;
                    reply.str = "Invalid bulk string length: " + line;
                }
            }
            break;

        case '*': // Array reply
            {
                try {
                    int64_t length = std::stoll(line.substr(1));
                    reply = parseArray(length, underLock);
                } catch (...) {
                    reply.type = ReplyType::ERROR;
                    reply.str = "Invalid array length: " + line;
                }
            }
            break;

        default:
            reply.type = ReplyType::ERROR;
            reply.str = "Unknown reply type: " + line;
            break;
    }

    return reply;
}

RedisClient::Reply RedisClient::ExecuteCommand(const std::string& cmd, 
                                               const std::vector<std::string>& args) {
    sylar::RWMutex::WriteLock lock(mutex_);
    
    if (!connected_ || !socket_ || !socket_->isConnected()) {
        Reply reply;
        reply.type = ReplyType::ERROR;
        reply.str = "Not connected";
        setError(reply.str);
        return reply;
    }

    // 构建并发送命令（已持写锁，使用无锁发送避免重复加锁/死锁）
    std::string command = buildCommand(cmd, args);
    if (!sendDataUnlocked(command)) {
        Reply reply;
        reply.type = ReplyType::ERROR;
        reply.str = lastError_;
        return reply;
    }

    // 解析响应（underLock=true 使用无锁 recv）
    Reply reply = parseReply(true);
    
    // 如果是错误，记录到 lastError_
    if (reply.isError()) {
        setError(reply.str);
        // 如果是认证错误，给出更友好的提示
        if (reply.str.find("NOAUTH") != std::string::npos || 
            reply.str.find("Authentication required") != std::string::npos) {
            SYLAR_LOG_ERROR(g_logger) << "redis noauth " << reply.str;
        }
    } else {
        lastError_.clear();
        lastErrorCode_ = 0;
    }

    return reply;
}

void RedisClient::setError(const std::string& error, int code) {
    lastError_ = error;
    lastErrorCode_ = code;
    // 错误信息保留，但不在 setError 中打印，由调用者决定是否打印
}

// ==================== 字符串操作实现 ====================

bool RedisClient::Set(const std::string& key, const std::string& value, int64_t expireTime) {
    std::vector<std::string> args = {key, value};
    if (expireTime > 0) {
        args.push_back("EX");
        args.push_back(std::to_string(expireTime));
    }
    auto reply = ExecuteCommand("SET", args);
    return reply.isOK();
}

bool RedisClient::Get(const std::string& key, std::string& value) {
    auto reply = ExecuteCommand("GET", {key});
    if (reply.type == ReplyType::BULK_STRING) {
        value = reply.str;
        return true;
    }
    if (reply.type == ReplyType::NIL) {
        value.clear();
        return true;
    }
    // 如果返回 INTEGER 类型（不应该发生，但处理一下）
    if (reply.type == ReplyType::INTEGER) {
        value = std::to_string(reply.integer);
        //                           << ", integer值: " << reply.integer;
        return true;
    }
    // 其他类型（ERROR等）返回 false，并清空 value
    value.clear();
    if (reply.isError()) {
        SYLAR_LOG_DEBUG(g_logger) << "redis get fail key=" << key << " " << reply.str;
    } else {
        //                           << ", type: " << static_cast<int>(reply.type);
    }
    return false;
}

int64_t RedisClient::Del(const std::string& key) {
    auto reply = ExecuteCommand("DEL", {key});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return 0;
}

int64_t RedisClient::Del(const std::vector<std::string>& keys) {
    if (keys.empty()) return 0;
    auto reply = ExecuteCommand("DEL", keys);
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return 0;
}

int64_t RedisClient::Exists(const std::string& key) {
    auto reply = ExecuteCommand("EXISTS", {key});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return 0;
}

bool RedisClient::Expire(const std::string& key, int64_t seconds) {
    auto reply = ExecuteCommand("EXPIRE", {key, std::to_string(seconds)});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer == 1;
    }
    return false;
}

bool RedisClient::Pexpire(const std::string& key, int64_t milliseconds) {
    auto reply = ExecuteCommand("PEXPIRE", {key, std::to_string(milliseconds)});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer == 1;
    }
    return false;
}

int64_t RedisClient::TTL(const std::string& key) {
    auto reply = ExecuteCommand("TTL", {key});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return -2; // Key不存在
}

bool RedisClient::Keys(const std::string& pattern, std::vector<std::string>& keys) {
    auto reply = ExecuteCommand("KEYS", {pattern});
    if (reply.type == ReplyType::ARRAY) {
        keys.clear();
        for (const auto& item : reply.array) {
            if (item.type == ReplyType::BULK_STRING) {
                keys.push_back(item.str);
            }
        }
        return true;
    }
    return false;
}

bool RedisClient::MGet(const std::vector<std::string>& keys, std::vector<std::string>& values) {
    values.clear();
    if (keys.empty()) return true;
    auto reply = ExecuteCommand("MGET", keys);
    if (reply.type == ReplyType::ARRAY) {
        for (const auto& item : reply.array) {
            if (item.type == ReplyType::BULK_STRING || item.type == ReplyType::STATUS) {
                values.push_back(item.str);
            } else if (item.type == ReplyType::NIL) {
                values.push_back("");
            } else {
                values.push_back("");
            }
        }
        return true;
    }
    return false;
}

bool RedisClient::MSet(const std::map<std::string, std::string>& kvs) {
    if (kvs.empty()) return true;
    std::vector<std::string> args;
    for (const auto& p : kvs) {
        args.push_back(p.first);
        args.push_back(p.second);
    }
    auto reply = ExecuteCommand("MSET", args);
    return reply.isOK();
}

int64_t RedisClient::Incr(const std::string& key) {
    auto reply = ExecuteCommand("INCR", {key});
    if (reply.type == ReplyType::INTEGER) return reply.integer;
    return -1;
}

int64_t RedisClient::IncrBy(const std::string& key, int64_t delta) {
    auto reply = ExecuteCommand("INCRBY", {key, std::to_string(delta)});
    if (reply.type == ReplyType::INTEGER) return reply.integer;
    return -1;
}

int64_t RedisClient::Decr(const std::string& key) {
    auto reply = ExecuteCommand("DECR", {key});
    if (reply.type == ReplyType::INTEGER) return reply.integer;
    return -1;
}

int64_t RedisClient::DecrBy(const std::string& key, int64_t delta) {
    auto reply = ExecuteCommand("DECRBY", {key, std::to_string(delta)});
    if (reply.type == ReplyType::INTEGER) return reply.integer;
    return -1;
}

bool RedisClient::GetSet(const std::string& key, const std::string& value, std::string& oldValue) {
    oldValue.clear();
    auto reply = ExecuteCommand("GETSET", {key, value});
    if (reply.type == ReplyType::BULK_STRING || reply.type == ReplyType::STATUS) {
        oldValue = reply.str;
        return true;
    }
    if (reply.type == ReplyType::NIL) return true;
    return false;
}

// ==================== Hash 操作实现 ====================

bool RedisClient::HSet(const std::string& key, const std::string& field, 
                      const std::string& value, int64_t& changed) {
    auto reply = ExecuteCommand("HSET", {key, field, value});
    if (reply.type == ReplyType::INTEGER) {
        changed = reply.integer;
        return true;
    }
    return false;
}

bool RedisClient::HSet(const std::string& key, const std::string& field, const std::string& value) {
    int64_t changed;
    return HSet(key, field, value, changed);
}

bool RedisClient::HGet(const std::string& key, const std::string& field, std::string& value) {
    auto reply = ExecuteCommand("HGET", {key, field});
    if (reply.type == ReplyType::BULK_STRING) {
        value = reply.str;
        return true;
    }
    if (reply.type == ReplyType::NIL) {
        value.clear();
        return true;
    }
    return false;
}

int64_t RedisClient::HDel(const std::string& key, const std::string& field) {
    auto reply = ExecuteCommand("HDEL", {key, field});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return 0;
}

int64_t RedisClient::HExists(const std::string& key, const std::string& field) {
    auto reply = ExecuteCommand("HEXISTS", {key, field});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return 0;
}

bool RedisClient::HGetAll(const std::string& key, std::map<std::string, std::string>& result) {
    auto reply = ExecuteCommand("HGETALL", {key});
    if (reply.type == ReplyType::ARRAY) {
        result.clear();
        for (size_t i = 0; i < reply.array.size(); i += 2) {
            if (i + 1 < reply.array.size() &&
                reply.array[i].type == ReplyType::BULK_STRING &&
                reply.array[i + 1].type == ReplyType::BULK_STRING) {
                result[reply.array[i].str] = reply.array[i + 1].str;
            }
        }
        return true;
    }
    return false;
}

bool RedisClient::HKeys(const std::string& key, std::vector<std::string>& fields) {
    auto reply = ExecuteCommand("HKEYS", {key});
    if (reply.type == ReplyType::ARRAY) {
        fields.clear();
        for (const auto& item : reply.array) {
            if (item.type == ReplyType::BULK_STRING) {
                fields.push_back(item.str);
            }
        }
        return true;
    }
    return false;
}

bool RedisClient::HVals(const std::string& key, std::vector<std::string>& values) {
    auto reply = ExecuteCommand("HVALS", {key});
    if (reply.type == ReplyType::ARRAY) {
        values.clear();
        for (const auto& item : reply.array) {
            if (item.type == ReplyType::BULK_STRING) {
                values.push_back(item.str);
            }
        }
        return true;
    }
    return false;
}

int64_t RedisClient::HLen(const std::string& key) {
    auto reply = ExecuteCommand("HLEN", {key});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return 0;
}

// ==================== List 操作实现 ====================

bool RedisClient::LPush(const std::string& key, const std::string& element, int64_t& len) {
    auto reply = ExecuteCommand("LPUSH", {key, element});
    if (reply.type == ReplyType::INTEGER) {
        len = reply.integer;
        return true;
    }
    return false;
}

bool RedisClient::RPush(const std::string& key, const std::string& element, int64_t& len) {
    auto reply = ExecuteCommand("RPUSH", {key, element});
    if (reply.type == ReplyType::INTEGER) {
        len = reply.integer;
        return true;
    }
    return false;
}

bool RedisClient::LPop(const std::string& key, std::string& element) {
    auto reply = ExecuteCommand("LPOP", {key});
    if (reply.type == ReplyType::BULK_STRING) {
        element = reply.str;
        return true;
    }
    if (reply.type == ReplyType::NIL) {
        element.clear();
        return true;
    }
    return false;
}

bool RedisClient::RPop(const std::string& key, std::string& element) {
    auto reply = ExecuteCommand("RPOP", {key});
    if (reply.type == ReplyType::BULK_STRING) {
        element = reply.str;
        return true;
    }
    if (reply.type == ReplyType::NIL) {
        element.clear();
        return true;
    }
    return false;
}

int64_t RedisClient::LLen(const std::string& key) {
    auto reply = ExecuteCommand("LLEN", {key});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return 0;
}

bool RedisClient::LRange(const std::string& key, int64_t start, int64_t stop, 
                        std::vector<std::string>& elements) {
    auto reply = ExecuteCommand("LRANGE", {key, std::to_string(start), std::to_string(stop)});
    if (reply.type == ReplyType::ARRAY) {
        elements.clear();
        for (const auto& item : reply.array) {
            if (item.type == ReplyType::BULK_STRING) {
                elements.push_back(item.str);
            }
        }
        return true;
    }
    return false;
}

bool RedisClient::LIndex(const std::string& key, int64_t index, std::string& element) {
    auto reply = ExecuteCommand("LINDEX", {key, std::to_string(index)});
    if (reply.type == ReplyType::BULK_STRING) {
        element = reply.str;
        return true;
    }
    if (reply.type == ReplyType::NIL) {
        element.clear();
        return true;
    }
    return false;
}

// ==================== Set 操作实现 ====================

bool RedisClient::SAdd(const std::string& key, const std::string& member, int64_t& added) {
    auto reply = ExecuteCommand("SADD", {key, member});
    if (reply.type == ReplyType::INTEGER) {
        added = reply.integer;
        return true;
    }
    return false;
}

int64_t RedisClient::SRem(const std::string& key, const std::string& member) {
    auto reply = ExecuteCommand("SREM", {key, member});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return 0;
}

int64_t RedisClient::SIsMember(const std::string& key, const std::string& member) {
    auto reply = ExecuteCommand("SISMEMBER", {key, member});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return 0;
}

bool RedisClient::SMembers(const std::string& key, std::vector<std::string>& members) {
    auto reply = ExecuteCommand("SMEMBERS", {key});
    if (reply.type == ReplyType::ARRAY) {
        members.clear();
        for (const auto& item : reply.array) {
            if (item.type == ReplyType::BULK_STRING) {
                members.push_back(item.str);
            }
        }
        return true;
    }
    return false;
}

int64_t RedisClient::SCard(const std::string& key) {
    auto reply = ExecuteCommand("SCARD", {key});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return 0;
}

// ==================== ZSet 操作实现 ====================

bool RedisClient::ZAdd(const std::string& key, double score, const std::string& member, int64_t& added) {
    auto reply = ExecuteCommand("ZADD", {key, std::to_string(score), member});
    if (reply.type == ReplyType::INTEGER) {
        added = reply.integer;
        return true;
    }
    return false;
}

int64_t RedisClient::ZRem(const std::string& key, const std::string& member) {
    auto reply = ExecuteCommand("ZREM", {key, member});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return 0;
}

bool RedisClient::ZScore(const std::string& key, const std::string& member, double& score) {
    auto reply = ExecuteCommand("ZSCORE", {key, member});
    if (reply.type == ReplyType::BULK_STRING) {
        try {
            score = std::stod(reply.str);
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

bool RedisClient::ZRange(const std::string& key, int64_t start, int64_t stop, 
                        std::vector<std::string>& members) {
    auto reply = ExecuteCommand("ZRANGE", {key, std::to_string(start), std::to_string(stop)});
    if (reply.type == ReplyType::ARRAY) {
        members.clear();
        for (const auto& item : reply.array) {
            if (item.type == ReplyType::BULK_STRING) {
                members.push_back(item.str);
            }
        }
        return true;
    }
    return false;
}

bool RedisClient::ZRangeWithScores(const std::string& key, int64_t start, int64_t stop,
                                   std::vector<std::pair<double, std::string>>& out) {
    out.clear();
    std::vector<std::string> args = {key, std::to_string(start), std::to_string(stop), "WITHSCORES"};
    auto reply = ExecuteCommand("ZRANGE", args);
    if (reply.type != ReplyType::ARRAY || reply.array.size() % 2 != 0) return false;
    for (size_t i = 0; i + 1 < reply.array.size(); i += 2) {
        const auto& scoreItem = reply.array[i + 1];
        const auto& memberItem = reply.array[i];
        if (memberItem.type != ReplyType::BULK_STRING && memberItem.type != ReplyType::STATUS) continue;
        double score = 0;
        if (scoreItem.type == ReplyType::BULK_STRING || scoreItem.type == ReplyType::STATUS) {
            try {
                score = std::stod(scoreItem.str);
            } catch (...) {
                continue;
            }
        }
        out.push_back(std::make_pair(score, memberItem.str));
    }
    return true;
}

bool RedisClient::ZRevRange(const std::string& key, int64_t start, int64_t stop, 
                           std::vector<std::string>& members) {
    auto reply = ExecuteCommand("ZREVRANGE", {key, std::to_string(start), std::to_string(stop)});
    if (reply.type == ReplyType::ARRAY) {
        members.clear();
        for (const auto& item : reply.array) {
            if (item.type == ReplyType::BULK_STRING) {
                members.push_back(item.str);
            }
        }
        return true;
    }
    return false;
}

bool RedisClient::ZRevRangeWithScores(const std::string& key, int64_t start, int64_t stop,
                                     std::vector<std::pair<double, std::string>>& out) {
    out.clear();
    std::vector<std::string> args = {key, std::to_string(start), std::to_string(stop), "WITHSCORES"};
    auto reply = ExecuteCommand("ZREVRANGE", args);
    if (reply.type != ReplyType::ARRAY || reply.array.size() % 2 != 0) return false;
    for (size_t i = 0; i + 1 < reply.array.size(); i += 2) {
        const auto& memberItem = reply.array[i];
        const auto& scoreItem = reply.array[i + 1];
        if (memberItem.type != ReplyType::BULK_STRING && memberItem.type != ReplyType::STATUS) continue;
        double score = 0;
        if (scoreItem.type == ReplyType::BULK_STRING || scoreItem.type == ReplyType::STATUS) {
            try {
                score = std::stod(scoreItem.str);
            } catch (...) {
                continue;
            }
        }
        out.push_back(std::make_pair(score, memberItem.str));
    }
    return true;
}

bool RedisClient::ZIncrBy(const std::string& key, double delta, const std::string& member, double& newScore) {
    auto reply = ExecuteCommand("ZINCRBY", {key, std::to_string(delta), member});
    if (reply.type == ReplyType::BULK_STRING || reply.type == ReplyType::STATUS) {
        try {
            newScore = std::stod(reply.str);
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

int64_t RedisClient::ZCard(const std::string& key) {
    auto reply = ExecuteCommand("ZCARD", {key});
    if (reply.type == ReplyType::INTEGER) {
        return reply.integer;
    }
    return 0;
}

}
}

