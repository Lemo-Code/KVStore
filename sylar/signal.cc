#include "sylar/signal.h"
#include "sylar/log.h"
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

SignalManager* g_signal_manager = nullptr;

SignalManager::SignalManager()
    : m_readFd(-1)
    , m_writeFd(-1)
    , m_running(false) {
    int fds[2];
    if (pipe(fds) != 0) {
        SYLAR_LOG_ERROR(g_logger) << "SignalManager pipe() failed";
        return;
    }
    m_readFd = fds[0];
    m_writeFd = fds[1];
    fcntl(m_readFd, F_SETFL, fcntl(m_readFd, F_GETFL) | O_NONBLOCK);
    fcntl(m_writeFd, F_SETFL, fcntl(m_writeFd, F_GETFL) | O_NONBLOCK);
    g_signal_manager = this;
}

SignalManager::~SignalManager() {
    stop();
    g_signal_manager = nullptr;
    if (m_readFd >= 0) { close(m_readFd); m_readFd = -1; }
    if (m_writeFd >= 0) { close(m_writeFd); m_writeFd = -1; }
}

static void defaultHandler(int signum) {
    int save = errno;
    extern SignalManager* g_signal_manager;
    if (g_signal_manager) g_signal_manager->notifySignal(signum);
    errno = save;
}

bool SignalManager::signal(int signum, Handler cb) {
    if (signum <= 0 || signum >= 64 || m_writeFd < 0) return false;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = defaultHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(signum, &sa, nullptr) != 0) {
        SYLAR_LOG_ERROR(g_logger) << "sigaction(" << signum << ") failed";
        return false;
    }
    sylar::Mutex::Lock lock(m_mutex);
    m_handlers[signum] = std::move(cb);
    return true;
}

bool SignalManager::ignore(int signum) {
    if (signum <= 0 || signum >= 64) return false;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    if (sigaction(signum, &sa, nullptr) != 0) return false;
    sylar::Mutex::Lock lock(m_mutex);
    m_handlers.erase(signum);
    return true;
}

bool SignalManager::reset(int signum) {
    if (signum <= 0 || signum >= 64) return false;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    if (sigaction(signum, &sa, nullptr) != 0) return false;
    sylar::Mutex::Lock lock(m_mutex);
    m_handlers.erase(signum);
    return true;
}

void SignalManager::threadLoop() {
    char buf[16];
    struct pollfd pfd;
    pfd.fd = m_readFd;
    pfd.events = POLLIN;
    while (m_running && m_readFd >= 0) {
        int ret = poll(&pfd, 1, 500);
        if (ret <= 0) continue;
        ssize_t n = read(m_readFd, buf, sizeof(buf));
        for (ssize_t i = 0; i < n; ++i) {
            int sig = static_cast<unsigned char>(buf[i]);
            Handler cb;
            {
                sylar::Mutex::Lock lock(m_mutex);
                auto it = m_handlers.find(sig);
                if (it != m_handlers.end()) cb = it->second;
            }
            if (cb) cb(sig);
        }
    }
}

void SignalManager::start() {
    if (m_running || m_readFd < 0) return;
    m_running = true;
    m_thread = std::make_shared<Thread>(std::bind(&SignalManager::threadLoop, this), "signal");
}

void SignalManager::notifySignal(int signum) {
    if (m_writeFd < 0) return;
    char c = static_cast<char>(signum & 0xFF);
    ssize_t n = write(m_writeFd, &c, 1);
    (void)n;
}

void SignalManager::stop() {
    m_running = false;
    if (m_thread) {
        m_thread->join();
        m_thread.reset();
    }
    for (auto& p : m_handlers) {
        reset(p.first);
    }
    m_handlers.clear();
}

} // namespace sylar
