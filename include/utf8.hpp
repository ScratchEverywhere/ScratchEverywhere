#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace utf8 {

inline std::vector<uint32_t> decode(const std::string &text) {
    std::vector<uint32_t> out;
    out.reserve(text.size());

    const unsigned char *data = reinterpret_cast<const unsigned char *>(text.data());
    size_t i = 0;
    const size_t len = text.size();

    auto isContinuation = [&](size_t idx) {
        return idx < len && (data[idx] & 0xC0) == 0x80;
    };

    while (i < len) {
        unsigned char c = data[i];

        if (c < 0x80) {
            out.push_back(c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && isContinuation(i + 1)) {
            uint32_t cp = (c & 0x1Fu) << 6 | (data[i + 1] & 0x3Fu);
            out.push_back(cp);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && isContinuation(i + 1) && isContinuation(i + 2)) {
            uint32_t cp = (c & 0x0Fu) << 12 | (data[i + 1] & 0x3Fu) << 6 | (data[i + 2] & 0x3Fu);
            out.push_back(cp);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && isContinuation(i + 1) && isContinuation(i + 2) && isContinuation(i + 3)) {
            uint32_t cp = (c & 0x07u) << 18 | (data[i + 1] & 0x3Fu) << 12 | (data[i + 2] & 0x3Fu) << 6 | (data[i + 3] & 0x3Fu);
            out.push_back(cp);
            i += 4;
        } else {
            out.push_back(0xFFFD);
            i += 1;
        }
    }

    return out;
}

} // namespace utf8
