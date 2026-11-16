#pragma once
#ifndef _XBOX_WINSOCK2_H
#define _XBOX_WINSOCK2_H

#ifndef LWIP_POSIX_SOCKETS_IO_NAMES
#define LWIP_POSIX_SOCKETS_IO_NAMES 0
#endif

#ifndef LWIP_TIMEVAL_PRIVATE
#define LWIP_TIMEVAL_PRIVATE 0
#endif

#include <sys/time.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <errno.h>
#include <string.h>

#ifndef SOCKET
#define SOCKET int
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET ((SOCKET)(~0))
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

#ifndef SD_RECEIVE
#define SD_RECEIVE 0
#define SD_SEND    1
#define SD_BOTH    2
#endif

/* Undefine lwIP macros that collide with C++ stream methods (e.g. nlohmann::json, std::ofstream) */
#ifdef accept
#undef accept
static inline SOCKET accept(SOCKET s, struct sockaddr *addr, socklen_t *addrlen) {
    return lwip_accept(s, addr, addrlen);
}
#endif

#ifdef bind
#undef bind
static inline int bind(SOCKET s, const struct sockaddr *name, socklen_t namelen) {
    return lwip_bind(s, name, namelen);
}
#endif

#ifdef shutdown
#undef shutdown
static inline int shutdown(SOCKET s, int how) {
    return lwip_shutdown(s, how);
}
#endif

#ifdef connect
#undef connect
static inline int connect(SOCKET s, const struct sockaddr *name, socklen_t namelen) {
    return lwip_connect(s, name, namelen);
}
#endif

#ifdef listen
#undef listen
static inline int listen(SOCKET s, int backlog) {
    return lwip_listen(s, backlog);
}
#endif

#ifdef socket
#undef socket
static inline SOCKET socket(int domain, int type, int protocol) {
    return lwip_socket(domain, type, protocol);
}
#endif

#ifdef recv
#undef recv
static inline ssize_t recv(SOCKET s, void *mem, size_t len, int flags) {
    return lwip_recv(s, mem, len, flags);
}
#endif

#ifdef send
#undef send
static inline ssize_t send(SOCKET s, const void *dataptr, size_t size, int flags) {
    return lwip_send(s, dataptr, size, flags);
}
#endif

#ifdef read
#undef read
#endif
#ifdef write
#undef write
#endif
#ifdef close
#undef close
#endif
#ifdef ioctl
#undef ioctl
#endif
#ifdef fcntl
#undef fcntl
#endif

#ifndef WSAEWOULDBLOCK
#define WSAEWOULDBLOCK     EWOULDBLOCK
#define WSAEINPROGRESS     EINPROGRESS
#define WSAEALREADY        EALREADY
#define WSAENOTSOCK        ENOTSOCK
#define WSAEDESTADDRREQ    EDESTADDRREQ
#define WSAEMSGSIZE        EMSGSIZE
#define WSAEPROTOTYPE      EPROTOTYPE
#define WSAENOPROTOOPT     ENOPROTOOPT
#define WSAEPROTONOSUPPORT EPROTONOSUPPORT
#define WSAESOCKTNOSUPPORT ESOCKTNOSUPPORT
#define WSAEOPNOTSUPP      EOPNOTSUPP
#define WSAEPFNOSUPPORT    EPFNOSUPPORT
#define WSAEAFNOSUPPORT    EAFNOSUPPORT
#define WSAEADDRINUSE      EADDRINUSE
#define WSAEADDRNOTAVAIL   EADDRNOTAVAIL
#define WSAENETDOWN        ENETDOWN
#define WSAENETUNREACH     ENETUNREACH
#define WSAENETRESET       ENETRESET
#define WSAECONNABORTED    ECONNABORTED
#define WSAECONNRESET      ECONNRESET
#define WSAENOBUFS         ENOBUFS
#define WSAEISCONN         EISCONN
#define WSAENOTCONN        ENOTCONN
#define WSAESHUTDOWN       ESHUTDOWN
#define WSAETOOMANYREFS    ETOOMANYREFS
#define WSAETIMEDOUT       ETIMEDOUT
#define WSAECONNREFUSED    ECONNREFUSED
#define WSAELOOP           ELOOP
#define WSAENAMETOOLONG    ENAMETOOLONG
#define WSAEHOSTDOWN       EHOSTDOWN
#define WSAEHOSTUNREACH    EHOSTUNREACH
#define WSAENOTEMPTY       ENOTEMPTY
#define WSAEINTR           EINTR
#define WSAEFAULT          EFAULT
#define WSAEINVAL          EINVAL
#endif

typedef struct WSAData {
    unsigned short wVersion;
    unsigned short wHighVersion;
    char szDescription[257];
    char szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char *lpVendorInfo;
} WSADATA, *LPWSADATA;

static inline int WSAStartup(unsigned short wVersionRequested, LPWSADATA lpWSAData) {
    (void)wVersionRequested;
    if (lpWSAData) {
        memset(lpWSAData, 0, sizeof(WSADATA));
    }
    return 0;
}

static inline int WSACleanup(void) {
    return 0;
}

static inline int WSAGetLastError(void) {
    return errno;
}

static inline void WSASetLastError(int iError) {
    errno = iError;
}

#endif /* _XBOX_WINSOCK2_H */
