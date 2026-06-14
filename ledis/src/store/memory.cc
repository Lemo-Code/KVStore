#include "ledis/store/memory.h"

#include <cmath>
#include <string>

namespace ledis {
namespace {

constexpr size_t kKeyEntryOverhead = 64;

}  // namespace

size_t estimateSdsMemory(const Sds& s) {
  return s.size() + 24;
}

size_t estimateObjectMemory(const LedisObject& obj) {
  if (obj.isString()) {
    Sds value;
    if (!obj.asString(&value)) {
      return 16;
    }
    return estimateSdsMemory(value) + 16;
  }
  if (obj.isHash()) {
    size_t bytes = 32;
    const HashDict* hash = obj.asHash();
    if (hash) {
      for (HashDict::const_iterator it = hash->begin(); it != hash->end(); ++it) {
        bytes += estimateSdsMemory(it->first) + estimateSdsMemory(it->second) + 16;
      }
    }
    return bytes;
  }
  if (obj.isList()) {
    size_t bytes = 32;
    const ListDeque* list = obj.asList();
    if (list) {
      for (size_t i = 0; i < list->size(); ++i) {
        bytes += estimateSdsMemory((*list)[i]) + 8;
      }
    }
    return bytes;
  }
  if (obj.isSet()) {
    size_t bytes = 32;
    const SdsSet* set = obj.asSet();
    if (set) {
      for (SdsSet::const_iterator it = set->begin(); it != set->end(); ++it) {
        bytes += estimateSdsMemory(*it) + 8;
      }
    }
    return bytes;
  }
  if (obj.isZset()) {
    size_t bytes = 32;
    const ZsetDict* zset = obj.asZset();
    if (zset) {
      for (ZsetDict::const_iterator it = zset->begin(); it != zset->end(); ++it) {
        bytes += estimateSdsMemory(it->first) + 24;
      }
    }
    return bytes;
  }
  return 16;
}

size_t estimateEntryMemory(const Sds& key, const LedisObject& obj) {
  return kKeyEntryOverhead + estimateSdsMemory(key) + estimateObjectMemory(obj);
}

}  // namespace ledis
