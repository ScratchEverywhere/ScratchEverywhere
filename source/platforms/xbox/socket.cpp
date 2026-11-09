#include <errno.h>
#include <fcntl.h>
#include <lwip/sockets.h>

#define SE_NET_CLOSE(fd) lwip_close(fd)
#define SE_NET_POLL(fds, n, ms) lwip_poll(fds, n, ms)
#define SE_NET_SET_NONBLOCK(fd) lwip_fcntl(fd, F_SETFL, lwip_fcntl(fd, F_GETFL, 0) | O_NONBLOCK)
#define SE_NET_RANDOM(buf, len) false

#include <socket_bsd.hpp>
