#include <execinfo.h>
#include <cxxabi.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace zero {

std::string BacktraceToString(int skip) {
    void* array[64];
    int size = backtrace(array, 64);
    char** symbols = backtrace_symbols(array, size);

    std::stringstream ss;
    for (int i = skip; i < size && symbols != nullptr; ++i) {
        // 尝试 demangle C++ 符号名
        const char* symbol = symbols[i];
        // 格式: "binary(mangled_symbol+offset) [address]"
        const char* begin = strchr(symbol, '(');
        const char* end = begin ? strchr(begin, '+') : nullptr;

        if (begin && end && (end - begin > 1)) {
            std::string mangled(begin + 1, end);
            int status = 0;
            char* demangled = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
            if (status == 0 && demangled) {
                ss << "  #" << (i - skip) << " " << demangled << "\n";
                free(demangled);
                continue;
            }
        }
        ss << "  #" << (i - skip) << " " << symbol << "\n";
    }

    free(symbols);
    return ss.str();
}

uint32_t GetThreadId() {
    return static_cast<uint32_t>(syscall(SYS_gettid));
}

uint64_t GetCurrentMS() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return static_cast<uint64_t>(ms.count());
}

} // namespace zero
