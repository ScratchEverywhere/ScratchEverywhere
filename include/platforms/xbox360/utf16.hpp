#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Xbox360 {

/// Converts a UTF-8 C string to a null-terminated UTF-16 vector for Xbox 360 XAM dialogs.
inline std::vector<uint16_t> utf8ToUtf16(const char *utf8) {
    std::vector<uint16_t> out;
    if (!utf8) {
        out.push_back(0);
        return out;
    }
    const auto *s = reinterpret_cast<const unsigned char *>(utf8);
    while (*s) {
        uint32_t cp = 0;
        if (*s < 0x80) {
            cp = *s++;
        } else if ((*s & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
            cp = ((*s & 0x1F) << 6) | (s[1] & 0x3F);
            s += 2;
        } else if ((*s & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
            cp = ((*s & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
            s += 3;
        } else if ((*s & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
            cp = ((*s & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
            s += 4;
        } else {
            cp = 0xFFFD;
            ++s;
        }
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<uint16_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<uint16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<uint16_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    out.push_back(0);
    return out;
}

/// Converts a null-terminated UTF-16 buffer from Xbox 360 XAM dialogs to UTF-8.
inline std::string utf16ToUtf8(const uint16_t *utf16) {
    std::string out;
    if (!utf16) return out;
    for (size_t i = 0; utf16[i] != 0; ++i) {
        uint32_t cp = utf16[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && utf16[i + 1] >= 0xDC00 && utf16[i + 1] <= 0xDFFF) {
            cp = 0x10000 + (((cp - 0xD800) << 10) | (utf16[++i] - 0xDC00));
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

} // namespace Xbox360
