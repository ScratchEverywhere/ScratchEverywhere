#include <net/tls.hpp>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

struct BioCtx {
    SE_Socket &socket;
    int timeoutMs = 30000;
    bool timedOut = false;

    explicit BioCtx(SE_Socket &s) : socket(s) {}
};

struct TlsStream::Impl {
    BioCtx bio;
    SSL_CTX *ctx = nullptr;
    SSL *ssl = nullptr;
    BIO_METHOD *method = nullptr;

    explicit Impl(SE_Socket &s) : bio(s) {}
};

static int bioWrite(BIO *bio, const char *buf, int len) {
    auto *impl = static_cast<BioCtx *>(BIO_get_data(bio));
    BIO_clear_retry_flags(bio);
    int r = impl->socket.send(buf, (size_t)len, impl->timeoutMs);
    return r < 0 ? -1 : r;
}

static int bioRead(BIO *bio, char *buf, int len) {
    auto *impl = static_cast<BioCtx *>(BIO_get_data(bio));
    BIO_clear_retry_flags(bio);
    int r = impl->socket.recv(buf, (size_t)len, impl->timeoutMs);
    if (r > 0) return r;
    if (r == 0) return 0;
    if (impl->socket.timedOut()) {
        BIO_set_retry_read(bio);
        impl->timedOut = true;
    }
    return -1;
}

static long bioCtrl(BIO *, int cmd, long, void *) {
    return cmd == BIO_CTRL_FLUSH ? 1 : 0;
}

TlsStream::TlsStream(SE_Socket &socket) : impl(new Impl(socket)) {}

TlsStream::~TlsStream() {
    if (impl->ssl) SSL_free(impl->ssl);
    if (impl->ctx) SSL_CTX_free(impl->ctx);
    if (impl->method) BIO_meth_free(impl->method);
    delete impl;
}

bool TlsStream::handshake(const std::string &hostname, int timeoutMs) {
    impl->ctx = SSL_CTX_new(TLS_client_method());
    if (!impl->ctx) return false;
    SSL_CTX_set_verify(impl->ctx, SSL_VERIFY_NONE, nullptr);
    SSL_CTX_set_min_proto_version(impl->ctx, TLS1_2_VERSION);

    impl->method = BIO_meth_new(BIO_TYPE_SOURCE_SINK | BIO_get_new_index(), "SE_Socket");
    if (!impl->method) return false;
    BIO_meth_set_write(impl->method, bioWrite);
    BIO_meth_set_read(impl->method, bioRead);
    BIO_meth_set_ctrl(impl->method, bioCtrl);

    BIO *bio = BIO_new(impl->method);
    if (!bio) return false;
    BIO_set_data(bio, &impl->bio);
    BIO_set_init(bio, 1);

    impl->ssl = SSL_new(impl->ctx);
    if (!impl->ssl) {
        BIO_free(bio);
        return false;
    }
    SSL_set_bio(impl->ssl, bio, bio);
    SSL_set_tlsext_host_name(impl->ssl, hostname.c_str());

    impl->bio.timeoutMs = timeoutMs;
    impl->bio.timedOut = false;
    bool ok = SSL_connect(impl->ssl) == 1;
    impl->bio.timeoutMs = 30000;
    if (!ok) ERR_clear_error();
    return ok;
}

int TlsStream::read(void *buf, size_t len, int timeoutMs) {
    if (!impl->ssl) return -1;
    impl->bio.timeoutMs = timeoutMs;
    impl->bio.timedOut = false;
    int r = SSL_read(impl->ssl, buf, (int)len);
    if (r > 0) return r;
    int err = SSL_get_error(impl->ssl, r);
    ERR_clear_error();
    if (err == SSL_ERROR_ZERO_RETURN) return 0;
    if (err == SSL_ERROR_SYSCALL && r == 0) return 0;
    return -1;
}

int TlsStream::write(const void *buf, size_t len, int timeoutMs) {
    if (!impl->ssl) return -1;
    impl->bio.timeoutMs = timeoutMs;
    impl->bio.timedOut = false;
    size_t done = 0;
    while (done < len) {
        int r = SSL_write(impl->ssl, static_cast<const char *>(buf) + done, (int)(len - done));
        if (r <= 0) {
            ERR_clear_error();
            return -1;
        }
        done += (size_t)r;
    }
    return (int)done;
}

bool TlsStream::timedOut() const {
    return impl->bio.timedOut;
}

void TlsStream::close() {
    if (impl->ssl) {
        SSL_shutdown(impl->ssl);
        ERR_clear_error();
    }
}
