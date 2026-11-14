#pragma once
#include <cstddef>
#include <cstdint>
#include <se_export.hpp>
#include <string>

class SE_EXPORT SE_Socket {
  public:
    SE_Socket();
    ~SE_Socket();

    SE_Socket(const SE_Socket &) = delete;
    SE_Socket &operator=(const SE_Socket &) = delete;

    static bool init();
    static void deinit();

    static void randomBytes(void *buf, size_t len);

    bool connect(const std::string &host, uint16_t port, int timeoutMs = 10000);
    bool isConnected() const;

    int send(const void *buf, size_t len, int timeoutMs = 30000);

    int recv(void *buf, size_t len, int timeoutMs = 30000);
    bool timedOut() const;

    bool waitReadable(int timeoutMs);

    void close();

  private:
    struct Impl;
    Impl *impl;
};
