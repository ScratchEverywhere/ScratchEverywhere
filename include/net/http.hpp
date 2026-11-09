#pragma once
#include "version_config.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <se_export.hpp>
#include <string>
#include <utility>
#include <vector>

struct SE_EXPORT HttpOptions {
    int timeoutMs = 30000;
    int maxRedirects = 5;
    std::string userAgent = std::string("ScratchEverywhere/") + SE_VERSION_STRING;
    std::vector<std::pair<std::string, std::string>> headers;
};

struct SE_EXPORT HttpResponse {
    int status = 0;
    std::map<std::string, std::string> headers;
    std::string finalUrl;
    std::string error;
};

using HttpSink = std::function<bool(const uint8_t *data, size_t len)>;

namespace Http {

SE_EXPORT bool get(const std::string &url, const HttpSink &sink, HttpResponse &response, const HttpOptions &options = {});

SE_EXPORT bool get(const std::string &url, std::string &body, HttpResponse &response, const HttpOptions &options = {});

} // namespace Http
