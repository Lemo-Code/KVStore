#include "lemo/io/reactor.h"

#include "lemo/io/fd_context.h"
#include "lemo/fiber/fiber.h"
#include "lemo/fiber/scheduler.h"
#include "lemo/utils/thread_util.h"

#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace lemo {
namespace io {

struct Reactor::FdContext {
  typedef thread::Mutex MutexType;

  struct EventContext {
    fiber::Scheduler* scheduler = nullptr;
    fiber::Fiber::ptr fiber;
    std::function<void()> cb;
  };

  EventContext& getContext(Reactor::Event event) {
    switch (event) {
      case Reactor::READ:
        return read;
      case Reactor::WRITE:
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

  void triggerEvent(Reactor::Event event) {
    assert(events & event);
    events = static_cast<Reactor::Event>(events & ~event);
    EventContext& ctx = getContext(event);
    if (ctx.cb) {
      ctx.scheduler->schedule(&ctx.cb);
      owner->recordScheduleWake();
    } else {
      if (ctx.fiber->getState() == fiber::Fiber::EXEC) {
        ctx.fiber->setState(fiber::Fiber::READY);
      }
      ctx.scheduler->schedule(&ctx.fiber);
      owner->recordScheduleWake();
    }
    resetContext(ctx);
  }

  void triggerEventRunnext(Reactor::Event event) {
    assert(events & event);
    events = static_cast<Reactor::Event>(events & ~event);
    EventContext& ctx = getContext(event);
    if (ctx.cb) {
      ctx.scheduler->scheduleNext(&ctx.cb);
      owner->recordRunnextWake();
    } else {
      ctx.scheduler->scheduleNext(&ctx.fiber);
      owner->recordRunnextWake();
    }
    resetContext(ctx);
  }

  EventContext read;
  EventContext write;
  int fd = 0;
  Reactor::Event events = Reactor::NONE;
  MutexType mutex;
  Reactor* owner = nullptr;
};

Reactor::Stats Reactor::stats() const {
  Stats s;
  s.epoll_ctl_ops = epoll_ctl_ops_.load(std::memory_order_relaxed);
  s.runnext_wakes = runnext_wakes_.load(std::memory_order_relaxed);
  s.schedule_wakes = schedule_wakes_.load(std::memory_order_relaxed);
  return s;
}

void Reactor::recordEpollCtl() {
  epoll_ctl_ops_.fetch_add(1, std::memory_order_relaxed);
}

void Reactor::recordRunnextWake() {
  runnext_wakes_.fetch_add(1, std::memory_order_relaxed);
}

void Reactor::recordScheduleWake() {
  schedule_wakes_.fetch_add(1, std::memory_order_relaxed);
}

Reactor::Reactor(fiber::Scheduler* owner)
    : owner_(owner),
      epfd_(0),
      ticklefds_{-1, -1},
      pending_event_count_(0) {
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
  recordEpollCtl();

  contextResize(32);
}

Reactor::~Reactor() {
  cancelAllEvents();
  ::close(epfd_);
  ::close(ticklefds_[0]);
  ::close(ticklefds_[1]);
  for (size_t i = 0; i < fd_contexts_.size(); ++i) {
    delete fd_contexts_[i];
  }
}

void Reactor::cancelAllEvents() {
  std::vector<FdContext*> contexts;
  {
    thread::RWMutex::ReadLock lock(mutex_);
    contexts = fd_contexts_;
  }
  for (size_t i = 0; i < contexts.size(); ++i) {
    FdContext* fd_ctx = contexts[i];
    if (fd_ctx == nullptr) continue;
    FdContext::MutexType::Lock lock(fd_ctx->mutex);
    if (!fd_ctx->events) continue;

    if (fd_ctx->events & READ) {
      epoll_event epevent;
      epevent.events = 0;
      epevent.data.ptr = fd_ctx;
      ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd_ctx->fd, &epevent);
      recordEpollCtl();
      fd_ctx->triggerEvent(READ);
      --pending_event_count_;
    }
    if (fd_ctx->events & WRITE) {
      epoll_event epevent;
      epevent.events = 0;
      epevent.data.ptr = fd_ctx;
      ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd_ctx->fd, &epevent);
      recordEpollCtl();
      fd_ctx->triggerEvent(WRITE);
      --pending_event_count_;
    }
  }
  pending_event_count_.store(0, std::memory_order_relaxed);
}

void Reactor::contextResize(size_t size) {
  if (size < fd_contexts_.size()) {
    size = fd_contexts_.size();
  }
  fd_contexts_.resize(size);
  for (size_t i = 0; i < fd_contexts_.size(); ++i) {
    if (!fd_contexts_[i]) {
      fd_contexts_[i] = new FdContext;
      fd_contexts_[i]->fd = static_cast<int>(i);
      fd_contexts_[i]->owner = this;
    }
  }
}

int Reactor::addEvent(int fd, Event event, std::function<void()> cb) {
  lemo::io::FdContext::ptr fdm_ctx = FdManager::Instance().get(fd, false);
  if (fdm_ctx && fdm_ctx->isClose()) {
    errno = EBADF;
    return -1;
  }

  FdContext* fd_ctx = nullptr;

  {
    thread::RWMutex::ReadLock lock(mutex_);
    if (static_cast<int>(fd_contexts_.size()) > fd) {
      fd_ctx = fd_contexts_[fd];
    }
  }
  if (!fd_ctx) {
    thread::RWMutex::WriteLock lock(mutex_);
    const size_t need = static_cast<size_t>(fd) + 1;
    const size_t grow = static_cast<size_t>(fd * 1.5) + 1;
    contextResize(need > grow ? need : grow);
    fd_ctx = fd_contexts_[fd];
  }

  FdContext::MutexType::Lock lock(fd_ctx->mutex);
  if (fdm_ctx && fdm_ctx->isClose()) {
    errno = EBADF;
    return -1;
  }
  assert(!(fd_ctx->events & event));

  const int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
  epoll_event epevent;
  epevent.events = EPOLLET | fd_ctx->events | event;
  epevent.data.ptr = fd_ctx;

  if (::epoll_ctl(epfd_, op, fd, &epevent)) {
    return -1;
  }
  recordEpollCtl();

  ++pending_event_count_;
  fd_ctx->events = static_cast<Event>(fd_ctx->events | event);
  FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
  assert(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb);

  event_ctx.scheduler = owner_;
  if (cb) {
    event_ctx.cb = std::move(cb);
  } else {
    event_ctx.fiber = fiber::Fiber::GetThis();
    assert(event_ctx.fiber->getState() == fiber::Fiber::EXEC);
  }
  return 0;
}

bool Reactor::delEvent(int fd, Event event) {
  thread::RWMutex::ReadLock lock(mutex_);
  if (static_cast<int>(fd_contexts_.size()) <= fd) {
    return false;
  }
  FdContext* fd_ctx = fd_contexts_[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock1(fd_ctx->mutex);
  if (!(fd_ctx->events & event)) {
    return false;
  }

  const Event new_event = static_cast<Event>(fd_ctx->events & ~event);
  const int op = new_event ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events = EPOLLET | new_event;
  epevent.data.ptr = fd_ctx;

  if (::epoll_ctl(epfd_, op, fd, &epevent)) {
    return false;
  }
  recordEpollCtl();

  --pending_event_count_;
  fd_ctx->events = new_event;
  fd_ctx->resetContext(fd_ctx->getContext(event));
  return true;
}

bool Reactor::cancelEvent(int fd, Event event) {
  thread::RWMutex::ReadLock lock(mutex_);
  if (static_cast<int>(fd_contexts_.size()) <= fd) {
    return false;
  }
  FdContext* fd_ctx = fd_contexts_[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock1(fd_ctx->mutex);
  if (!(fd_ctx->events & event)) {
    return false;
  }

  const Event new_event = static_cast<Event>(fd_ctx->events & ~event);
  const int op = new_event ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events = EPOLLET | new_event;
  epevent.data.ptr = fd_ctx;

  if (::epoll_ctl(epfd_, op, fd, &epevent) == 0) {
    recordEpollCtl();
  }

  fd_ctx->triggerEvent(event);
  --pending_event_count_;
  return true;
}

bool Reactor::cancelAll(int fd) {
  thread::RWMutex::ReadLock lock(mutex_);
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
  if (::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &epevent) == 0) {
    recordEpollCtl();
  }

  bool woke = false;
  if (fd_ctx->events & READ) {
    fd_ctx->triggerEvent(READ);
    --pending_event_count_;
    woke = true;
  }
  if (fd_ctx->events & WRITE) {
    fd_ctx->triggerEvent(WRITE);
    --pending_event_count_;
    woke = true;
  }

  assert(fd_ctx->events == NONE);
  return woke;
}

void Reactor::tickle() {
  const int rt = static_cast<int>(::write(ticklefds_[1], "T", 1));
  if (rt < 0 && errno != EAGAIN) {
    assert(false);
  }
}

void Reactor::poll(uint64_t timeout_ms) {
  const uint64_t kMaxEvents = 256;
  epoll_event events[kMaxEvents];

  static const int kMaxTimeout = 100;
  if (timeout_ms != UINT64_MAX) {
    timeout_ms = timeout_ms > static_cast<uint64_t>(kMaxTimeout) ? kMaxTimeout
                                                                 : timeout_ms;
  } else {
    timeout_ms = kMaxTimeout;
  }

  int rt = 0;
  do {
    rt = ::epoll_wait(epfd_, events, static_cast<int>(kMaxEvents),
                      static_cast<int>(timeout_ms));
    if (rt < 0 && errno == EINTR) {
      continue;
    }
    break;
  } while (true);

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

    const int left_events = static_cast<int>(fd_ctx->events & ~real_events);
    const int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    event.events = EPOLLET | left_events;

    if (::epoll_ctl(epfd_, op, fd_ctx->fd, &event) == 0) {
      recordEpollCtl();
    }

    if (real_events & READ) {
      fd_ctx->triggerEventRunnext(READ);
      --pending_event_count_;
    }
    if (real_events & WRITE) {
      fd_ctx->triggerEventRunnext(WRITE);
      --pending_event_count_;
    }
  }
}

}  // namespace io
}  // namespace lemo
