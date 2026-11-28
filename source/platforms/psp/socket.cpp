#include <sys/select.h>
#include <sys/socket.h>

#define SE_NET_NO_POLL_H
struct pollfd {
    int fd;
    short events;
    short revents;
};
#define POLLIN 0x0001
#define POLLOUT 0x0004

static int se_pspPoll(struct pollfd *fds, int, int timeoutMs) {
    fd_set readSet, writeSet;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    if (fds->events & POLLIN) FD_SET(fds->fd, &readSet);
    if (fds->events & POLLOUT) FD_SET(fds->fd, &writeSet);

    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int r = select(fds->fd + 1, &readSet, &writeSet, nullptr, &tv);
    fds->revents = 0;
    if (r > 0) {
        if (FD_ISSET(fds->fd, &readSet)) fds->revents |= POLLIN;
        if (FD_ISSET(fds->fd, &writeSet)) fds->revents |= POLLOUT;
    }
    return r;
}

#define SE_NET_POLL(fds, n, ms) se_pspPoll(fds, n, ms)
#define SE_NET_SET_NONBLOCK(fd)                                           \
    do {                                                                  \
        int se_one = 1;                                                   \
        setsockopt(fd, SOL_SOCKET, SO_NONBLOCK, &se_one, sizeof(se_one)); \
    } while (0)
#define SE_NET_IS_BLOCKED(e) ((e) == EWOULDBLOCK || (e) == EAGAIN || (e) == EINPROGRESS || (e) == EINTR || (e) == 35 || (e) == 36)
#define SE_NET_USE_GETHOSTBYNAME
#define SE_NET_RANDOM(buf, len) false

#include <socket_bsd.hpp>
