#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <se_export.hpp>
#include <string>
#include <utility>
#include <vector>

namespace net_detail {
struct WebSocketState;
}

class SE_EXPORT WebSocket {
  public:
    using MessageHandler = std::function<void(const std::string &message, bool binary)>;

    WebSocket();
    ~WebSocket();

    WebSocket(const WebSocket &) = delete;
    WebSocket &operator=(const WebSocket &) = delete;

    void onMessage(MessageHandler handler);

    bool connect(const std::string &url, const std::vector<std::string> &protocols = {},
                 const std::vector<std::pair<std::string, std::string>> &headers = {}, int timeoutMs = 15000);

    bool sendText(const std::string &message);

    bool pump(int timeoutMs);

    void close(uint16_t code = 1000);

    bool isOpen() const;
    const std::string &error() const;

  private:
    std::unique_ptr<net_detail::WebSocketState> state;
};
