#include <net/tls.hpp>

#include <Security/SecureTransport.h>

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

struct BioCtx {
    SE_Socket &socket;
    int timeoutMs = 30000;
    bool timedOut = false;

    explicit BioCtx(SE_Socket &s) : socket(s) {}
};

struct TlsStream::Impl {
    BioCtx bio;
    SSLContextRef ctx = nullptr;

    explicit Impl(SE_Socket &s) : bio(s) {}

    ~Impl() {
        if (ctx) CFRelease(ctx);
    }
};

static OSStatus readCallback(SSLConnectionRef connection, void *data, size_t *length) {
    auto *bio = const_cast<BioCtx *>(static_cast<const BioCtx *>(connection));
    int r = bio->socket.recv(data, *length, bio->timeoutMs);
    if (r > 0) {
        OSStatus status = (size_t)r < *length ? errSSLWouldBlock : noErr;
        *length = (size_t)r;
        return status;
    }
    *length = 0;
    if (r == 0) return errSSLClosedGraceful;
    if (bio->socket.timedOut()) {
        bio->timedOut = true;
        return errSSLWouldBlock;
    }
    return errSSLClosedAbort;
}

static OSStatus writeCallback(SSLConnectionRef connection, const void *data, size_t *length) {
    auto *bio = const_cast<BioCtx *>(static_cast<const BioCtx *>(connection));
    int r = bio->socket.send(data, *length, bio->timeoutMs);
    if (r >= 0) return noErr;
    *length = 0;
    if (bio->socket.timedOut()) {
        bio->timedOut = true;
        return errSSLWouldBlock;
    }
    return errSSLClosedAbort;
}

TlsStream::TlsStream(SE_Socket &socket) : impl(new Impl(socket)) {}

TlsStream::~TlsStream() {
    delete impl;
}

bool TlsStream::handshake(const std::string &hostname, int timeoutMs) {
    impl->ctx = SSLCreateContext(nullptr, kSSLClientSide, kSSLStreamType);
    if (!impl->ctx) return false;

    if (SSLSetIOFuncs(impl->ctx, readCallback, writeCallback) != noErr) return false;
    if (SSLSetConnection(impl->ctx, &impl->bio) != noErr) return false;
    if (SSLSetPeerDomainName(impl->ctx, hostname.c_str(), hostname.size()) != noErr) return false;
    SSLSetProtocolVersionMin(impl->ctx, kTLSProtocol12);

    if (SSLSetSessionOption(impl->ctx, kSSLSessionOptionBreakOnServerAuth, true) != noErr) return false;

    impl->bio.timeoutMs = timeoutMs;
    impl->bio.timedOut = false;
    OSStatus status;
    do {
        status = SSLHandshake(impl->ctx);
    } while (status == errSSLServerAuthCompleted || (status == errSSLWouldBlock && !impl->bio.timedOut));
    impl->bio.timeoutMs = 30000;
    return status == noErr;
}

int TlsStream::read(void *buf, size_t len, int timeoutMs) {
    if (!impl->ctx) return -1;
    impl->bio.timeoutMs = timeoutMs;
    impl->bio.timedOut = false;
    for (;;) {
        size_t processed = 0;
        OSStatus status = SSLRead(impl->ctx, buf, len, &processed);
        if (processed > 0) return (int)processed;
        if (status == noErr) continue;
        if (status == errSSLClosedGraceful || status == errSSLClosedNoNotify || status == errSSLClosedAbort) return 0;
        if (status == errSSLWouldBlock && !impl->bio.timedOut) continue;
        return -1;
    }
}

int TlsStream::write(const void *buf, size_t len, int timeoutMs) {
    if (!impl->ctx) return -1;
    impl->bio.timeoutMs = timeoutMs;
    impl->bio.timedOut = false;
    size_t done = 0;
    while (done < len) {
        size_t processed = 0;
        OSStatus status = SSLWrite(impl->ctx, static_cast<const char *>(buf) + done, len - done, &processed);
        done += processed;
        if (status == noErr) continue;
        if (status == errSSLWouldBlock && !impl->bio.timedOut) continue;
        return -1;
    }
    return (int)done;
}

bool TlsStream::timedOut() const {
    return impl->bio.timedOut;
}

void TlsStream::close() {
    if (impl->ctx) SSLClose(impl->ctx);
}
