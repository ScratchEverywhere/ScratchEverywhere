#include "math.hpp"
#include "nonstd/expected.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <math.h>
#include <os.hpp>
#include <random>
#include <stdexcept>
#include <string>
#ifdef RENDERER_CITRO2D
#include <citro2d.h>
#endif
#ifdef RENDERER_GL2D
#include <nds.h>
#endif

#ifdef PLAYDATE
#include <pdcpp/pdnewlib.h>

extern PlaydateAPI *pd;
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
#elif defined(RENDERER_SDL1) || defined(RENDERER_SDL2) || defined(RENDERER_SDL3) || defined(RENDERER_OPENGL)
    return (r << 24) |
           (g << 16) |
           (b << 8) |
           a;
#elif defined(RENDERER_CITRO2D)
    return C2D_Color32(r, g, b, a);
#endif
    return 0;
}

nonstd::expected<double, std::string> Math::parseNumber(std::string str) {
    // Scratch has whitespace trimming
    while (!str.empty() && std::isspace(str[0])) {
        str.erase(0, 1);
    }
    while (!str.empty() && std::isspace(str.back())) {
        str.pop_back();
    }

    if (str.empty()) return nonstd::make_unexpected("Invalid Argument");

    if (str == "Infinity" || str == "+Infinity") {
        return std::numeric_limits<double>::infinity();
    } else if (str == "-Infinity") {
        return -std::numeric_limits<double>::infinity();
    }

    uint8_t base = 0;
    std::string validcharacters = "0123456789+-eE.";
    if (str[0] == '0') {
        if (str[1] == 'x' || str[1] == 'X') {
            base = 16;
            validcharacters = "0123456789ABCDEFabcedef";
        } else if (str[1] == 'b' || str[1] == 'B') {
            base = 2;
            validcharacters = "01";
        } else if (str[1] == 'o' || str[1] == 'O') {
            base = 8;
            validcharacters = "01234567";
        }
        if (base != 0) str = str.substr(2, str.length() - 2);
    }

    for (size_t i = 0; i < str.length(); i++) {
        if (validcharacters.find(str[i]) == std::string::npos) {
            return nonstd::make_unexpected("Invalid Argument");
        }
        if (base == 0) {
            if (i == str.length() - 1 && (str[i] == '+' || str[i] == '-' || str[i] == 'e' || str[i] == 'E')) {
                // implementation differece, "1e" doesn't work in Scratch but works
                // with std::stod()
                // signs (+, -) should also not be at the end
                return nonstd::make_unexpected("Invalid Argument");
            }
            if ((str[i] == 'e' || str[i] == 'E') && str.find('.', i + 1) != std::string::npos) {
                // implementation differece, decimal point after e doesn't work in
                // Scratch but works with std::stod()
                return nonstd::make_unexpected("Invalid Argument");
            }
        }
    }

    double conversion;
    char *endptr = nullptr;

    errno = 0;
    if (base == 0) {
        conversion = std::strtod(str.c_str(), &endptr);
    } else {
        int power = 0;
        int digit = 0;
        char c;
        for (int i = str.length() - 1; i >= 0; i--) {
            c = str[i];
            if (c >= '0' && c <= '9') {
                digit = c - '0';
            } else if (c >= 'A' && c <= 'F') {
                digit = c - 'A' + 10;
            } else if (c >= 'a' && c <= 'f') {
                digit = c - 'a' + 10;
            } else {
                return nonstd::make_unexpected("Invalid Argument");
            }
            conversion += digit * std::pow(base, power);
            power++;
        }
    }
    if (errno == ERANGE) {
        if (str[0] == '-') return -std::numeric_limits<double>::infinity();
        else return std::numeric_limits<double>::infinity();
    }

    return conversion;
}

bool Math::isNumber(const std::string &str) {
    return parseNumber(str).has_value();
}

std::string Math::toString(double number) {
    if (std::isnan(number)) return "NaN";
    if (std::isinf(number)) return std::signbit(number) ? "-Infinity" : "Infinity";
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

#ifdef PLAYDATE
    static std::mt19937 generator(pd->system->getCurrentTimeMilliseconds());
#else
    static std::random_device rd;
    static std::mt19937 generator(rd());
#endif
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
