#ifndef LSTL_HASH_FUNCTORS_H
#define LSTL_HASH_FUNCTORS_H

#include "hash.h"

namespace lstl {
namespace detail {

template <typename Key, typename Hash>
struct hashtable_hash_key {
  Hash h;
  hashtable_hash_key() : h() {}
  explicit hashtable_hash_key(const Hash& hf) : h(hf) {}

  template <typename Value>
  size_t operator()(const Value& v) const {
    return h(v.first);
  }
};

template <typename Key, typename Pred>
struct hashtable_equal_key {
  Pred p;
  hashtable_equal_key() : p() {}
  explicit hashtable_equal_key(const Pred& eq) : p(eq) {}

  template <typename Value>
  bool operator()(const Value& a, const Value& b) const {
    return p(a.first, b.first);
  }
};

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_HASH_FUNCTORS_H
