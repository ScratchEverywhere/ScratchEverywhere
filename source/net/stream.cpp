#include <net/stream.hpp>
#include <net/tls.hpp>

#include <algorithm>
#include <cctype>

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool NetUrl::parse(const std::string &input, NetUrl &out) {
    size_t schemeEnd = input.find("://");
    if (schemeEnd == std::string::npos) return false;

    NetUrl url;
    url.scheme = lower(input.substr(0, schemeEnd));
    if (url.scheme != "http" && url.scheme != "https" && url.scheme != "ws" && url.scheme != "wss") return false;

    std::string rest = input.substr(schemeEnd + 3);
    size_t frag = rest.find('#');
    if (frag != std::string::npos) rest.erase(frag);

    size_t pathStart = rest.find_first_of("/?");
    std::string authority = rest.substr(0, pathStart);
    if (pathStart == std::string::npos) {
        url.path = "/";
    } else if (rest[pathStart] == '?') {
        url.path = "/" + rest.substr(pathStart);
    } else {
        url.path = rest.substr(pathStart);
    }

    size_t at = authority.rfind('@');
    if (at != std::string::npos) authority.erase(0, at + 1);

    std::string portStr;
    if (!authority.empty() && authority[0] == '[') {
        size_t close = authority.find(']');
        if (close == std::string::npos) return false;
        url.host = authority.substr(1, close - 1);
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':') return false;
            portStr = authority.substr(close + 2);
        }
    } else {
        size_t colon = authority.rfind(':');
        if (colon == std::string::npos) {
            url.host = authority;
        } else {
            url.host = authority.substr(0, colon);
            portStr = authority.substr(colon + 1);
        }
    }
    if (url.host.empty()) return false;

    if (portStr.empty()) {
        url.port = url.secure() ? 443 : 80;
    } else {
        if (portStr.size() > 5 || !std::all_of(portStr.begin(), portStr.end(), [](unsigned char c) { return std::isdigit(c); })) return false;
        int port = std::stoi(portStr);
        if (port < 1 || port > 65535) return false;
        url.port = (uint16_t)port;
    }

    out = url;
    return true;
}

NetUrl NetUrl::resolve(const std::string &location, bool *ok) const {
    NetUrl result = *this;
    if (ok) *ok = true;

    if (location.find("://") != std::string::npos) {
        if (!NetUrl::parse(location, result) && ok) *ok = false;
        return result;
    }

    if (location.compare(0, 2, "//") == 0) {
        if (!NetUrl::parse(scheme + ":" + location, result) && ok) *ok = false;
        return result;
    }

    if (!location.empty() && location[0] == '/') {
        std::string frag = location.substr(0, location.find('#'));
        result.path = frag;
    } else {
        std::string base = path.substr(0, path.find('?'));
        base.erase(base.rfind('/') + 1);
        std::string frag = location.substr(0, location.find('#'));
        result.path = (!frag.empty() && frag[0] == '?') ? path.substr(0, path.find('?')) + frag : base + frag;
    }
    return result;
}

std::string NetUrl::hostHeader() const {
    std::string h = host.find(':') != std::string::npos ? "[" + host + "]" : host;
    uint16_t defaultPort = secure() ? 443 : 80;
    if (port != defaultPort) h += ":" + std::to_string(port);
    return h;
}

NetStream::NetStream() = default;

NetStream::~NetStream() {
    close();
}

bool NetStream::open(const NetUrl &url, int timeoutMs) {
    close();
    if (!socket.connect(url.host, url.port, timeoutMs)) return false;
    if (url.secure()) {
        tls = std::make_unique<TlsStream>(socket);
        if (!tls->handshake(url.host, timeoutMs)) {
            close();
            return false;
        }
    }
    return true;
}

int NetStream::read(void *buf, size_t len, int timeoutMs) {
    timedOutFlag = false;
    int r = tls ? tls->read(buf, len, timeoutMs) : socket.recv(buf, len, timeoutMs);
    if (r < 0) timedOutFlag = tls ? tls->timedOut() : socket.timedOut();
    return r;
}

int NetStream::write(const void *buf, size_t len, int timeoutMs) {
    timedOutFlag = false;
    int r = tls ? tls->write(buf, len, timeoutMs) : socket.send(buf, len, timeoutMs);
    if (r < 0) timedOutFlag = tls ? tls->timedOut() : socket.timedOut();
    return r;
}

bool NetStream::timedOut() const {
    return timedOutFlag;
}

bool NetStream::waitReadable(int timeoutMs) {
    return socket.waitReadable(timeoutMs);
}

void NetStream::close() {
    if (tls) {
        tls->close();
        tls.reset();
    }
    socket.close();
}
