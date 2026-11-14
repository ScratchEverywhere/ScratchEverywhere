#pragma once
#include <cstddef>
#include <se_export.hpp>
#include <socket.hpp>
#include <string>

class SE_EXPORT TlsStream {
  public:
    explicit TlsStream(SE_Socket &socket);
    ~TlsStream();

    TlsStream(const TlsStream &) = delete;
    TlsStream &operator=(const TlsStream &) = delete;

    bool handshake(const std::string &hostname, int timeoutMs = 15000);

    int read(void *buf, size_t len, int timeoutMs = 30000);

    int write(const void *buf, size_t len, int timeoutMs = 30000);

    bool timedOut() const;

    void close();

  private:
    struct Impl;
    Impl *impl;
};
