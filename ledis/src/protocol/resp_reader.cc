#include "ledis/protocol/resp_reader.h"

#include <cctype>
#include <cstdlib>
#include <string>

namespace ledis {
namespace {

bool findCrlf(const lemo::buffer::ChainBuffer& chain, size_t start,
              size_t* cr_pos) {
  const size_t n = chain.readable();
  if (start + 1 >= n) {
    return false;
  }
  for (size_t i = start; i + 1 < n; ++i) {
    const uint8_t* r = chain.peek_ptr(i);
    const uint8_t* l = chain.peek_ptr(i + 1);
    if (r && l && *r == '\r' && *l == '\n') {
      *cr_pos = i;
      return true;
    }
  }
  return false;
}

bool readBytes(const lemo::buffer::ChainBuffer& chain, size_t start,
               size_t len, std::string* out) {
  if (chain.readable() < start + len) {
    return false;
  }
  out->assign(len, '\0');
  for (size_t i = 0; i < len; ++i) {
    const uint8_t* p = chain.peek_ptr(start + i);
    if (!p) {
      return false;
    }
    (*out)[i] = static_cast<char>(*p);
  }
  return true;
}

bool parseInt64Line(const lemo::buffer::ChainBuffer& chain, size_t start,
                    size_t end, int64_t* out) {
  if (end <= start) {
    return false;
  }
  std::string s;
  s.resize(end - start);
  for (size_t i = start; i < end; ++i) {
    const uint8_t* p = chain.peek_ptr(i);
    if (!p) {
      return false;
    }
    s[i - start] = static_cast<char>(*p);
  }
  char* endptr = nullptr;
  const long long v = std::strtoll(s.c_str(), &endptr, 10);
  if (endptr != s.c_str() + static_cast<ptrdiff_t>(s.size())) {
    return false;
  }
  *out = static_cast<int64_t>(v);
  return true;
}

}  // namespace

RespReader::RespReader(lemo::buffer::ChainBuffer& chain,
                       const ProtocolLimits& limits)
    : chain_(chain), limits_(limits) {}

ParseResult RespReader::peekValueAt(size_t skip, RespValue* out,
                                    size_t* frame_len) const {
  if (!out || !frame_len) {
    return ParseResult::kProtocolError;
  }
  if (skip >= chain_.readable()) {
    return ParseResult::kNeedMore;
  }
  const uint8_t* type_ptr = chain_.peek_ptr(skip);
  if (!type_ptr) {
    return ParseResult::kNeedMore;
  }
  const char type = static_cast<char>(*type_ptr);

  if (type == '+' || type == '-') {
    size_t cr = 0;
    if (!findCrlf(chain_, skip + 1, &cr)) {
      return ParseResult::kNeedMore;
    }
    std::string payload;
    if (!readBytes(chain_, skip + 1, cr - (skip + 1), &payload)) {
      return ParseResult::kNeedMore;
    }
    out->type = (type == '+') ? RespType::kSimpleString : RespType::kError;
    out->bulk = Sds(std::move(payload));
    *frame_len = cr + 2 - skip;
    return ParseResult::kOk;
  }

  if (type == ':') {
    size_t cr = 0;
    if (!findCrlf(chain_, skip + 1, &cr)) {
      return ParseResult::kNeedMore;
    }
    int64_t v = 0;
    if (!parseInt64Line(chain_, skip + 1, cr, &v)) {
      return ParseResult::kProtocolError;
    }
    out->type = RespType::kInteger;
    out->integer = v;
    *frame_len = cr + 2 - skip;
    return ParseResult::kOk;
  }

  if (type == '$') {
    size_t cr = 0;
    if (!findCrlf(chain_, skip + 1, &cr)) {
      return ParseResult::kNeedMore;
    }
    int64_t len = 0;
    if (!parseInt64Line(chain_, skip + 1, cr, &len)) {
      return ParseResult::kProtocolError;
    }
    if (len < -1) {
      return ParseResult::kProtocolError;
    }
    if (len == -1) {
      out->type = RespType::kNull;
      out->bulk = Sds();
      *frame_len = cr + 2 - skip;
      return ParseResult::kOk;
    }
    if (static_cast<size_t>(len) > limits_.bulk_string_max) {
      return ParseResult::kProtocolError;
    }
    const size_t data_start = cr + 2;
    const size_t frame_end = data_start + static_cast<size_t>(len) + 2;
    if (chain_.readable() < frame_end) {
      return ParseResult::kNeedMore;
    }
    size_t tail_cr = 0;
    if (!findCrlf(chain_, data_start + static_cast<size_t>(len), &tail_cr) ||
        tail_cr != data_start + static_cast<size_t>(len)) {
      return ParseResult::kProtocolError;
    }
    std::string payload;
    if (!readBytes(chain_, data_start, static_cast<size_t>(len), &payload)) {
      return ParseResult::kNeedMore;
    }
    out->type = RespType::kBulkString;
    out->bulk = Sds(std::move(payload));
    *frame_len = frame_end - skip;
    return ParseResult::kOk;
  }

  if (type == '*') {
    size_t cr = 0;
    if (!findCrlf(chain_, skip + 1, &cr)) {
      return ParseResult::kNeedMore;
    }
    int64_t count = 0;
    if (!parseInt64Line(chain_, skip + 1, cr, &count)) {
      return ParseResult::kProtocolError;
    }
    if (count < 0) {
      return ParseResult::kProtocolError;
    }
    if (static_cast<size_t>(count) > limits_.argc_max) {
      return ParseResult::kProtocolError;
    }
    out->type = RespType::kArray;
    out->array.clear();
    out->array.reserve(static_cast<size_t>(count));
    size_t cursor = cr + 2;
    for (int64_t i = 0; i < count; ++i) {
      RespValue elem;
      size_t elem_len = 0;
      const ParseResult pr =
          peekValueAt(cursor, &elem, &elem_len);
      if (pr != ParseResult::kOk) {
        return pr;
      }
      out->array.push_back(std::move(elem));
      cursor += elem_len;
    }
    *frame_len = cursor - skip;
    return ParseResult::kOk;
  }

  return ParseResult::kProtocolError;
}

ParseResult RespReader::tryParseOne(RespValue* out, size_t* consumed) {
  if (!out || !consumed) {
    return ParseResult::kProtocolError;
  }
  *consumed = 0;
  if (chain_.readable() == 0) {
    return ParseResult::kNeedMore;
  }
  if (limits_.query_buffer_limit > 0 &&
      chain_.readable() > limits_.query_buffer_limit) {
    return ParseResult::kProtocolError;
  }
  size_t frame_len = 0;
  const ParseResult pr = peekValueAt(0, out, &frame_len);
  if (pr == ParseResult::kOk) {
    *consumed = frame_len;
  }
  return pr;
}

ParseResult RespReader::parseOne(RespValue* out) {
  size_t consumed = 0;
  const ParseResult pr = tryParseOne(out, &consumed);
  if (pr == ParseResult::kOk) {
    chain_.consume(consumed);
  }
  return pr;
}

void RespReader::uppercaseInPlace(Sds* s) {
  std::string upper = s->str();
  for (char& c : upper) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  *s = Sds(std::move(upper));
}

ParseResult RespReader::valueToCommand(const RespValue& val,
                                       Command* cmd) const {
  if (!cmd || val.type != RespType::kArray) {
    return ParseResult::kProtocolError;
  }
  if (val.array.empty()) {
    return ParseResult::kProtocolError;
  }
  cmd->name = val.array[0].bulk;
  cmd->args.clear();
  if (val.array[0].type != RespType::kBulkString) {
    return ParseResult::kProtocolError;
  }
  uppercaseInPlace(&cmd->name);
  for (size_t i = 1; i < val.array.size(); ++i) {
    if (val.array[i].type != RespType::kBulkString) {
      return ParseResult::kProtocolError;
    }
    cmd->args.push_back(val.array[i].bulk);
  }
  return ParseResult::kOk;
}

ParseResult RespReader::tryParseCommand(Command* cmd, size_t* consumed) {
  if (!cmd || !consumed) {
    return ParseResult::kProtocolError;
  }
  RespValue val;
  const ParseResult pr = tryParseOne(&val, consumed);
  if (pr != ParseResult::kOk) {
    return pr;
  }
  return valueToCommand(val, cmd);
}

ParseResult RespReader::parseCommand(Command* cmd) {
  size_t consumed = 0;
  const ParseResult pr = tryParseCommand(cmd, &consumed);
  if (pr == ParseResult::kOk) {
    chain_.consume(consumed);
  }
  return pr;
}

}  // namespace ledis
