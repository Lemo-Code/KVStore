# lstl 全方位 Bug 审计报告

> 审计日期: 2025-06-15
> 审计范围: 39 个头文件, 1520 行测试, 4 个基准
> 方法: 3 个并行 Agent + 手工边缘测试

---

## 一、审计摘要

| 维度 | 数量 |
|------|------|
| **总 Bug 数** | **69** |
| Critical (崩溃/泄漏/数据损坏) | 8 |
| High (功能缺失/严重性能退化) | 28 |
| Medium (API 不全/次优实现) | 25 |
| Low (代码质量/小改进) | 8 |
| **已修复** | **3** (successor 迭代/B+树 value_type/sentinel_) |

---

## 二、Critical Bugs (需立即修复)

### Bug 1: slist `const_iterator = const iterator` — 根本性 const 错误
- **文件**: `container/slist.h:88`
- **严重度**: 🔴 CRITICAL
- **问题**: `typedef const iterator const_iterator;` 导致 const slist 完全不可用。`const_iterator` 的 `operator++` 是非 const 的（因为 iterator 的 operator++ 是非 const），编译失败。同时 `operator*` 返回 `T&` 而非 `const T&`，破坏 const 正确性。
- **修复**: 编写独立的 `const_iterator` 类

### Bug 2: bmap/bset `const_iterator = const iterator` — 同上
- **文件**: `container/bmap.h:98`, `container/bset.h:71`
- **严重度**: 🔴 CRITICAL
- **修复**: 同上

### Bug 3: deque `clear()` 泄漏所有 buffer 和 map
- **文件**: `container/deque.h:227-234`
- **严重度**: 🔴 CRITICAL
- **问题**: `clear()` 没有释放任何 buffer 或 map 数组。每个 buffer (64 T) 和 map (8 entries) 在析构时泄漏。
- **修复**: clear() 遍历 start_ 到 finish_ 的所有 buffer，销毁元素后 deallocate；~deque() 额外释放 map

### Bug 4: unordered 系列 `erase(iterator)` 返回无效迭代器
- **文件**: `container/unordered_map.h:106`, `unordered_set.h:81-83`, `unordered_multimap.h:65-69`, `unordered_multiset.h:64-68`
- **严重度**: 🔴 CRITICAL
- **问题**: `erase(it)` 调用 `hashtable::erase(it)` (返回 void)，然后返回已失效的 `it`。标准要求返回下一个有效迭代器。
- **修复**: erase 前计算 next 迭代器，或修改 hashtable::erase 返回下一位置

### Bug 5: `uninitialized_copy` POD 路径对非随机访问迭代器错误
- **文件**: `memory/uninitialized.h`
- **严重度**: 🔴 CRITICAL
- **问题**: POD 路径使用 `result + n` (要求随机访问) 且 `std::distance` 消费输入迭代器。对 forward-only 迭代器会编译失败或行为错误。
- **修复**: 限制 POD 路径仅在随机访问迭代器时启用

### Bug 6: `uninitialized_copy` 使用 `is_trivially_copy_constructible` 而非 `is_trivially_copyable`
- **文件**: `memory/uninitialized.h`
- **严重度**: 🔴 CRITICAL
- **问题**: memcpy 到未初始化内存要求完整 `is_trivially_copyable`。有平凡拷贝构造但非平凡析构的类型会被错误地 memcpy，导致 UB。
- **修复**: 使用 `std::is_trivially_copyable` 或组合检查

### Bug 7: pool `reallocate` 在第二次分配失败时泄漏旧内存
- **文件**: `memory/pool.h`
- **严重度**: 🔴 CRITICAL
- **问题**: `reallocate` 先 `allocate(new_size)`，如果成功再 `deallocate(p, old)`。如果 allocate 抛异常，p 保持有效但已由旧的 deallocate 处理... 实际问题是：`void* new_p = allocate(new_size);` 成功后，如果后续 `memcpy` 或 `deallocate` 抛异常，new_p 泄漏。
- **修复**: 使用 try-catch 保护

### Bug 8: temporary_buffer 整数溢出
- **文件**: `memory/temporary_buffer.h`
- **严重度**: 🔴 CRITICAL
- **问题**: `size_t bytes = static_cast<size_t>(n) * sizeof(T);` 可能溢出。n 是 ptrdiff_t，可能为负数（但已有 n<=0 检查）。极端大小下乘法溢出。
- **修复**: 使用溢出检查或限制最大分配

---

## 三、High Bugs (严重影响)

