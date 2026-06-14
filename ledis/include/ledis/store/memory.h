#pragma once

#include "ledis/store/object.h"
#include "ledis/types.h"

namespace ledis {

/** 估算 key + value 占用的内存字节数（近似值，用于 maxmemory 淘汰）。 */
size_t estimateSdsMemory(const Sds& s);
size_t estimateObjectMemory(const LedisObject& obj);
size_t estimateEntryMemory(const Sds& key, const LedisObject& obj);

}  // namespace ledis
