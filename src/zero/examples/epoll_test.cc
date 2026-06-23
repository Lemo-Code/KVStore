// 最小 epoll accept 测试
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <cassert>

int main() {
    setbuf(stdout, NULL);
    printf("=== Minimal epoll accept test ===\n");

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listen_fd >= 0);

    int val = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);
    assert(bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) == 0);
    assert(listen(listen_fd, 5) == 0);
    printf("Listening on :9999\n");

    int epfd = epoll_create1(0);
    assert(epfd >= 0);

    epoll_event ev = {};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd;
    assert(epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == 0);
    printf("Registered with epoll\n");

    // poll
    printf("epoll_wait...\n");
    epoll_event events[10];
    int n = epoll_wait(epfd, events, 10, 3000);
    printf("epoll_wait returned: %d\n", n);

    if (n > 0) {
        printf("events[0].events=0x%x data.fd=%d\n",
               events[0].events, events[0].data.fd);
        if (events[0].data.fd == listen_fd && (events[0].events & EPOLLIN)) {
            sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);
            int client = accept(listen_fd, (sockaddr*)&client_addr, &len);
            printf("accept returned: %d\n", client);
        }
    }

    close(epfd);
    close(listen_fd);
    printf("=== Done ===\n");
    return 0;
}
