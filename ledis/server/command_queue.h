#pragma once
#include <string>
#include <string_view>
#include <lstl/container/vector.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include "zero/thread/mutex.h"

namespace ledis {
struct ClientContext;
struct PendingCommand {
    ClientContext* client = nullptr;
    lstl::vector<std::string_view> args;
    int db_index = 0;
};

class CommandQueue {
public:
    CommandQueue() { event_fd_ = eventfd(0, EFD_NONBLOCK|EFD_SEMAPHORE); }
    ~CommandQueue() { if(event_fd_>=0) ::close(event_fd_); }
    void push(PendingCommand cmd) {
        bool was_empty;
        { zero::Mutex::Lock lk(mutex_); was_empty = queue_.empty(); queue_.push_back(std::move(cmd)); }
        if (was_empty) { uint64_t v=1; ::write(event_fd_,&v,sizeof(v)); }
    }
    lstl::vector<PendingCommand> drain() {
        lstl::vector<PendingCommand> out;
        { zero::Mutex::Lock lk(mutex_); out.swap(queue_); }
        return out;
    }
    int eventFd() const { return event_fd_; }
private:
    zero::Mutex mutex_;
    lstl::vector<PendingCommand> queue_;
    int event_fd_ = -1;
};
} // namespace ledis
