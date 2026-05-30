#ifndef LSTL_ASSOCIATIVE_H
#define LSTL_ASSOCIATIVE_H

// 关联容器（有序：红黑树 / B+ 树 / 跳表；无序：哈希表）
#include "associative/bmap.h"
#include "associative/bset.h"
#include "associative/map.h"
#include "associative/multimap.h"
#include "associative/multiset.h"
#include "associative/set.h"
#include "associative/skip_map.h"
#include "associative/skip_multimap.h"
#include "associative/skip_multiset.h"
#include "associative/skip_set.h"
#include "associative/unordered_map.h"
#include "associative/unordered_multimap.h"
#include "associative/unordered_multiset.h"
#include "associative/unordered_set.h"

namespace lstl {

template <typename Key, typename Hash = hash<Key>, typename Pred = equal_to<Key>,
          typename Alloc = allocator<Key> >
class hash_set : public unordered_set<Key, Hash, Pred, Alloc> {
 public:
  hash_set() : unordered_set<Key, Hash, Pred, Alloc>() {}
  explicit hash_set(typename unordered_set<Key, Hash, Pred, Alloc>::size_type n,
                    const Hash& hf = Hash(), const Pred& eq = Pred())
      : unordered_set<Key, Hash, Pred, Alloc>(n, hf, eq) {}
  template <typename InputIterator>
  hash_set(InputIterator first, InputIterator last,
           typename unordered_set<Key, Hash, Pred, Alloc>::size_type n = 0,
           const Hash& hf = Hash(), const Pred& eq = Pred())
      : unordered_set<Key, Hash, Pred, Alloc>(first, last, n, hf, eq) {}
};

template <typename Key, typename T, typename Hash = hash<Key>, typename Pred = equal_to<Key>,
          typename Alloc = allocator<pair<const Key, T> > >
class hash_map : public unordered_map<Key, T, Hash, Pred, Alloc> {
 public:
  hash_map() : unordered_map<Key, T, Hash, Pred, Alloc>() {}
  explicit hash_map(typename unordered_map<Key, T, Hash, Pred, Alloc>::size_type n,
                    const Hash& hf = Hash(), const Pred& eq = Pred())
      : unordered_map<Key, T, Hash, Pred, Alloc>(n, hf, eq) {}
  template <typename InputIterator>
  hash_map(InputIterator first, InputIterator last,
           typename unordered_map<Key, T, Hash, Pred, Alloc>::size_type n = 0,
           const Hash& hf = Hash(), const Pred& eq = Pred())
      : unordered_map<Key, T, Hash, Pred, Alloc>(first, last, n, hf, eq) {}
};

}  // namespace lstl

#endif  // LSTL_ASSOCIATIVE_H
