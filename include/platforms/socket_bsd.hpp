#pragma once
#include <chrono>
#include <cstring>
#include <os.hpp>
#include <socket.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET se_sock_t;
#define SE_SOCK_INVALID INVALID_SOCKET
#ifndef SE_NET_PLATFORM_INIT
#define SE_NET_PLATFORM_INIT() (WSAStartup(MAKEWORD(2, 2), &se_wsaData) == 0)
#define SE_NET_PLATFORM_DEINIT() WSACleanup()
static WSADATA se_wsaData;
#endif
#ifndef SE_NET_CLOSE
#define SE_NET_CLOSE(fd) closesocket(fd)
#endif
#ifndef SE_NET_POLL
#define SE_NET_POLL(fds, n, ms) WSAPoll(fds, n, ms)
#endif
#ifndef SE_NET_LAST_ERROR
#define SE_NET_LAST_ERROR() WSAGetLastError()
#endif
#ifndef SE_NET_IS_BLOCKED
#define SE_NET_IS_BLOCKED(e) ((e) == WSAEWOULDBLOCK || (e) == WSAEINPROGRESS || (e) == WSAEINTR)
#endif
#ifndef SE_NET_SET_NONBLOCK
#define SE_NET_SET_NONBLOCK(fd)           \
    do {                                  \
        u_long se_nb = 1;                 \
        ioctlsocket(fd, FIONBIO, &se_nb); \
    } while (0)
#endif
#ifndef SE_NET_RANDOM
#include <bcrypt.h>
#define SE_NET_RANDOM(buf, len) (BCryptGenRandom(nullptr, (PUCHAR)(buf), (ULONG)(len), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0)
#endif
#define SE_NET_SEND_FLAGS 0
#else
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int se_sock_t;
#define SE_SOCK_INVALID (-1)
#ifndef SE_NET_PLATFORM_INIT
#define SE_NET_PLATFORM_INIT() true
#define SE_NET_PLATFORM_DEINIT() ((void)0)
#endif
#ifndef SE_NET_CLOSE
#define SE_NET_CLOSE(fd) ::close(fd)
#endif
#ifndef SE_NET_POLL
#define SE_NET_POLL(fds, n, ms) ::poll(fds, n, ms)
#endif
#ifndef SE_NET_LAST_ERROR
#define SE_NET_LAST_ERROR() errno
#endif
#ifndef SE_NET_IS_BLOCKED
#define SE_NET_IS_BLOCKED(e) ((e) == EWOULDBLOCK || (e) == EAGAIN || (e) == EINPROGRESS || (e) == EINTR)
#endif
#ifndef SE_NET_SET_NONBLOCK
#define SE_NET_SET_NONBLOCK(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK)
#endif
#ifndef SE_NET_RANDOM
#include <cstdio>
static bool se_readUrandom(void *buf, size_t len) {
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return false;
    bool ok = fread(buf, 1, len, f) == len;
    fclose(f);
    return ok;
}
#define SE_NET_RANDOM(buf, len) se_readUrandom(buf, len)
#endif
#ifdef MSG_NOSIGNAL
#define SE_NET_SEND_FLAGS MSG_NOSIGNAL
#else
#define SE_NET_SEND_FLAGS 0
#endif
#endif

struct SE_Socket::Impl {
    se_sock_t fd = SE_SOCK_INVALID;
    bool timedOut = false;
};

static int se_refCount = 0;

static int se_msLeft(std::chrono::steady_clock::time_point deadline) {
    auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
    return left < 0 ? 0 : (int)left;
}

static int se_waitFor(se_sock_t fd, short events, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        struct pollfd p;
        p.fd = fd;
        p.events = events;
        p.revents = 0;
        int r = SE_NET_POLL(&p, 1, se_msLeft(deadline));
        if (r < 0 && SE_NET_IS_BLOCKED(SE_NET_LAST_ERROR()) && se_msLeft(deadline) > 0) continue;
        return r;
    }
}

SE_Socket::SE_Socket() : impl(new Impl) {}

