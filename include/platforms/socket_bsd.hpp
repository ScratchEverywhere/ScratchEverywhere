#pragma once
#include <cstring>
#include <ctime>
#include <os.hpp>
#include <socket.hpp>
#include <thread.hpp>
#include <timer.hpp>
#include <vector>

#if defined(_WIN32) && !defined(__XBOX__) && !defined(NXDK)
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
#ifndef SE_NET_NO_POLL_H
#include <poll.h>
#endif
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

#ifndef SE_NET_SOCKET
#define SE_NET_SOCKET(domain, type, protocol) ::socket(domain, type, protocol)
#define SE_NET_CONNECT(fd, addr, len) ::connect(fd, addr, len)
#define SE_NET_SEND(fd, buf, len, flags) ::send(fd, buf, len, flags)
#define SE_NET_RECV(fd, buf, len, flags) ::recv(fd, buf, len, flags)
#define SE_NET_GETSOCKOPT(fd, level, name, value, len) ::getsockopt(fd, level, name, value, len)
#define SE_NET_SETSOCKOPT(fd, level, name, value, len) ::setsockopt(fd, level, name, value, len)
#define SE_NET_GETADDRINFO(host, service, hints, res) ::getaddrinfo(host, service, hints, res)
#define SE_NET_FREEADDRINFO(ai) ::freeaddrinfo(ai)
#define SE_NET_GETHOSTBYNAME(host) ::gethostbyname(host)
#endif

struct SeAddress {
    int family = 0;
    int type = 0;
    int protocol = 0;
    socklen_t length = 0;
    alignas(8) unsigned char storage[128];
};

struct SE_Socket::Impl {
    se_sock_t fd = SE_SOCK_INVALID;
    bool timedOut = false;
};

static int se_refCount = 0;

static SE_Mutex &se_netMutex() {
    static SE_Mutex mutex;
    static const bool initialised = (mutex.init(), true);
    (void)initialised;
    return mutex;
}

struct SeDeadline {
    Timer timer;
    int totalMs;

    explicit SeDeadline(int timeoutMs) : totalMs(timeoutMs) {}

    int msLeft() {
        uint64_t elapsed = timer.getTimeMs();
        return elapsed >= (uint64_t)totalMs ? 0 : totalMs - (int)elapsed;
    }
};

static int se_waitFor(se_sock_t fd, short events, int timeoutMs) {
    SeDeadline deadline(timeoutMs);
    for (;;) {
        struct pollfd p;
        p.fd = fd;
        p.events = events;
        p.revents = 0;
        int r = SE_NET_POLL(&p, 1, deadline.msLeft());
        if (r < 0 && SE_NET_IS_BLOCKED(SE_NET_LAST_ERROR()) && deadline.msLeft() > 0) continue;
        return r;
    }
}

SE_Socket::SE_Socket() : impl(new Impl) {}

SE_Socket::~SE_Socket() {
    close();
    delete impl;
}

bool SE_Socket::init() {
    se_netMutex().lock();
    bool ok = true;
    if (se_refCount == 0) {
        if (!OS::initWifi()) {
            ok = false;
        } else if (!SE_NET_PLATFORM_INIT()) {
            OS::deInitWifi();
            ok = false;
        }
    }
    if (ok) se_refCount++;
    se_netMutex().unlock();
    return ok;
}

void SE_Socket::deinit() {
    se_netMutex().lock();
    if (se_refCount > 0 && --se_refCount == 0) {
        SE_NET_PLATFORM_DEINIT();
        OS::deInitWifi();
    }
    se_netMutex().unlock();
}

void SE_Socket::randomBytes(void *buf, size_t len) {
    if (SE_NET_RANDOM(buf, len)) return;
    int stackMarker;
    static uint64_t state = ((uint64_t)time(nullptr) * 0x9E3779B97F4A7C15ULL ^ (uint64_t)(uintptr_t)&stackMarker ^ (uint64_t)(uintptr_t)&se_refCount) | 1;
    uint8_t *out = static_cast<uint8_t *>(buf);
    for (size_t i = 0; i < len; i++) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        out[i] = (uint8_t)(state >> 24);
    }
}

