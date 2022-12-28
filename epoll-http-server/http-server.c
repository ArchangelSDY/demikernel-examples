// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// This should come first.
// Glibc macro to expose definitions corresponding to the POSIX.1-2008 base specification.
// See https://man7.org/linux/man-pages/man7/feature_test_macros.7.html.
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>

void build_sockaddr(const char *const ip_str, const char *const port_str, struct sockaddr_in *const addr)
{
    int port = -1;

    sscanf(port_str, "%d", &port);
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    assert(inet_pton(AF_INET, ip_str, &addr->sin_addr) == 1);
}

void set_nonblocking(int fd)
{
    assert(fcntl(fd, F_SETFD, fcntl(fd, F_GETFD, 0) | O_NONBLOCK) == 0);
}

void epoll_ctl_add(int ep, int fd, uint32_t events)
{
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    assert(epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev) == 0);
}

#define MAX_EVENTS 10240
static const char *resp = "HTTP/1.1 204 No Content\r\nServer: demikernel\r\nContent-Length: 0\r\n\r\n";

int main(int argc, char *const argv[])
{
    struct sockaddr_in saddr = {0};
    int sockfd = -1;
    int connfd;
    char buf[8192];
    int n;
    int ep;
    int nevs;
    struct epoll_event events[MAX_EVENTS];

    (void)argc;

    int resp_len = strlen(resp);
    printf("resp len: %d\n", resp_len);

    build_sockaddr(argv[1], argv[2], &saddr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(bind(sockfd, (const struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) == 0);
    set_nonblocking(sockfd);
    assert(listen(sockfd, 10240) == 0);

    ep = epoll_create(1);
    epoll_ctl_add(ep, sockfd, EPOLLIN | EPOLLOUT | EPOLLET);

    while (1) {
        nevs = epoll_wait(ep, events, MAX_EVENTS, 1000);

        for (int i = 0; i < nevs; i++) {
            if (events[i].data.fd == sockfd) {
                connfd = accept(sockfd, NULL, NULL);
                set_nonblocking(connfd);

                epoll_ctl_add(ep, connfd, EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP);
            } else if (events[i].events & EPOLLIN) {
                for (;;) {
                    // We don't really care about content
                    // bzero(buf, sizeof(buf));
                    n = read(events[i].data.fd, buf, sizeof(buf));
                    if (n <= 0) {
                        break;
                    } else {
                        write(events[i].data.fd, resp, resp_len);
                    }
                }
            } else {
                // unexpected
                printf("unexpected\n");
            }

            if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
                epoll_ctl(ep, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                close(events[i].data.fd);
            }
        }
    }

    return 0;
}