SE_Socket::~SE_Socket() {
    close();
    delete impl;
}

bool SE_Socket::init() {
    if (se_refCount == 0) {
        if (!OS::initWifi()) return false;
        if (!SE_NET_PLATFORM_INIT()) {
            OS::deInitWifi();
            return false;
        }
    }
    se_refCount++;
    return true;
}

void SE_Socket::deinit() {
    if (se_refCount == 0) return;
    if (--se_refCount == 0) {
        SE_NET_PLATFORM_DEINIT();
        OS::deInitWifi();
    }
}

void SE_Socket::randomBytes(void *buf, size_t len) {
    if (SE_NET_RANDOM(buf, len)) return;
    static uint64_t state = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count() | 1;
    uint8_t *out = static_cast<uint8_t *>(buf);
    for (size_t i = 0; i < len; i++) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        out[i] = (uint8_t)(state >> 24);
    }
}

bool SE_Socket::connect(const std::string &host, uint16_t port, int timeoutMs) {
    close();
    impl->timedOut = false;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = nullptr;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || res == nullptr) return false;

    for (struct addrinfo *ai = res; ai != nullptr; ai = ai->ai_next) {
        se_sock_t fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd == SE_SOCK_INVALID) continue;

        SE_NET_SET_NONBLOCK(fd);

        bool ok = false;
        int r = ::connect(fd, ai->ai_addr, (int)ai->ai_addrlen);
        if (r == 0) {
            ok = true;
        } else if (SE_NET_IS_BLOCKED(SE_NET_LAST_ERROR())) {
            if (se_waitFor(fd, POLLOUT, timeoutMs) > 0) {
                int err = 0;
                socklen_t errLen = sizeof(err);
                ok = getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &errLen) == 0 && err == 0;
            }
        }

        if (!ok) {
            SE_NET_CLOSE(fd);
            continue;
        }

        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
#ifdef SO_NOSIGPIPE
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&one, sizeof(one));
#endif
        impl->fd = fd;
        break;
    }

    freeaddrinfo(res);
    return impl->fd != SE_SOCK_INVALID;
}

bool SE_Socket::isConnected() const {
    return impl->fd != SE_SOCK_INVALID;
}

int SE_Socket::send(const void *buf, size_t len, int timeoutMs) {
    if (impl->fd == SE_SOCK_INVALID) return -1;
    impl->timedOut = false;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    const char *p = static_cast<const char *>(buf);
    size_t sent = 0;
    while (sent < len) {
        int r = (int)::send(impl->fd, p + sent, (int)(len - sent), SE_NET_SEND_FLAGS);
        if (r > 0) {
            sent += (size_t)r;
            continue;
        }
        if (r < 0 && SE_NET_IS_BLOCKED(SE_NET_LAST_ERROR())) {
            int w = se_waitFor(impl->fd, POLLOUT, se_msLeft(deadline));
            if (w > 0) continue;
            impl->timedOut = w == 0;
        }
        return -1;
    }
    return (int)sent;
}

int SE_Socket::recv(void *buf, size_t len, int timeoutMs) {
    if (impl->fd == SE_SOCK_INVALID) return -1;
    impl->timedOut = false;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        int r = (int)::recv(impl->fd, static_cast<char *>(buf), (int)len, 0);
        if (r >= 0) return r;
        if (!SE_NET_IS_BLOCKED(SE_NET_LAST_ERROR())) return -1;

        int w = se_waitFor(impl->fd, POLLIN, se_msLeft(deadline));
        if (w > 0) continue;
        impl->timedOut = w == 0;
        return -1;
    }
}

bool SE_Socket::timedOut() const {
    return impl->timedOut;
}

bool SE_Socket::waitReadable(int timeoutMs) {
    if (impl->fd == SE_SOCK_INVALID) return false;
    return se_waitFor(impl->fd, POLLIN, timeoutMs) > 0;
}

void SE_Socket::close() {
    if (impl->fd == SE_SOCK_INVALID) return;
    SE_NET_CLOSE(impl->fd);
    impl->fd = SE_SOCK_INVALID;
}
