#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#ifdef __3DS__
#include <citro2d.h>
#endif
#ifdef __NDS__
#include <nds.h>
#endif

int Math::color(int r, int g, int b, int a) {
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    a = std::clamp(a, 0, 255);

#ifdef __NDS__
    int r5 = r >> 3;
    int g5 = g >> 3;
    int b5 = b >> 3;
    return RGB15(r5, g5, b5);
#elif defined(RENDERER_SDL1) || defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
    return (r << 24) |
           (g << 16) |
           (b << 8) |
           a;
#elif defined(__3DS__)
    return C2D_Color32(r, g, b, a);
#endif
    return 0;
}

std::optional<double> Math::parseNumber(std::string_view str) {
    // Scratch has whitespace trimming
    while (!str.empty() &&
           std::isspace(static_cast<unsigned char>(str.front())))
        str.remove_prefix(1);
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back())))
        str.remove_suffix(1);
    if (str.empty()) return std::nullopt;

    // Scratch has hex/binary/octal support with prefix
    int base = 10;
    if (str[0] == '0' && str.size() >= 2) {
        char prefix = str[1];
        if (prefix == 'x' || prefix == 'X') {
            base = 16;
            str.remove_prefix(2);
        } else if (prefix == 'b' || prefix == 'B') {
            base = 2;
            str.remove_prefix(2);
        } else if (prefix == 'o' || prefix == 'O') {
            base = 8;
            str.remove_prefix(2);
        }
    }

    if (str.empty()) return std::nullopt;

    double num = 0.0;

    // Decimal conversion
    if (base == 10) {
        size_t i = 0;

        if (str == "Infinity") {
            return std::numeric_limits<double>::infinity();
        }

        bool negative = false;
        if (i < str.size() && (str[i] == '+' || str[i] == '-')) {
            negative = (str[i] == '-');
            i++;
            if (i == str.size()) return std::nullopt;
        }

        // "+Infinity" or "-Infinity" invalid
        if (str == "Infinity") return std::nullopt;

        // Integer part
        double value = 0.0;
        bool hasDigit = false;
        while (i < str.size() && (unsigned)(str[i] - '0') < 10u) {
            hasDigit = true;
            value = value * 10.0 + (str[i] - '0');
            i++;
        }

        // Fractional part
        if (i < str.size() && str[i] == '.') {
            i++;
            double place = 0.1;
            if (i == str.size() && !hasDigit)
                return std::nullopt; // "." or "+." invalid

            while (i < str.size() && (unsigned)(str[i] - '0') < 10u) {
                hasDigit = true;
                value += (str[i] - '0') * place;
                place *= 0.1;
                i++;
            }
        }

        if (!hasDigit) return std::nullopt;

        // Exponent
        int exponent = 0;
        if (i < str.size() && (str[i] == 'e' || str[i] == 'E')) {
            i++;

            if (i == str.size()) return std::nullopt;

            bool expNegative = false;
            if (str[i] == '+' || str[i] == '-') {
                expNegative = (str[i] == '-');
                i++;
                if (i == str.size()) return std::nullopt; // "1e+" invalid
            }

            if (!((unsigned)(str[i] - '0') < 10u)) return std::nullopt;

            // parse exponent digits
            int expValue = 0;
            while (i < str.size() && (unsigned)(str[i] - '0') < 10u) {
                if (expValue < 10000) expValue = expValue * 10 + (str[i] - '0');
                i++;
            }
            exponent = expNegative ? -expValue : expValue;
        }

        if (i != str.size()) return std::nullopt;

        // Apply exponent safely
        if (exponent != 0) {
            errno = 0;
            double scale = std::pow(10.0, (double)exponent);

            if (errno == ERANGE || !std::isfinite(scale)) {
                return negative ? -std::numeric_limits<double>::infinity()
                                : std::numeric_limits<double>::infinity();
            }

            value *= scale;
        }

        if (negative) value = -value;

        // Overflow handling
        if (!std::isfinite(value)) {
            return negative ? -std::numeric_limits<double>::infinity()
                            : std::numeric_limits<double>::infinity();
        }

        return value;
        // Hex/binary/octal conversion: integer-based conversion only
    } else {
        auto val_for_base = [](unsigned char c, int base) -> int8_t {
            switch (base) {
            case 16:
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                return -1;
            case 2:
                return (c == '0' || c == '1') ? c - '0' : -1;
            case 8:
                return (c >= '0' && c <= '7') ? c - '0' : -1;
            default:
                return -1; // unreachable
            }
        };

        for (size_t i = 0; i < str.size(); i++) {
            int8_t val = val_for_base(str[i], base);
            if (val == -1) return std::nullopt;
            // num = num * base + val
            num = std::fma<double>(num, base, val);
        }
    }

    return num;
}

bool Math::isNumber(const std::string_view str) {
    auto result = parseNumber(str);
    return result ? true : false
}

double Math::degreesToRadians(double degrees) {
    return degrees * (M_PI / 180.0);
}

double Math::radiansToDegrees(double radians) {
    return radians * (180.0 / M_PI);
}

std::string Math::generateRandomString(int length) {
    std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz12"
                        "34567890-=[];',./_+{}|:<>?~`";
    std::string result;

    static std::mt19937 generator(
        static_cast<unsigned int>(std::time(nullptr)));
    std::uniform_int_distribution<> distribution(0, chars.size() - 1);

    for (int i = 0; i < length; i++) {
        result += chars[distribution(generator)];
    }

    return result;
}

std::string Math::removeQuotations(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(),
                               [](char c) { return c == '"'; }),
                value.end());
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
