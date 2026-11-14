#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <se_export.hpp>
#include <socket.hpp>
#include <string>

class TlsStream;

struct SE_EXPORT NetUrl {
    std::string scheme;
    std::string host;
    uint16_t port = 0;
    std::string path = "/";

    bool secure() const { return scheme == "https" || scheme == "wss"; }

    static bool parse(const std::string &url, NetUrl &out);

    NetUrl resolve(const std::string &location, bool *ok = nullptr) const;

    std::string hostHeader() const;
};

class SE_EXPORT NetStream {
  public:
    NetStream();
    ~NetStream();

    NetStream(const NetStream &) = delete;
    NetStream &operator=(const NetStream &) = delete;

    bool open(const NetUrl &url, int timeoutMs = 10000);

    int read(void *buf, size_t len, int timeoutMs = 30000);
    int write(const void *buf, size_t len, int timeoutMs = 30000);
    bool timedOut() const;

    bool waitReadable(int timeoutMs);

    void close();

  private:
    SE_Socket socket;
    std::unique_ptr<TlsStream> tls;
    bool timedOutFlag = false;
};