### Move 语义缺失 (10 bugs)
| Bug | 文件 | 问题 |
|-----|------|------|
| 9 | vector.h | insert_aux 不接受 T&&，move push_back 退化为 copy |
| 10 | list.h | 无 move assignment operator |
| 11 | slist.h | 无 move constructor / move assignment |
| 12 | deque.h | 无 move constructor / move assignment |
| 13 | map.h | 无 copy assignment operator |
| 14 | set.h | 无显式 copy/move operations |
| 15 | unordered_*×4 | 无 move operations，退化为 copy |
| 16 | priority_queue.h | 无 move constructor / assignment |
| 17 | bmap/bset | 无 copy/move operations |
| 18 | skip_map/set | erase(iterator) 返回 void 而非 iterator |

### SFINAE 缺失 (3 bugs)
| Bug | 文件 | 问题 |
|-----|------|------|
| 19 | list.h:103 | `list<int>(5,10)` 匹配 InputIterator 构造函数 → crash |
| 20 | slist.h:105 | 同上 |
| 21 | set.h:46 | `set<int>(5,10)` → crash |

### Swap O(n) 而非 O(1) (6 bugs)
| Bug | 文件 | 问题 |
|-----|------|------|
| 22 | map.h:155 | `lstl::swap(tree_,...)` 退化为 copy O(n) |
| 23 | set.h:98 | 同上 |
| 24 | multimap.h:117 | 同上 |
| 25 | multiset.h:105 | 同上 |
| 26 | unordered_map.h:120 | hashtable 无成员 swap |
| 27 | skip_map.h:108 | skip_list 无成员 swap |

### 其他 High (4 bugs)
| Bug | 文件 | 问题 |
|-----|------|------|
| 28 | bmap/bset | insert 返回 void 而非 pair<iterator,bool> |
| 29 | bmap/bset | 无 begin() const / end() const / find() const |
| 30 | deque.h | pop_back/pop_front 清空 buffer 但不判断最后一 buffer |
| 31 | list.h:157 | size() O(n) 而非 O(1) |

---

## 四、Medium Bugs (API 不完整/次优)

| Bug | 文件 | 问题 |
|-----|------|------|
| 32-39 | vector/list/slist/deque/stack/queue/priority_queue | 缺少 emplace/emplace_back |
| 40-43 | map/set/skip_map/bmap | 缺少 move insert |
| 44-47 | stack/queue/priority_queue | 缺少 push(T&&) |
| 48 | multimap/multiset | erase(key) O(k log n) 而非 O(log n + k) |
| 49 | list.h:103 | `reserve` 缺少异常安全保护 (new 失败时泄漏) |
| 50 | priority_queue.h | 无 move operations |
| 51 | rb_tree | swap 交换 key_compare_ 但 key_of_value_ 未被交换 |
| 52 | slist.h:118 | copy ctor 双次反转 (O(2n) 而非 O(n)) |
| 53 | hashtable:359 | erase 返回 void 而非下一迭代器 |
| 54 | pool.h | 全局锁竞争，多线程性能退化 (已知) |
| 55 | functional.h | hash<long long> 在 32 位平台 UB (右移 32 位) |
| 56 | bmap.h:72 | iterator typedef 遮蔽外部 value_type (编译警告) ✅ 已修复 |

---

## 五、Low Bugs (代码质量)

| Bug | 文件 | 问题 |
|-----|------|------|
| 57 | vector.h:181 | 空 vector 拷贝构造分配 capacity=1 而非 0 |
| 58 | list.h:148 | end() const 中使用 const_cast |
| 59-64 | 全部容器 | 缺少 cbegin/cend/crbegin/crend |
| 65-69 | 全部容器 | 缺少比较运算符 (==, !=, <, >) |

---

## 六、已修复 Bug

| Bug | 修复内容 | 状态 |
|-----|---------|------|
| rb_tree successor | 单节点树迭代死循环 → 添加 sentinel_ 指针 + climbed 标志 | ✅ |
| bmap value_type 警告 | iterator typedef 遮蔽外部 value_type | ✅ |
| uninitialized_copy 返回值 | POD 路径 result 未推进 | ✅ (之前修复) |
| map move constructor | 缺失导致 move 后源非空 | ✅ (之前修复) |
| vector move assignment | 缺失导致 move 退化为 copy | ✅ (之前修复) |
| pool 线程安全 | 添加 recursive_mutex | ✅ (之前修复) |

---

## 七、修复优先级

### P0 — 本周必须修复 (影响正确性)
1. deque 内存泄漏 (Bug 3)
2. unordered erase 返回无效迭代器 (Bug 4)
3. uninitialized_copy 类型检测 (Bug 5, 6)

### P1 — 两周内修复 (影响可用性)
4. slist/bmap/bset const_iterator (Bug 1, 2)
5. list/slist/set SFINAE 保护 (Bug 19-21)
6. 全局 swap O(n) (Bug 22-27)

### P2 — 一个月内修复 (API 完整性)
7. Move 语义补全 (共 ~10 个)
8. emplace 系列接口
9. const 迭代器 (cbegin/cend)

### P3 — 后续迭代
10. 比较运算符
11. 性能优化 (size O(1) 等)
