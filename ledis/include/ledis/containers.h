#pragma once

/**
 * @file containers.h
 * @brief Store 层 lstl 容器别名（仅 ledis_store 等链 lstl_memory 的目标 include）。
 */

#include "ledis/types.h"

#include "container.h"

namespace ledis {

struct SdsHash {
  Size operator()(const Sds& key) const {
    return static_cast<Size>(key.hash());
  }
};

struct SdsEqual {
  bool operator()(const Sds& lhs, const Sds& rhs) const { return lhs == rhs; }
};

template <typename T>
using Vector = lstl::vector<T>;

template <typename T>
using Deque = lstl::deque<T>;

template <typename Mapped>
using SdsDict = lstl::unordered_map<Sds, Mapped, SdsHash, SdsEqual>;

template <typename Mapped>
using UInt64Map = lstl::unordered_map<uint64_t, Mapped>;

using SdsVector = Vector<Sds>;

/** Hash field → value */
using HashDict = SdsDict<Sds>;

/** List 元素序列 */
using ListDeque = Deque<Sds>;

/** Set 成员集合 */
using SdsSet = lstl::unordered_set<Sds, SdsHash, SdsEqual>;

/** Zset member → score */
using ZsetDict = SdsDict<double>;

}  // namespace ledis

// lstl 已用于 Store::Keyspace::dict_（SdsDict）。
// Protocol/Session 层只 include types.h；勿链 lstl 到 ledis_protocol。
