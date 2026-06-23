#pragma once

#include <cstdio>
#include <string>
#include <vector>
#include <unistd.h>

namespace kvtest {

class TempFile {
public:
    explicit TempFile(const std::string& pattern) {
        path_ = pattern;
        auto pos = path_.find("XXXXXX");
        if (pos != std::string::npos) {
            std::vector<char> buf(path_.begin(), path_.end());
            buf.push_back('\0');
            int fd = mkstemp(buf.data());
            if (fd >= 0) {
                ::close(fd);
                path_.assign(buf.data());
            }
        }
    }

    ~TempFile() { std::remove(path_.c_str()); }

    const std::string& path() const { return path_; }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

private:
    std::string path_;
};

} // namespace kvtest
