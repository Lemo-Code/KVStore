#include "ledis/protocol/resp_value.h"

namespace ledis {

RespValue RespValue::makeSimpleString(Sds s) {
  RespValue v;
  v.type = RespType::kSimpleString;
  v.bulk = Move(s);
  return v;
}

RespValue RespValue::makeError(Sds msg) {
  RespValue v;
  v.type = RespType::kError;
  v.bulk = Move(msg);
  return v;
}

RespValue RespValue::makeInteger(int64_t n) {
  RespValue v;
  v.type = RespType::kInteger;
  v.integer = n;
  return v;
}

RespValue RespValue::makeBulk(Sds s) {
  RespValue v;
  v.type = RespType::kBulkString;
  v.bulk = Move(s);
  return v;
}

RespValue RespValue::makeNull() {
  RespValue v;
  v.type = RespType::kNull;
  return v;
}

RespValue RespValue::makeArray(StdVector<RespValue> elems) {
  RespValue v;
  v.type = RespType::kArray;
  v.array = Move(elems);
  return v;
}

}  // namespace ledis
