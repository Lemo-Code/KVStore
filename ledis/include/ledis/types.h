#pragma once

/**
 * @file types.h
 * @brief Ledis 统一类型别名：业务代码只使用本文件中的 using，不直接写 std:: / lstl::。
 *
 * 分层：
 *   String / Std* / Move / UniquePtr — 全模块通用（Protocol/Session/Store 头文件均可 include）
 *   Vector / Deque / SdsDict / HashDict / ListDeque — 见 containers.h（仅 Store 链 lstl）
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <type_traits>
#include <utility>
#include <vector>

namespace ledis {

// ---------------------------------------------------------------------------
// 标准库别名（Protocol / Session / Server 层默认使用）
// ---------------------------------------------------------------------------
using String = std::string;
using Size = std::size_t;

template <typename T>
using StdVector = std::vector<T>;

template <typename T>
using StdDeque = std::deque<T>;

template <typename K, typename V, typename Hash = std::hash<K>,
          typename Eq = std::equal_to<K>>
using StdUnorderedMap = std::unordered_map<K, V, Hash, Eq>;

template <typename K, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>>
using StdUnorderedSet = std::unordered_set<K, Hash, Eq>;

template <typename T>
using UniquePtr = std::unique_ptr<T>;

template <typename T>
using SharedPtr = std::shared_ptr<T>;

template <typename Sig>
using Function = std::function<Sig>;

template <typename T, Size N>
using Array = std::array<T, N>;

template <typename T>
inline typename std::remove_reference<T>::type&& Move(T&& value) noexcept {
  return static_cast<typename std::remove_reference<T>::type&&>(value);
}

}  // namespace ledis

#include "ledis/store/sds.h"

namespace ledis {

/** Command 参数列表（MPSC 路径暂用 std，后续可改为 SdsVector）。 */
using SdsArgList = StdVector<Sds>;

}  // namespace ledis