static bool se_resolve(const std::string &host, uint16_t port, std::vector<SeAddress> &out) {
#if defined(SE_NET_RESOLVE)
    return SE_NET_RESOLVE(host, port, out);
#elif defined(SE_NET_USE_GETHOSTBYNAME)
    se_netMutex().lock();
    struct hostent *he = SE_NET_GETHOSTBYNAME(host.c_str());
    if (he == nullptr || he->h_addrtype != AF_INET || he->h_addr_list == nullptr) {
        se_netMutex().unlock();
        return false;
    }
    for (char **addr = he->h_addr_list; *addr != nullptr; addr++) {
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        memcpy(&sa.sin_addr, *addr, sizeof(sa.sin_addr));

        SeAddress a;
        a.family = AF_INET;
        a.type = SOCK_STREAM;
        a.protocol = 0;
        a.length = sizeof(sa);
        memcpy(a.storage, &sa, sizeof(sa));
        out.push_back(a);
    }
    se_netMutex().unlock();
    return !out.empty();
#else
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = nullptr;
    std::string portStr = std::to_string(port);
    if (SE_NET_GETADDRINFO(host.c_str(), portStr.c_str(), &hints, &res) != 0 || res == nullptr) return false;

    for (struct addrinfo *ai = res; ai != nullptr; ai = ai->ai_next) {
        if (ai->ai_addrlen > sizeof(SeAddress::storage)) continue;
        SeAddress a;
        a.family = ai->ai_family;
        a.type = ai->ai_socktype;
        a.protocol = ai->ai_protocol;
        a.length = (socklen_t)ai->ai_addrlen;
        memcpy(a.storage, ai->ai_addr, ai->ai_addrlen);
        out.push_back(a);
    }
    SE_NET_FREEADDRINFO(res);
    return !out.empty();
#endif
}

bool SE_Socket::connect(const std::string &host, uint16_t port, int timeoutMs) {
    close();
    impl->timedOut = false;

    std::vector<SeAddress> addresses;
    if (!se_resolve(host, port, addresses)) return false;

    for (const SeAddress &address : addresses) {
        se_sock_t fd = SE_NET_SOCKET(address.family, address.type, address.protocol);
        if (fd == SE_SOCK_INVALID) continue;

        SE_NET_SET_NONBLOCK(fd);

        bool ok = false;
        int r = SE_NET_CONNECT(fd, reinterpret_cast<const struct sockaddr *>(address.storage), (int)address.length);
        if (r == 0) {
            ok = true;
        } else if (SE_NET_IS_BLOCKED(SE_NET_LAST_ERROR())) {
            if (se_waitFor(fd, POLLOUT, timeoutMs) > 0) {
                int err = 0;
                socklen_t errLen = sizeof(err);
                ok = SE_NET_GETSOCKOPT(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &errLen) == 0 && err == 0;
            }
        }

        if (!ok) {
            SE_NET_CLOSE(fd);
            continue;
        }

        int one = 1;
        SE_NET_SETSOCKOPT(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
#ifdef SO_NOSIGPIPE
        SE_NET_SETSOCKOPT(fd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&one, sizeof(one));
#endif
        impl->fd = fd;
        break;
    }

    return impl->fd != SE_SOCK_INVALID;
}

bool SE_Socket::isConnected() const {
    return impl->fd != SE_SOCK_INVALID;
}

int SE_Socket::send(const void *buf, size_t len, int timeoutMs) {
    if (impl->fd == SE_SOCK_INVALID) return -1;
    impl->timedOut = false;

    SeDeadline deadline(timeoutMs);
    const char *p = static_cast<const char *>(buf);
    size_t sent = 0;
    while (sent < len) {
        int r = (int)SE_NET_SEND(impl->fd, p + sent, (int)(len - sent), SE_NET_SEND_FLAGS);
        if (r > 0) {
            sent += (size_t)r;
            continue;
        }
        if (r < 0 && SE_NET_IS_BLOCKED(SE_NET_LAST_ERROR())) {
            int w = se_waitFor(impl->fd, POLLOUT, deadline.msLeft());
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

    SeDeadline deadline(timeoutMs);
    for (;;) {
        int r = (int)SE_NET_RECV(impl->fd, static_cast<char *>(buf), (int)len, 0);
        if (r >= 0) return r;
        if (!SE_NET_IS_BLOCKED(SE_NET_LAST_ERROR())) return -1;

        int w = se_waitFor(impl->fd, POLLIN, deadline.msLeft());
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
