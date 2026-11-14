#define LWIP_COMPAT_SOCKETS 0
#define LWIP_POSIX_SOCKETS_IO_NAMES 0

#include <errno.h>
#include <fcntl.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

#define SE_NET_SOCKET(domain, type, protocol) lwip_socket(domain, type, protocol)
#define SE_NET_CONNECT(fd, addr, len) lwip_connect(fd, addr, len)
#define SE_NET_SEND(fd, buf, len, flags) lwip_send(fd, buf, len, flags)
#define SE_NET_RECV(fd, buf, len, flags) lwip_recv(fd, buf, len, flags)
#define SE_NET_GETSOCKOPT(fd, level, name, value, len) lwip_getsockopt(fd, level, name, value, len)
#define SE_NET_SETSOCKOPT(fd, level, name, value, len) lwip_setsockopt(fd, level, name, value, len)
#define SE_NET_GETADDRINFO(host, service, hints, res) lwip_getaddrinfo(host, service, hints, res)
#define SE_NET_FREEADDRINFO(ai) lwip_freeaddrinfo(ai)
#define SE_NET_GETHOSTBYNAME(host) lwip_gethostbyname(host)

#define SE_NET_CLOSE(fd) lwip_close(fd)
#define SE_NET_POLL(fds, n, ms) lwip_poll(fds, n, ms)
#define SE_NET_SET_NONBLOCK(fd) lwip_fcntl(fd, F_SETFL, lwip_fcntl(fd, F_GETFL, 0) | O_NONBLOCK)
#define SE_NET_RANDOM(buf, len) false

#include <socket_bsd.hpp>
