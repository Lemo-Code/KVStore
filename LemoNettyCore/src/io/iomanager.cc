#include "lemo/io/iomanager.h"

#include "lemo/io/hook.h"

#include <csignal>
#include <vector>

namespace lemo {
namespace io {

IOManager::IOManager(size_t threads, bool use_caller, const std::string& name)
    : fiber::Scheduler(threads, use_caller, name), reactor_(this) {
  start();
}

IOManager::~IOManager() { stop(); }

void IOManager::stop() {
  reactor_.cancelAllEvents();
  tickle();
  fiber::Scheduler::stop();
}

bool IOManager::cancelAll(int fd) {
  const bool cancelled = reactor_.cancelAll(fd);
  tickle();
  return cancelled;
}

IOManager* IOManager::GetThis() {
  return static_cast<IOManager*>(fiber::Scheduler::GetThis());
}

void IOManager::tickle() { reactor_.tickle(); }

bool IOManager::stopping() {
  uint64_t timeout = 0;
  return stopping(timeout);
}

bool IOManager::stopping(uint64_t& timeout) {
  timeout = getNextTimer();
  if (reactor_.pendingEventCount() != 0) {
    return false;
  }
  return fiber::Scheduler::stopping();
}

void IOManager::onTimerInsertedAtFront() { tickle(); }

void IOManager::idle() {
  uint64_t next_timeout = 0;
  if (stopping(next_timeout)) {
    return;
  }

  std::vector<std::function<void()>> cbs;
  listExpiredCb(cbs);
  if (!cbs.empty()) {
    schedule(cbs.begin(), cbs.end());
    cbs.clear();
  }

  reactor_.poll(next_timeout);
}

void IOManager::run() {
  signal(SIGPIPE, SIG_IGN);
  set_hook_enable(true);
  set_hook_iomanager(this);
  fiber::Scheduler::run();
}

}  // namespace io
}  // namespace lemo
