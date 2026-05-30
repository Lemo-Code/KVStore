#include "io/iomanager.h"

#include "fiber/fiber.h"
#include "io/hook.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace net {

struct IOManager::FdContext {
  typedef Mutex MutexType;

  struct EventContext {
    Scheduler* scheduler = nullptr;
    Fiber::ptr fiber;
    std::function<void()> cb;
  };

  EventContext& getContext(IOManager::Event event) {
    switch (event) {
      case IOManager::READ:
        return read;
      case IOManager::WRITE:
        return write;
      default:
        assert(false);
    }
    throw std::invalid_argument("getContext invalid event");
  }

  void resetContext(EventContext& ctx) {
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;
  }

  void triggerEvent(IOManager::Event event) {
    assert(events & event);
    events = static_cast<IOManager::Event>(events & ~event);
    EventContext& ctx = getContext(event);
    if (ctx.cb) {
      ctx.scheduler->schedule(&ctx.cb);
    } else {
      ctx.scheduler->schedule(&ctx.fiber);
    }
    resetContext(ctx);
  }

  EventContext read;
  EventContext write;
  int fd = 0;
  IOManager::Event events = IOManager::NONE;
  MutexType mutex;
};

IOManager::IOManager(size_t threads, bool use_caller, const std::string& name)
    : Scheduler(threads, use_caller, name) {
  epfd_ = ::epoll_create(5000);
  assert(epfd_ > 0);

  int rt = ::pipe(ticklefds_);
  assert(rt == 0);

  epoll_event event;
  std::memset(&event, 0, sizeof(event));
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = ticklefds_[0];

  rt = ::fcntl(ticklefds_[0], F_SETFL, O_NONBLOCK);
  assert(rt == 0);

  rt = ::epoll_ctl(epfd_, EPOLL_CTL_ADD, ticklefds_[0], &event);
  assert(rt == 0);

  contextResize(32);
  start();
}

IOManager::~IOManager() {
  stop();
  ::close(epfd_);
  ::close(ticklefds_[0]);
  ::close(ticklefds_[1]);
  for (size_t i = 0; i < fd_contexts_.size(); ++i) {
    delete fd_contexts_[i];
  }
}

void IOManager::stop() {
  cancelAllEvents();
  Scheduler::stop();
}

void IOManager::cancelAllEvents() {
  std::vector<FdContext*> contexts;
  {
    RWMutexType::ReadLock lock(mutex_);
    contexts = fd_contexts_;
  }
  for (FdContext* fd_ctx : contexts) {
    if (fd_ctx == nullptr) {
      continue;
    }
    FdContext::MutexType::Lock lock(fd_ctx->mutex);
    if (!fd_ctx->events) {
      continue;
    }
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;
    if (::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd_ctx->fd, &epevent)) {
      continue;
    }

    if (fd_ctx->events & READ) {
      fd_ctx->triggerEvent(READ);
      --pending_event_count_;
    }
    if (fd_ctx->events & WRITE) {
      fd_ctx->triggerEvent(WRITE);
      --pending_event_count_;
    }
  }
  pending_event_count_.store(0, std::memory_order_relaxed);
}

void IOManager::contextResize(size_t size) {
  if (size < fd_contexts_.size()) {
    size = fd_contexts_.size();
  }
  fd_contexts_.resize(size);
  for (size_t i = 0; i < fd_contexts_.size(); ++i) {
    if (!fd_contexts_[i]) {
      fd_contexts_[i] = new FdContext;
      fd_contexts_[i]->fd = static_cast<int>(i);
    }
  }
}

int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
  FdContext* fd_ctx = nullptr;

  RWMutexType::ReadLock lock(mutex_);
  if (static_cast<int>(fd_contexts_.size()) > fd) {
    fd_ctx = fd_contexts_[fd];
    lock.unlock();
  } else {
    lock.unlock();
    RWMutexType::WriteLock lock2(mutex_);
    const size_t need = static_cast<size_t>(fd) + 1;
    const size_t grow = static_cast<size_t>(fd * 1.5) + 1;
    contextResize(need > grow ? need : grow);
    fd_ctx = fd_contexts_[fd];
  }

  FdContext::MutexType::Lock lock2(fd_ctx->mutex);
  assert(!(fd_ctx->events & event));

  int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
  epoll_event epevent;
  epevent.events = EPOLLET | fd_ctx->events | event;
  epevent.data.ptr = fd_ctx;

  int rt = ::epoll_ctl(epfd_, op, fd, &epevent);
  if (rt) {
    return -1;
  }

  ++pending_event_count_;
  fd_ctx->events = static_cast<Event>(fd_ctx->events | event);
  FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
  assert(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb);

  event_ctx.scheduler = Scheduler::GetThis();
  if (cb) {
    event_ctx.cb.swap(cb);
  } else {
    event_ctx.fiber = Fiber::GetThis();
    assert(event_ctx.fiber->getState() == Fiber::EXEC);
  }
  return 0;
}

