#include "lemo/utils/thread_util.h"

#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace lemo {
namespace utils {
namespace {

thread_local std::string g_thread_name("unknown");

void apply_pthread_name(const std::string& name) {
  std::string trimmed = name.empty() ? "unknown" : name;
  if (trimmed.size() > 15) {
    trimmed.resize(15);
  }
#if defined(__linux__) || defined(__APPLE__)
  pthread_setname_np(pthread_self(), trimmed.c_str());
#else
  (void)trimmed;
#endif
}

}  // namespace

uint32_t GetThreadId() {
#if defined(__linux__)
  return static_cast<uint32_t>(syscall(SYS_gettid));
#else
  return static_cast<uint32_t>(pthread_self());
#endif
}

const std::string& GetThreadName() {
  if (g_thread_name == "unknown") {
#if defined(__linux__) || defined(__APPLE__)
    char name[16] = {0};
    if (pthread_getname_np(pthread_self(), name, sizeof(name)) == 0 &&
        name[0] != '\0') {
      g_thread_name = name;
    }
#endif
  }
  return g_thread_name;
}

void SetThreadName(const std::string& name) {
  g_thread_name = name.empty() ? "unknown" : name;
  apply_pthread_name(g_thread_name);
}

}  // namespace utils
}  // namespace lemo
