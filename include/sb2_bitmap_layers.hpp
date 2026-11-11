#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace Sb2BitmapLayers {
inline constexpr const char *prefix = "__sb2text__";

// Distinct text layers over a shared base (x3/x2/x1) need distinct cache keys.
inline std::string key(const std::string &base, const std::string &text) {
    return std::string(prefix) + base + "__" + text;
}

inline std::optional<std::pair<std::string, std::string>> split(const std::string &name) {
    const std::string marker(prefix);
    if (name.compare(0, marker.size(), marker) != 0) return std::nullopt;
    const auto separator = name.find("__", marker.size());
    if (separator == std::string::npos || separator == marker.size() || separator + 2 == name.size()) return std::nullopt;
    return std::make_pair(name.substr(marker.size(), separator - marker.size()), name.substr(separator + 2));
}

// Canvas source-over on straight RGBA, at (0,0), clipped to the base image.
// Merge before any half-quality or hardware-size reduction.
inline void composite(uint8_t *base, int width, int height, const uint8_t *text, int textWidth, int textHeight) {
    for (int y = 0; y < std::min(height, textHeight); ++y) {
        for (int x = 0; x < std::min(width, textWidth); ++x) {
            auto *dst = base + (static_cast<std::size_t>(y) * width + x) * 4;
            const auto *src = text + (static_cast<std::size_t>(y) * textWidth + x) * 4;
            const uint32_t sa = src[3], da = dst[3];
            if (sa == 0) continue;
            const uint32_t alpha = sa * 255 + da * (255 - sa);
            for (int c = 0; c < 3; ++c) {
                const uint32_t color = src[c] * sa * 255 + dst[c] * da * (255 - sa);
                dst[c] = static_cast<uint8_t>((color + alpha / 2) / alpha);
            }
            dst[3] = static_cast<uint8_t>((alpha + 127) / 255);
        }
    }
}
}