bool IOManager::delEvent(int fd, Event event) {
  RWMutexType::ReadLock lock(mutex_);
  if (static_cast<int>(fd_contexts_.size()) <= fd) {
    return false;
  }
  FdContext* fd_ctx = fd_contexts_[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock1(fd_ctx->mutex);
  if (!(fd_ctx->events & event)) {
    return false;
  }

  Event new_event = static_cast<Event>(fd_ctx->events & ~event);
  int op = new_event ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events = EPOLLET | new_event;
  epevent.data.ptr = fd_ctx;

  if (::epoll_ctl(epfd_, op, fd, &epevent)) {
    return false;
  }

  --pending_event_count_;
  fd_ctx->events = new_event;
  fd_ctx->resetContext(fd_ctx->getContext(event));
  return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
  RWMutexType::ReadLock lock(mutex_);
  if (static_cast<int>(fd_contexts_.size()) <= fd) {
    return false;
  }
  FdContext* fd_ctx = fd_contexts_[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock1(fd_ctx->mutex);
  if (!(fd_ctx->events & event)) {
    return false;
  }

  Event new_event = static_cast<Event>(fd_ctx->events & ~event);
  int op = new_event ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events = EPOLLET | new_event;
  epevent.data.ptr = fd_ctx;

  if (::epoll_ctl(epfd_, op, fd, &epevent)) {
    return false;
  }

  fd_ctx->triggerEvent(event);
  --pending_event_count_;
  return true;
}

bool IOManager::cancelAll(int fd) {
  RWMutexType::ReadLock lock(mutex_);
  if (static_cast<int>(fd_contexts_.size()) <= fd) {
    return false;
  }
  FdContext* fd_ctx = fd_contexts_[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock1(fd_ctx->mutex);
  if (!fd_ctx->events) {
    return false;
  }

  epoll_event epevent;
  epevent.events = 0;
  epevent.data.ptr = fd_ctx;

  if (::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &epevent)) {
    return false;
  }

  if (fd_ctx->events & READ) {
    fd_ctx->triggerEvent(READ);
    --pending_event_count_;
  }
  if (fd_ctx->events & WRITE) {
    fd_ctx->triggerEvent(WRITE);
    --pending_event_count_;
  }

  assert(fd_ctx->events == NONE);
  return true;
}

IOManager* IOManager::GetThis() {
  return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle() {
  int rt = static_cast<int>(::write(ticklefds_[1], "T", 1));
  if (rt < 0 && errno != EAGAIN) {
    assert(false);
  }
}

bool IOManager::stopping() {
  uint64_t timeout = 0;
  return stopping(timeout);
}

bool IOManager::stopping(uint64_t& timeout) {
  timeout = getNextTimer();
  if (pending_event_count_ != 0) {
    return false;
  }
  return Scheduler::stopping();
}

IOManager::FdContext* IOManager::getFdContext(int fd) {
  RWMutexType::ReadLock lock(mutex_);
  if (fd < 0 || static_cast<int>(fd_contexts_.size()) <= fd) {
    return nullptr;
  }
  FdContext* ctx = fd_contexts_[fd];
  if (ctx == nullptr || ctx->events == NONE) {
    return nullptr;
  }
  return ctx;
}

void IOManager::idle() {
  const uint64_t kMaxEvents = 256;
  epoll_event* events = new epoll_event[kMaxEvents];
  std::shared_ptr<epoll_event> shared_events(
      events, [](epoll_event* ptr) { delete[] ptr; });

  uint64_t next_timeout = 0;
  if (stopping(next_timeout)) {
    return;
  }

  int rt = 0;
  do {
    static const int kMaxTimeout = 5000;
    if (next_timeout != UINT64_MAX) {
      next_timeout = next_timeout > static_cast<uint64_t>(kMaxTimeout)
                         ? kMaxTimeout
                         : next_timeout;
    } else {
      next_timeout = kMaxTimeout;
    }
    rt = ::epoll_wait(epfd_, events, static_cast<int>(kMaxEvents),
                      static_cast<int>(next_timeout));
    if (rt < 0 && errno == EINTR) {
      continue;
    }
    break;
  } while (true);

  std::vector<std::function<void()>> cbs;
  listExpiredCb(cbs);
  if (!cbs.empty()) {
    schedule(cbs.begin(), cbs.end());
    cbs.clear();
  }

  for (int i = 0; i < rt; ++i) {
    epoll_event& event = events[i];
    if (event.data.fd == ticklefds_[0]) {
      uint8_t dummy[256];
      while (::read(ticklefds_[0], dummy, sizeof(dummy)) > 0) {
      }
      continue;
    }

    FdContext* fd_ctx = static_cast<FdContext*>(event.data.ptr);
    FdContext::MutexType::Lock lock(fd_ctx->mutex);
    if (event.events & (EPOLLERR | EPOLLHUP)) {
      event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
    }

    int real_events = NONE;
    if (event.events & EPOLLIN) {
      real_events |= READ;
    }
    if (event.events & EPOLLOUT) {
      real_events |= WRITE;
    }

    if ((fd_ctx->events & real_events) == NONE) {
      continue;
    }

    int left_events = static_cast<int>(fd_ctx->events & ~real_events);
    int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    event.events = EPOLLET | left_events;

    if (::epoll_ctl(epfd_, op, fd_ctx->fd, &event)) {
      continue;
    }

    if (real_events & READ) {
      fd_ctx->triggerEvent(READ);
      --pending_event_count_;
    }
    if (real_events & WRITE) {
      fd_ctx->triggerEvent(WRITE);
      --pending_event_count_;
    }
  }
}

void IOManager::onTimerInsertedAtFront() { tickle(); }

void IOManager::run() {
  set_hook_enable(true);
  Scheduler::run();
}

}  // namespace net
