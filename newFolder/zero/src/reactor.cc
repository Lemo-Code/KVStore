/**
 * @file    reactor.cc
 * @brief   Reactor implementation (epoll + eventfd).
 */

#include "zero/reactor/reactor.h"
#include "zero/fiber/fiber.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

namespace zero {

static thread_local Reactor* t_reactor = nullptr;

Reactor::Reactor() {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        throw std::runtime_error("epoll_create1 failed");
    }

    event_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (event_fd_ < 0) {
        ::close(epoll_fd_);
        throw std::runtime_error("eventfd failed");
    }

    // Register eventfd for wakeup
    addEvent(event_fd_, EPOLLIN, nullptr);

    t_reactor = this;
}

Reactor::~Reactor() {
    ::close(event_fd_);
    ::close(epoll_fd_);
    if (t_reactor == this) t_reactor = nullptr;
}

void Reactor::ensureFdCapacity(int fd) {
    if (static_cast<size_t>(fd) >= fd_ctxs_.size()) {
        fd_ctxs_.resize(fd + 64);
    }
}

int Reactor::addEvent(int fd, uint32_t events, Fiber* waiter) {
    ensureFdCapacity(fd);
    EventCtx& ctx = fd_ctxs_[fd];

    if (events & EPOLLIN)  ctx.read_waiter = waiter;
    if (events & EPOLLOUT) ctx.write_waiter = waiter;
    ctx.registered_events |= events;

    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.events = ctx.registered_events | EPOLLONESHOT;
    ev.data.fd = fd;

    int op = EPOLL_CTL_ADD;
    // Try ADD first, if it fails with EEXIST, use MOD
    int ret = epoll_ctl(epoll_fd_, op, fd, &ev);
    if (ret < 0 && errno == EEXIST) {
        op = EPOLL_CTL_MOD;
        ret = epoll_ctl(epoll_fd_, op, fd, &ev);
    }
    return ret;
}

int Reactor::delEvent(int fd) {
    return epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
}

bool Reactor::cancelEvent(int fd, uint32_t event) {
    ensureFdCapacity(fd);
    EventCtx& ctx = fd_ctxs_[fd];

    if ((event & EPOLLIN) && ctx.read_waiter) {
        ctx.read_waiter = nullptr;
        ctx.registered_events &= ~EPOLLIN;
    }
    if ((event & EPOLLOUT) && ctx.write_waiter) {
        ctx.write_waiter = nullptr;
        ctx.registered_events &= ~EPOLLOUT;
    }

    if (ctx.registered_events == 0) {
        delEvent(fd);
        return true;
    }

    // Re-arm with remaining events
    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.events = ctx.registered_events | EPOLLONESHOT;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    return true;
}

int Reactor::poll(int timeout_ms) {
    struct epoll_event events[kMaxEvents];
    int n = epoll_wait(epoll_fd_, events, kMaxEvents, timeout_ms);
    if (n <= 0) return 0;

    for (int i = 0; i < n; ++i) {
        int fd = events[i].data.fd;

        // eventfd wakeup — just consume the data
        if (fd == event_fd_) {
            uint64_t val;
            ::read(event_fd_, &val, sizeof(val));
            continue;
        }

        ensureFdCapacity(fd);
        EventCtx& ctx = fd_ctxs_[fd];
        uint32_t revents = events[i].events;

        // Resume waiting fibers
        if ((revents & EPOLLIN) && ctx.read_waiter) {
            Fiber* f = ctx.read_waiter;
            ctx.read_waiter = nullptr;
            ctx.registered_events &= ~EPOLLIN;
            if (f->state() == Fiber::HOLD) {
                f->set_state(Fiber::READY);
                // Scheduler will resume this fiber
            }
        }
        if ((revents & EPOLLOUT) && ctx.write_waiter) {
            Fiber* f = ctx.write_waiter;
            ctx.write_waiter = nullptr;
            ctx.registered_events &= ~EPOLLOUT;
            if (f->state() == Fiber::HOLD) {
                f->set_state(Fiber::READY);
            }
        }

        // Re-arm if there are still registered events
        if (ctx.registered_events) {
            struct epoll_event ev;
            std::memset(&ev, 0, sizeof(ev));
            ev.events = ctx.registered_events | EPOLLONESHOT;
            ev.data.fd = fd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
        }
    }
    return n;
}

void Reactor::notify() {
    uint64_t val = 1;
    ::write(event_fd_, &val, sizeof(val));
}

Reactor* Reactor::GetCurrent() {
    return t_reactor;
}

} // namespace zero
