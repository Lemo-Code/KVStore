#pragma once

#include "ledis/types.h"

namespace ledis {

enum class RespType {
  kSimpleString,
  kError,
  kInteger,
  kBulkString,
  kNull,
  kArray,
};

struct RespValue {
  RespType type = RespType::kSimpleString;
  int64_t integer = 0;
  Sds bulk;
  StdVector<RespValue> array;

  bool isError() const { return type == RespType::kError; }
  bool isNullBulk() const { return type == RespType::kNull; }

  static RespValue makeSimpleString(Sds s);
  static RespValue makeError(Sds msg);
  static RespValue makeInteger(int64_t n);
  static RespValue makeBulk(Sds s);
  static RespValue makeNull();
  static RespValue makeArray(StdVector<RespValue> elems);
};

using RespArray = StdVector<RespValue>;

}  // namespace ledis
