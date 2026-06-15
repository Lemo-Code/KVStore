/**
 * @file    echo_server.cpp
 * @brief   Simple TCP echo server using fiber + reactor.
 *
 * Demonstrates:
 *   - Fiber per connection (cooperative multitasking)
 *   - Reactor (epoll) for non-blocking I/O
 *   - Blocking-free accept/read/write via fiber yield
 *
 * Usage: ./echo_server [port]
 */

#include "zero/fiber/fiber.h"
#include "zero/reactor/reactor.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <vector>

using namespace zero;

static int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Handle a single client connection in its own fiber
static void handleClient(int client_fd) {
    char buf[4096];
    Reactor* reactor = Reactor::GetCurrent();

    while (true) {
        // Read from client
        ssize_t n = ::read(client_fd, buf, sizeof(buf));
        if (n > 0) {
            // Echo back
            ::write(client_fd, buf, n);
        } else if (n == 0) {
            // Client closed connection
            break;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available — register with epoll and yield
            reactor->addEvent(client_fd, EPOLLIN, Fiber::GetCurrent());
            Fiber::GetCurrent()->yield();
        } else {
            break; // Error
        }
    }
    reactor->delEvent(client_fd);
    ::close(client_fd);
    printf("  client fd=%d disconnected\n", client_fd);
}

int main(int argc, char** argv) {
    int port = (argc > 1) ? atoi(argv[1]) : 8888;

    // Create listening socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    setNonBlocking(listen_fd);

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 128);

    printf("Echo server listening on port %d\n", port);
    printf("Test: nc localhost %d\n", port);

    // Create main fiber
    Fiber main_fiber;

    // Create reactor
    Reactor reactor;

    // Track client fibers
    std::vector<Fiber*> clients;

    // Accept loop (runs on main fiber)
    while (true) {
        // Poll for events (non-blocking)
        reactor.poll(10); // 10ms timeout

        // Accept new connections
        int client_fd = ::accept(listen_fd, nullptr, nullptr);
        if (client_fd >= 0) {
            setNonBlocking(client_fd);
            printf("New client fd=%d\n", client_fd);

            // Create a fiber for this client
            auto* client = new Fiber([client_fd]() {
                handleClient(client_fd);
            });
            clients.push_back(client);
            client->resume(); // Start handling the client
        }

        // Cleanup terminated fibers
        for (auto it = clients.begin(); it != clients.end(); ) {
            if ((*it)->state() == Fiber::TERM) {
                delete *it;
                it = clients.erase(it);
            } else {
                ++it;
            }
        }
    }

    return 0;
}
