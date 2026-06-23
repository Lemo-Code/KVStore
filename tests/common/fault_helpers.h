#pragma once

#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>

namespace kvtest {

inline bool writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    ofs << content;
    return ofs.good();
}

inline bool chmodPath(const std::string& path, mode_t mode) {
    return ::chmod(path.c_str(), mode) == 0;
}

inline std::string randomPath(const std::string& prefix) {
    return prefix + std::to_string(::getpid()) + "_" +
           std::to_string(static_cast<unsigned>(::time(nullptr)));
}

} // namespace kvtest
