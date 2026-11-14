#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>
#include <mbedtls/version.h>
#include <net/tls.hpp>
#include <timer.hpp>
#if defined(SE_MBEDTLS_USER_CONFIG)
#include <mbedtls/platform_time.h>
#endif

#if MBEDTLS_VERSION_MAJOR < 2 || MBEDTLS_VERSION_MAJOR > 3
#error "The mbedTLS TLS backend supports mbedTLS 2.28 and 3.x"
#endif

#define SE_TLS_ERR_SEND_FAILED (-0x004E)
#define SE_TLS_ERR_RECV_FAILED (-0x004C)

struct BioCtx {
    SE_Socket &socket;
    int timeoutMs = 30000;
    bool timedOut = false;

    explicit BioCtx(SE_Socket &s) : socket(s) {}
};

struct TlsStream::Impl {
    BioCtx bio;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;

    explicit Impl(SE_Socket &s) : bio(s) {
        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
    }

    ~Impl() {
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
    }
};

static int bioSend(void *ctx, const unsigned char *buf, size_t len) {
    auto *bio = static_cast<BioCtx *>(ctx);
    int r = bio->socket.send(buf, len, bio->timeoutMs);
    if (r < 0) return bio->socket.timedOut() ? MBEDTLS_ERR_SSL_WANT_WRITE : SE_TLS_ERR_SEND_FAILED;
    return r;
}

static int bioRecv(void *ctx, unsigned char *buf, size_t len) {
    auto *bio = static_cast<BioCtx *>(ctx);
    int r = bio->socket.recv(buf, len, bio->timeoutMs);
    if (r >= 0) return r;
    if (bio->socket.timedOut()) {
        bio->timedOut = true;
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return SE_TLS_ERR_RECV_FAILED;
}

#if defined(SE_MBEDTLS_USER_CONFIG)
extern "C" mbedtls_ms_time_t mbedtls_ms_time(void) {
    static Timer timer;
    return (mbedtls_ms_time_t)timer.getTimeMs();
}
#endif

#if defined(SE_MBEDTLS_USER_CONFIG)
extern "C" int mbedtls_hardware_poll(void *, unsigned char *out, size_t len, size_t *olen) {
    SE_Socket::randomBytes(out, len);
    *olen = len;
    return 0;
}
#endif

static int entropySource(void *, unsigned char *out, size_t len, size_t *olen) {
    SE_Socket::randomBytes(out, len);
    *olen = len;
    return 0;
}

TlsStream::TlsStream(SE_Socket &socket) : impl(new Impl(socket)) {}

TlsStream::~TlsStream() {
    delete impl;
}

bool TlsStream::handshake(const std::string &hostname, int timeoutMs) {
    if (mbedtls_entropy_add_source(&impl->entropy, entropySource, nullptr, 32, MBEDTLS_ENTROPY_SOURCE_STRONG) != 0) return false;
    if (mbedtls_ctr_drbg_seed(&impl->drbg, mbedtls_entropy_func, &impl->entropy, nullptr, 0) != 0) return false;

    if (mbedtls_ssl_config_defaults(&impl->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) return false;
    mbedtls_ssl_conf_authmode(&impl->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&impl->conf, mbedtls_ctr_drbg_random, &impl->drbg);
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    mbedtls_ssl_conf_max_tls_version(&impl->conf, MBEDTLS_SSL_VERSION_TLS1_2);
#endif

    if (mbedtls_ssl_setup(&impl->ssl, &impl->conf) != 0) return false;
    if (mbedtls_ssl_set_hostname(&impl->ssl, hostname.c_str()) != 0) return false;
    mbedtls_ssl_set_bio(&impl->ssl, &impl->bio, bioSend, bioRecv, nullptr);

    impl->bio.timeoutMs = timeoutMs;
    impl->bio.timedOut = false;
    int r;
    while ((r = mbedtls_ssl_handshake(&impl->ssl)) != 0) {
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
            if (impl->bio.timedOut) return false;
            continue;
        }
        return false;
    }
    impl->bio.timeoutMs = 30000;
    return true;
}

int TlsStream::read(void *buf, size_t len, int timeoutMs) {
    impl->bio.timeoutMs = timeoutMs;
    impl->bio.timedOut = false;
    for (;;) {
        int r = mbedtls_ssl_read(&impl->ssl, static_cast<unsigned char *>(buf), len);
        if (r >= 0) return r;
        if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || r == MBEDTLS_ERR_SSL_CONN_EOF) return 0;
        if (r == MBEDTLS_ERR_SSL_WANT_READ && !impl->bio.timedOut) continue;
        if (r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (r == SE_TLS_ERR_RECV_FAILED && !impl->bio.socket.timedOut()) return 0;
        return -1;
    }
}

int TlsStream::write(const void *buf, size_t len, int timeoutMs) {
    impl->bio.timeoutMs = timeoutMs;
    impl->bio.timedOut = false;
    size_t done = 0;
    while (done < len) {
        int r = mbedtls_ssl_write(&impl->ssl, static_cast<const unsigned char *>(buf) + done, len - done);
        if (r > 0) {
            done += (size_t)r;
            continue;
        }
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
            if (impl->bio.timedOut || impl->bio.socket.timedOut()) return -1;
            continue;
        }
        return -1;
    }
    return (int)done;
}

bool TlsStream::timedOut() const {
    return impl->bio.timedOut;
}

void TlsStream::close() {
    mbedtls_ssl_close_notify(&impl->ssl);
}
