#include "ledis/protocol/resp_writer.h"

#include <cstdio>

namespace ledis {

namespace {

String crlfLine(const String& prefix, const Sds& payload) {
  return prefix + payload.str() + "\r\n";
}

}  // namespace

String RespWriter::encodeSimpleString(const Sds& s) {
  return crlfLine("+", s);
}

String RespWriter::encodeError(const Sds& msg) {
  return crlfLine("-", msg);
}

String RespWriter::encodeError(const char* msg) {
  return encodeError(Sds(msg));
}

String RespWriter::encodeInteger(int64_t n) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(n));
  return String(":") + buf + "\r\n";
}

String RespWriter::encodeBulk(const Sds& s) {
  char len_buf[32];
  std::snprintf(len_buf, sizeof(len_buf), "%zu", s.size());
  return String("$") + len_buf + "\r\n" + s.str() + "\r\n";
}

String RespWriter::encodeNullBulk() {
  return "$-1\r\n";
}

String RespWriter::encodeArray(const RespArray& elems) {
  char count_buf[32];
  std::snprintf(count_buf, sizeof(count_buf), "%zu", elems.size());
  String out = String("*") + count_buf + "\r\n";
  for (const RespValue& v : elems) {
    out += encode(v);
  }
  return out;
}

String RespWriter::encodeCommand(const SdsArgList& argv) {
  char count_buf[32];
  std::snprintf(count_buf, sizeof(count_buf), "%zu", argv.size());
  String out = String("*") + count_buf + "\r\n";
  for (const Sds& arg : argv) {
    out += encodeBulk(arg);
  }
  return out;
}

String RespWriter::encodeCommand(const Command& cmd) {
  SdsArgList argv;
  argv.reserve(cmd.argc());
  argv.push_back(cmd.name);
  for (const Sds& arg : cmd.args) {
    argv.push_back(arg);
  }
  return encodeCommand(argv);
}

String RespWriter::encode(const RespValue& val) {
  switch (val.type) {
    case RespType::kSimpleString:
      return encodeSimpleString(val.bulk);
    case RespType::kError:
      return encodeError(val.bulk);
    case RespType::kInteger:
      return encodeInteger(val.integer);
    case RespType::kBulkString:
      return encodeBulk(val.bulk);
    case RespType::kNull:
      return encodeNullBulk();
    case RespType::kArray:
      return encodeArray(val.array);
  }
  return encodeError("ERR internal encode error");
}

}  // namespace ledis
