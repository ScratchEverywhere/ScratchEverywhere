#include "math.hpp"
#include "nonstd/expected.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <math.h>
#include <os.hpp>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#ifdef RENDERER_CITRO2D
#include <citro2d.h>
#endif
#ifdef RENDERER_GL2D
#include <nds.h>
#endif

int Math::color(int r, int g, int b, int a) {
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    a = std::clamp(a, 0, 255);

#ifdef RENDERER_GL2D
    int r5 = r >> 3;
    int g5 = g >> 3;
    int b5 = b >> 3;
    return RGB15(r5, g5, b5);
#elif defined(RENDERER_SDL1) || defined(RENDERER_SDL2) || defined(RENDERER_SDL3) || defined(RENDERER_OPENGL) || defined(RENDERER_OPENGL_CORE)
    return (r << 24) |
           (g << 16) |
           (b << 8) |
           a;
#elif defined(RENDERER_CITRO2D)
    return C2D_Color32(r, g, b, a);
#elif !defined(RENDERER_HEADLESS)
#error "You forgot to add your new renderer here didn't you..."
#endif
    return 0;
}

namespace {
std::pair<uint32_t, size_t> decodeUtf8At(std::string_view str, size_t pos) {
    unsigned char c0 = static_cast<unsigned char>(str[pos]);
    if (c0 < 0x80) return {c0, 1};
    size_t len;
    uint32_t cp;
    if ((c0 & 0xE0) == 0xC0) {
        len = 2;
        cp = c0 & 0x1F;
    } else if ((c0 & 0xF0) == 0xE0) {
        len = 3;
        cp = c0 & 0x0F;
    } else if ((c0 & 0xF8) == 0xF0) {
        len = 4;
        cp = c0 & 0x07;
    } else return {c0, 1};
    if (pos + len > str.size()) return {c0, 1};
    for (size_t i = 1; i < len; i++) {
        unsigned char c = static_cast<unsigned char>(str[pos + i]);
        if ((c & 0xC0) != 0x80) return {c0, 1};
        cp = (cp << 6) | (c & 0x3F);
    }
    return {cp, len};
}

bool isJsWhitespaceCodepoint(uint32_t cp) {
    switch (cp) {
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
    case 0x20:
    case 0xA0:
    case 0x1680:
    case 0x2000:
    case 0x2001:
    case 0x2002:
    case 0x2003:
    case 0x2004:
    case 0x2005:
    case 0x2006:
    case 0x2007:
    case 0x2008:
    case 0x2009:
    case 0x200A:
    case 0x2028:
    case 0x2029:
    case 0x202F:
    case 0x205F:
    case 0x3000:
    case 0xFEFF:
        return true;
    default:
        return false;
    }
}
} // namespace

nonstd::expected<double, std::string> Math::parseNumber(std::string_view str) {
    // Trim whitespace
    while (!str.empty()) {
        auto [cp, len] = decodeUtf8At(str, 0);
        if (!isJsWhitespaceCodepoint(cp)) break;
        str.remove_prefix(len);
    }
    while (!str.empty()) {
        size_t start = str.size() - 1;
        while (start > 0 && (static_cast<unsigned char>(str[start]) & 0xC0) == 0x80)
            start--;
        auto [cp, len] = decodeUtf8At(str, start);
        if (start + len != str.size() || !isJsWhitespaceCodepoint(cp)) break;
        str.remove_suffix(len);
    }

    if (str.empty()) return nonstd::make_unexpected("Invalid Argument");

    if (str == "Infinity" || str == "+Infinity") {
        return std::numeric_limits<double>::infinity();
    } else if (str == "-Infinity") {
        return -std::numeric_limits<double>::infinity();
    }

    uint8_t base = 0;
    if (str.size() >= 2 && str[0] == '0') {
        char second = str[1];
        if (second == 'x' || second == 'X') base = 16;
        else if (second == 'b' || second == 'B') base = 2;
        else if (second == 'o' || second == 'O') base = 8;
        if (base != 0) str.remove_prefix(2);
    }

    if (base != 0) {
        double conversion = 0;
        for (char c : str) {
            int digit;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else return nonstd::make_unexpected("Invalid Argument");

            if (digit >= base) return nonstd::make_unexpected("Invalid Argument");
            conversion = conversion * base + digit;
        }
        return conversion;
    }

    size_t e_pos = str.find_first_of("eE");
    if (e_pos != std::string_view::npos) {
        if (str.find('.', e_pos + 1) != std::string_view::npos) {
            return nonstd::make_unexpected("Invalid Argument");
        }
    }

    if (!str.empty() && str.front() == '+') {
        str.remove_prefix(1);
    }

    double conversion = 0;

#if defined(__cpp_lib_to_chars) && (__cpp_lib_to_chars >= 201611L)
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), conversion);

    if (ec == std::errc::invalid_argument || ptr != str.data() + str.size()) {
        return nonstd::make_unexpected("Invalid Argument");
    }

    if (ec == std::errc::result_out_of_range) {
        if (str.size() >= 128) return nonstd::make_unexpected("Invalid Argument");
        char stack_buf[128];
        std::memcpy(stack_buf, str.data(), str.size());
        stack_buf[str.size()] = '\0';
        conversion = std::strtod(stack_buf, nullptr);
    } else if (std::isinf(conversion)) {
        return nonstd::make_unexpected("Invalid Argument");
    }
#else
    char stack_buf[128];
    if (str.size() >= sizeof(stack_buf)) {
        return nonstd::make_unexpected("Invalid Argument");
    }

    std::memcpy(stack_buf, str.data(), str.size());
    stack_buf[str.size()] = '\0';

    char *endptr = nullptr;
    errno = 0;
    conversion = std::strtod(stack_buf, &endptr);

    if (endptr != stack_buf + str.size()) {
        return nonstd::make_unexpected("Invalid Argument");
    }
    if (std::isinf(conversion) && errno != ERANGE) {
        return nonstd::make_unexpected("Invalid Argument");
    }
#endif

    return conversion;
}

bool Math::isNumber(const std::string &str) {
    return parseNumber(str).has_value();
}

std::string Math::toString(double number) {
    if (std::isnan(number)) return "NaN";
    if (std::isinf(number)) return std::signbit(number) ? "-Infinity" : "Infinity";
    if (number == 0) return "0";
    char buffer[32];
    d2s_buffered(number, buffer);
    return std::string(buffer);
}

double Math::degreesToRadians(double degrees) {
    return degrees * (M_PI / 180.0);
}

double Math::radiansToDegrees(double radians) {
    return radians * (180.0 / M_PI);
}

int16_t Math::radiansToAngle16(float radians) {
    return (int16_t)(radians * (32768.0f / (2.0f * M_PI)));
}

std::string Math::generateRandomString(int length) {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-=[];',./_+{}|:<>?~`";
    std::string result;

    static std::mt19937 generator(static_cast<unsigned int>(std::time(nullptr)));
    std::uniform_int_distribution<> distribution(0, chars.size() - 1);

    for (int i = 0; i < length; i++) {
        result += chars[distribution(generator)];
    }

    return result;
}

std::string Math::removeQuotations(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](char c) { return c == '"'; }), value.end());
    return value;
}

const uint32_t Math::next_pow2(uint32_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}
