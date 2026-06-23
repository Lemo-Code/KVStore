#pragma once
#include "kvstore/common/kv_error.h"
#include <string>
#include <functional>
namespace zero { namespace kvstore {
using RequestHandler = std::function<Status(const std::string& req, std::string& rsp)>;
class ClientSession {
public:
    ClientSession(RequestHandler h) : handler_(std::move(h)) {}
    Status Process(const std::string& input, std::string& output);
private:
    RequestHandler handler_;
    std::string buffer_;
};
}} // namespace
