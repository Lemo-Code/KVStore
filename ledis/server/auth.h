#pragma once

#include <string>

namespace ledis {

// ============================================================
// AuthManager — 密码认证
// ============================================================
class AuthManager {
public:
    bool enabled() const { return !password_.empty(); }
    bool check(const std::string& pwd) const { return pwd == password_; }
    void setPassword(const std::string& pwd) { password_ = pwd; }
    const std::string& password() const { return password_; }

private:
    std::string password_;
};

} // namespace ledis
