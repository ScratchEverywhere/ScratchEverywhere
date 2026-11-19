#include <net/http.hpp>
#include <net/stream.hpp>
#include <socket.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

class Reader {
  public:
    Reader(NetStream &stream, int timeoutMs) : stream(stream), timeoutMs(timeoutMs) {}

    bool readLine(std::string &line, size_t maxLen = 16384) {
        line.clear();
        for (;;) {
            while (pos < len) {
                char c = buf[pos++];
                if (c == '\n') {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    return true;
                }
                line += c;
                if (line.size() > maxLen) return false;
            }
            if (!fill()) return false;
        }
    }

    bool readBody(size_t n, const HttpSink &sink) {
        while (n > 0) {
            if (pos == len && !fill()) return false;
            size_t chunk = std::min(n, len - pos);
            if (!sink(reinterpret_cast<const uint8_t *>(buf) + pos, chunk)) {
                aborted = true;
                return false;
            }
            pos += chunk;
            n -= chunk;
        }
        return true;
    }

    bool readUntilClose(const HttpSink &sink) {
        for (;;) {
            if (pos < len) {
                if (!sink(reinterpret_cast<const uint8_t *>(buf) + pos, len - pos)) {
                    aborted = true;
                    return false;
                }
                pos = len;
            }
            if (!fill()) return eof;
        }
    }

    bool consume(size_t n) {
        while (n > 0) {
            if (pos == len && !fill()) return false;
            size_t chunk = std::min(n, len - pos);
            pos += chunk;
            n -= chunk;
        }
        return true;
    }

    bool aborted = false;
    bool eof = false;

  private:
    bool fill() {
        int r = stream.read(buf, sizeof(buf), timeoutMs);
        if (r <= 0) {
            eof = r == 0;
            return false;
        }
        pos = 0;
        len = (size_t)r;
        return true;
    }

    NetStream &stream;
    int timeoutMs;
    char buf[4096];
    size_t pos = 0, len = 0;
};

bool readChunked(Reader &reader, const HttpSink &sink) {
    std::string line;
    for (;;) {
        if (!reader.readLine(line)) return false;
        size_t semi = line.find(';');
        std::string hex = trim(line.substr(0, semi));
        if (hex.empty() || hex.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return false;
        size_t size = (size_t)strtoull(hex.c_str(), nullptr, 16);

        if (size == 0) {
            do {
                if (!reader.readLine(line)) return false;
            } while (!line.empty());
            return true;
        }

        if (!reader.readBody(size, sink)) return false;
        if (!reader.readLine(line) || !line.empty()) return false;
    }
}

enum class Result { Done,
                    Redirect,
                    Failed };

Result request(const NetUrl &url, const HttpSink &sink, HttpResponse &response, const HttpOptions &options, std::string &location) {
    NetStream stream;
    if (!stream.open(url, std::min(options.timeoutMs, 15000))) {
        response.error = "Could not connect to " + url.host;
        return Result::Failed;
    }

    std::string req = "GET " + url.path + " HTTP/1.1\r\n";
    req += "Host: " + url.hostHeader() + "\r\n";
    req += "User-Agent: " + options.userAgent + "\r\n";
    req += "Accept: */*\r\n";
    req += "Connection: close\r\n";
    for (const auto &h : options.headers)
        req += h.first + ": " + h.second + "\r\n";
    req += "\r\n";

    if (stream.write(req.data(), req.size(), options.timeoutMs) < 0) {
        response.error = "Failed to send request";
        return Result::Failed;
    }

    Reader reader(stream, options.timeoutMs);
    std::string line;
    if (!reader.readLine(line)) {
        response.error = stream.timedOut() ? "Timed out waiting for response" : "Connection closed before response";
        return Result::Failed;
    }

    if (line.compare(0, 5, "HTTP/") != 0) {
        response.error = "Malformed status line";
        return Result::Failed;
    }
    size_t sp = line.find(' ');
    if (sp == std::string::npos) {
        response.error = "Malformed status line";
        return Result::Failed;
    }
    response.status = atoi(line.c_str() + sp + 1);
    response.headers.clear();

    for (;;) {
        if (!reader.readLine(line)) {
            response.error = "Malformed headers";
            return Result::Failed;
        }
        if (line.empty()) break;
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name = toLower(trim(line.substr(0, colon)));
        std::string value = trim(line.substr(colon + 1));
        auto it = response.headers.find(name);
        if (it != response.headers.end()) {
            it->second += ", " + value;
        } else {
            response.headers[name] = value;
        }
    }

    int status = response.status;
    if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
        auto loc = response.headers.find("location");
        if (loc != response.headers.end() && !loc->second.empty()) {
            location = loc->second;
            return Result::Redirect;
        }
    }

    if (status < 200 || status >= 300) {
        response.error = "HTTP " + std::to_string(status);
        return Result::Failed;
    }

    bool ok;
    auto te = response.headers.find("transfer-encoding");
    auto cl = response.headers.find("content-length");
    if (te != response.headers.end() && toLower(te->second).find("chunked") != std::string::npos) {
        ok = readChunked(reader, sink);
    } else if (cl != response.headers.end()) {
        char *end = nullptr;
        unsigned long long n = strtoull(cl->second.c_str(), &end, 10);
        if (end == cl->second.c_str()) {
            response.error = "Bad Content-Length";
            return Result::Failed;
        }
        ok = reader.readBody((size_t)n, sink);
    } else {
        ok = reader.readUntilClose(sink);
    }

    if (!ok) {
        response.error = reader.aborted ? "Aborted" : stream.timedOut() ? "Timed out during transfer"
                                                                        : "Connection lost during transfer";
        return Result::Failed;
    }
    return Result::Done;
}

} // namespace

bool Http::get(const std::string &urlStr, const HttpSink &sink, HttpResponse &response, const HttpOptions &options) {
    response = HttpResponse();

    NetUrl url;
    if (!NetUrl::parse(urlStr, url) || (url.scheme != "http" && url.scheme != "https")) {
        response.error = "Unsupported or invalid URL";
        return false;
    }

    if (!SE_Socket::init()) {
        response.error = "Network unavailable";
        return false;
    }

    bool success = false;
    for (int redirects = 0;; redirects++) {
        response.finalUrl = url.scheme + "://" + url.hostHeader() + url.path;
        std::string location;
        Result r = request(url, sink, response, options, location);
        if (r == Result::Done) {
            success = true;
            break;
        }
        if (r == Result::Failed) break;

        if (redirects >= options.maxRedirects) {
            response.error = "Too many redirects";
            break;
        }
        bool ok;
        url = url.resolve(location, &ok);
        if (!ok || (url.scheme != "http" && url.scheme != "https")) {
            response.error = "Bad redirect target";
            break;
        }
    }

    SE_Socket::deinit();
    return success;
}

bool Http::get(const std::string &url, std::string &body, HttpResponse &response, const HttpOptions &options) {
    body.clear();
    return Http::get(
        url, [&body](const uint8_t *data, size_t len) {
            body.append(reinterpret_cast<const char *>(data), len);
            return true;
        },
        response, options);
}
