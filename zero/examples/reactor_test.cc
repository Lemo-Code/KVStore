// Reactor 测试: pipe + epoll 唤醒
#include "zero/scheduler/reactor.h"
#include "zero/base/macro.h"

#include <cstdio>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>

int main() {
    setbuf(stdout, NULL);
    printf("=== Reactor Test ===\n");

    // 创建 pipe
    int pipefd[2];
    assert(pipe(pipefd) == 0);

    // 设置读端为非阻塞
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);

    zero::Reactor reactor;

    // 测试事件注册
    int rt = reactor.addEvent(pipefd[0], zero::Reactor::READ, nullptr, 5000);
    printf("addEvent(READ): %d (0=success)\n", rt);
    assert(rt == 0);
    printf("hasPending=%d\n", reactor.hasPendingEvents());
    assert(reactor.hasPendingEvents());

    // 写数据到 pipe
    char msg[] = "hello";
    printf("writing to pipe...\n");
    write(pipefd[1], msg, sizeof(msg));
    printf("write done\n");

    // Poll: 应该检测到读就绪
    printf("calling poll...\n");
    std::vector<zero::Fiber::ptr> ready;
    int n = reactor.poll(100, ready);
    printf("poll(): %d ready fibers\n", n);

    // 验证数据可读
    char buf[16];
    ssize_t nr = read(pipefd[0], buf, sizeof(buf));
    printf("read: %zd bytes\n", nr);
    assert(nr == sizeof(msg));

    // 不再有待处理事件 (EPOLLET, 事件已被消费)
    assert(!reactor.hasPendingEvents());

    // 测试删除事件
    rt = reactor.addEvent(pipefd[0], zero::Reactor::READ, nullptr, 0);
    assert(rt == 0);
    assert(reactor.hasPendingEvents());

    bool ok = reactor.delEvent(pipefd[0], zero::Reactor::READ);
    assert(ok);
    assert(!reactor.hasPendingEvents());

    // 测试定时器
    int timer_fired = 0;
    reactor.addTimer(50, [&timer_fired]() { timer_fired++; });

    // 多 poll 几次让定时器到期
    for (int i = 0; i < 5; ++i) {
        reactor.poll(20, ready);
        if (timer_fired > 0) break;
    }
    printf("Timer fired: %d\n", timer_fired);
    assert(timer_fired > 0);

    // 测试 wakeup
    int test_val = 0;
    reactor.addTimer(100, [&test_val]() { test_val = 1; });
    reactor.wakeup();  // 提前唤醒 epoll_wait

    reactor.poll(10, ready);
    // timer 可能还不到, 但 wakeup 让 poll 提前返回了

    close(pipefd[0]);
    close(pipefd[1]);

    printf("=== Reactor Test PASSED ===\n");
    return 0;
}
