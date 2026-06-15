#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "ledis/cmd/cmd_context.h"

namespace ledis {

// ============================================================
// 命令标志位
// ============================================================
enum CmdFlags : uint32_t {
    CMD_READONLY = 1u << 0,   // 只读命令 (未来可由 IO 线程直接处理)
    CMD_WRITE    = 1u << 1,   // 写命令
    CMD_FAST     = 1u << 2,   // O(1) 时间复杂度
    CMD_DENYOOM  = 1u << 3,   // OOM 时拒绝执行
    CMD_NOSCRIPT = 1u << 4,   // 不可用于 Lua 脚本
    CMD_PUBSUB   = 1u << 5,   // Pub/Sub 相关
};

// ============================================================
// CmdHandler — 命令处理函数指针类型
// ============================================================
using CmdHandler = void (*)(CmdContext& ctx);

// ============================================================
// CmdInfo — 命令注册信息
// ============================================================
struct CmdInfo {
    const char* name;        // 命令名 (小写)
    CmdHandler  handler;     // 处理函数
    int         arity;       // 参数个数:
                             //   >0: 精确个数 (含命令名本身)
                             //   -n: 至少 n 个参数
                             //   e.g. 2=恰好2个参数, -2=至少2个参数
    uint32_t    flags;       // 标志位
    const char* sflags;      // 字符串标志 (兼容 Redis 风格 "rwF..")
    uint64_t    calls = 0;   // 调用统计
    uint64_t    usecs = 0;   // 累计微秒

    // 检查参数个数是否正确
    bool checkArity(int argc) const {
        if (arity > 0) {
            return argc == arity;
        } else {
            return argc >= (-arity);
        }
    }
};

// ============================================================
// 命令注册表
// ============================================================
// 所有命令在 cmd_table.cc 中注册，按字母排序以支持二分查找
extern const CmdInfo* g_cmd_table;
extern const size_t   g_cmd_table_size;

// 查找命令 (二分查找，O(log N))
const CmdInfo* lookupCmd(std::string_view name);

// 命令分发入口 (在 cmd_table.cc 中实现)
class StorageEngine;
void dispatchCommand(StorageEngine& engine, CmdContext& ctx);

} // namespace ledis
