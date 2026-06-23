#pragma once
#include <string>
#include <sstream>
#include <utility>
#include <cstdint>

namespace zero { namespace kvstore {

enum ErrorCode : uint8_t {
    kOk         = 0,
    kNotFound   = 1,
    kConflict   = 2, 
    kIOError    = 3,
    kNotLeader  = 4,
    kTimeout    = 5,
    kInvalidArg = 6,
    kRedirect   = 7,
    kInternal   = 8,
};

class Status {
public:
    Status() : code_(kOk) {}
    Status(ErrorCode c, std::string msg) : code_(c), msg_(std::move(msg)) {}
    static Status OK() { return Status(); }
    static Status NotFound(const std::string& key) { return Status(kNotFound, "key not found: "+key); }
    static Status Conflict(const std::string& d) { return Status(kConflict, d); }
    static Status IOError(const std::string& d) { return Status(kIOError, d); }
    static Status NotLeader(const std::string& hint="") { Status s(kNotLeader, "not leader"); s.redirect_=hint; return s; }
    static Status Timeout(const std::string& op) { return Status(kTimeout, op+" timed out"); }
    static Status InvalidArg(const std::string& d) { return Status(kInvalidArg, d); }
    static Status Redirect(const std::string& node) { Status s(kRedirect, "redirect to "+node); s.redirect_=node; return s; }
    static Status Internal(const std::string& d) { return Status(kInternal, d); }
    bool ok() const { return code_ == kOk; }
    bool IsNotFound() const { return code_ == kNotFound; }
    bool IsConflict() const { return code_ == kConflict; }
    bool IsIOError() const { return code_ == kIOError; }
    bool IsNotLeader() const { return code_ == kNotLeader; }
    bool IsTimeout() const { return code_ == kTimeout; }
    bool IsRedirect() const { return code_ == kRedirect; }
    ErrorCode Code() const { return code_; }
    const std::string& Msg() const { return msg_; }
    const std::string& RedirectNode() const { return redirect_; }
    std::string ToString() const { if(ok()) return "OK"; std::ostringstream o; o<<(int)code_<<":"<<msg_; return o.str(); }
private:
    ErrorCode code_ = kOk;
    std::string msg_;
    std::string redirect_;
};

template <typename T> struct Result { Status status; T value; Result():status(kInternal,""){} Result(Status s):status(std::move(s)){} Result(T v):status(Status::OK()),value(std::move(v)){} Result(Status s,T v):status(std::move(s)),value(std::move(v)){} bool ok()const{return status.ok();} };

}} // namespace
