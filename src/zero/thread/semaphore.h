#pragma once

#include <semaphore.h>
#include "zero/base/noncopyable.h"

namespace zero {

// 信号量 — 线程间同步
class Semaphore : public Noncopyable {
public:
    explicit Semaphore(uint32_t count = 0) {
        sem_init(&sem_, 0, count);
    }
    ~Semaphore() {
        sem_destroy(&sem_);
    }

    // P 操作 (减 1，若无资源则阻塞)
    void wait() {
        sem_wait(&sem_);
    }

    // V 操作 (加 1，唤醒等待者)
    void notify() {
        sem_post(&sem_);
    }

private:
    sem_t sem_;
};

} // namespace zero
