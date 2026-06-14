#ifndef __SYLAR_DB_REDIS_CLIENT_H__
#define __SYLAR_DB_REDIS_CLIENT_H__

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "sylar/socket.h"
#include "sylar/address.h"
#include "sylar/mutex.h"

namespace sylar {
namespace db {

/**
 * @brief 基于 sylar 协程库的 Redis 客户端实现
 * 完全兼容协程环境，无需 hiredis
 */
class RedisClient {
public:
    /**
     * @brief Redis 响应类型
     */
    enum class ReplyType {
        NIL = 0,        // 空响应
        STATUS = 1,     // 状态响应 (如 "OK")
        ERROR = 2,      // 错误响应
        INTEGER = 3,    // 整数响应
        BULK_STRING = 4,// 批量字符串
        ARRAY = 5       // 数组响应
    };

    /**
     * @brief Redis 响应对象
     */
    struct Reply {
        ReplyType type;
        std::string str;              // 用于 STATUS, ERROR, BULK_STRING
        int64_t integer;              // 用于 INTEGER
        std::vector<Reply> array;     // 用于 ARRAY
        
        Reply() : type(ReplyType::NIL), integer(0) {}
        
        bool isOK() const { 
            return type == ReplyType::STATUS && str == "OK"; 
        }
        
        bool isError() const { 
            return type == ReplyType::ERROR; 
        }
        
        bool isNil() const { 
            return type == ReplyType::NIL; 
        }
    };

public:
    RedisClient();
    ~RedisClient();

    /**
     * @brief 连接到 Redis 服务器
     * @param host Redis 服务器地址
     * @param port Redis 服务器端口
     * @param password Redis 密码（可选）
     * @param timeout_ms 命令读写超时（毫秒）
     * @param connect_timeout_ms 建连超时（毫秒），0 表示使用 timeout_ms
     * @return 是否连接成功
     */
    bool Connect(const std::string& host = "127.0.0.1",
                 uint32_t port = 6379,
                 const std::string& password = "",
                 uint64_t timeout_ms = 5000,
                 uint64_t connect_timeout_ms = 0);

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
     * @brief 执行 Redis 命令（使用参数数组）
     * @param cmd 命令名
     * @param args 参数列表
     * @return 响应对象
     */
    Reply ExecuteCommand(const std::string& cmd, 
                        const std::vector<std::string>& args = {});

    /**
     * @brief 执行 Redis 命令（可变参数版本）
     * @param cmd 命令名
     * @param args 参数列表（可变参数）
     */
    template<typename... Args>
    Reply ExecuteCommand(const std::string& cmd, Args... args) {
        std::vector<std::string> argVec = {args...};
        return ExecuteCommand(cmd, argVec);
    }

    /**
     * @brief 获取最后的错误信息
     */
    std::string getError() const { return lastError_; }

    /**
     * @brief 获取最后的错误码
     */
    int getErrorCode() const { return lastErrorCode_; }

    // ==================== 字符串操作 ====================
    bool Set(const std::string& key, const std::string& value, int64_t expireTime = 0);
    bool Get(const std::string& key, std::string& value);
    int64_t Del(const std::string& key);
    /** 删除多个 key，返回删除个数 */
    int64_t Del(const std::vector<std::string>& keys);
    int64_t Exists(const std::string& key);
    bool Expire(const std::string& key, int64_t seconds);
    /** 毫秒级过期 */
    bool Pexpire(const std::string& key, int64_t milliseconds);
    int64_t TTL(const std::string& key);
    bool Keys(const std::string& pattern, std::vector<std::string>& keys);
    /** 批量获取，与 keys 顺序一致，不存在的 key 对应空字符串 */
    bool MGet(const std::vector<std::string>& keys, std::vector<std::string>& values);
    /** 批量设置 key-value */
    bool MSet(const std::map<std::string, std::string>& kvs);
    /** 自增 1，返回新值，失败返回 -1 */
    int64_t Incr(const std::string& key);
    int64_t IncrBy(const std::string& key, int64_t delta);
    int64_t Decr(const std::string& key);
    int64_t DecrBy(const std::string& key, int64_t delta);
    /** 设置新值并返回旧值，key 不存在时旧值为空 */
    bool GetSet(const std::string& key, const std::string& value, std::string& oldValue);

    // ==================== Hash 操作 ====================
    bool HSet(const std::string& key, const std::string& field, 
              const std::string& value, int64_t& changed);
    /** HSet 不关心 changed 的重载 */
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
    /** 带分数返回，pair: (score, member) */
    bool ZRangeWithScores(const std::string& key, int64_t start, int64_t stop,
                          std::vector<std::pair<double, std::string>>& out);
    bool ZRevRange(const std::string& key, int64_t start, int64_t stop, 
                   std::vector<std::string>& members);
    bool ZRevRangeWithScores(const std::string& key, int64_t start, int64_t stop,
                             std::vector<std::pair<double, std::string>>& out);
    /** 增加成员分数，返回新分数 */
    bool ZIncrBy(const std::string& key, double delta, const std::string& member, double& newScore);
    int64_t ZCard(const std::string& key);

private:
    /**
     * @brief 构建 RESP 协议格式的命令
     */
    std::string buildCommand(const std::string& cmd, const std::vector<std::string>& args);

    /**
     * @brief 发送数据到 Redis 服务器
     * @param needLock 是否在内部加读锁（ExecuteCommand 已持写锁时传 false 避免重复加锁）
     */
    bool sendData(const std::string& data, bool needLock = true);

    /**
     * @brief 从 Redis 服务器接收一行数据（以 \r\n 结尾）
     */
    bool recvLine(std::string& line, bool needLock = true);

    /**
     * @brief 从 Redis 服务器接收指定长度的数据
     */
    bool recvData(void* buffer, size_t length, bool needLock = true);

    /**
     * @brief 解析 Redis 响应
     * @param underLock 是否由已持锁的 ExecuteCommand 调用（内部使用无锁 recv，避免死锁）
     */
    Reply parseReply(bool underLock = false);

    /** 无锁内部实现（调用方已持 mutex 时使用） */
    bool sendDataUnlocked(const std::string& data);
    bool recvLineUnlocked(std::string& line);
    bool recvDataUnlocked(void* buffer, size_t length);
    /** 保证 recvLineBuf_ 中至少有 n 字节（从 socket 读入），供 bulk/行解析统一从缓冲取 */
    bool ensureRecvBufUnlocked(size_t n);

    Reply parseArray(int64_t length, bool underLock = false);
    Reply parseBulkString(int64_t length, bool underLock = false);

    /**
     * @brief 设置错误信息
     */
    void setError(const std::string& error, int code = -1);

    /**
     * @brief 认证
     */
    bool authenticate(const std::string& password);

private:
    sylar::Socket::ptr socket_;       // Socket 连接
    mutable bool connected_;          // 是否已连接
    std::string lastError_;           // 最后错误信息
    int lastErrorCode_;               // 最后错误码
    mutable sylar::RWMutex mutex_;    // 读写锁保护连接状态
    std::string recvLineBuf_;         // recvLine 行缓冲，避免逐字节 recv
};

}
}

#endif // __SYLAR_DB_REDIS_CLIENT_H__

