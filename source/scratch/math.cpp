#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#ifdef __3DS__
#include <citro2d.h>
#elif defined(__NDS__)
#include <nds.h>
#endif

uint32_t Math::color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
#ifdef __NDS__
    int r5 = r >> 3;
    int g5 = g >> 3;
    int b5 = b >> 3;
    return RGB15(r5, g5, b5);
#elif defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
    return (r << 24) | (g << 16) | (b << 8) | a;
#elif defined(__3DS__)
    return C2D_Color32(r, g, b, a);
#endif
    return 0;
}

double Math::parseNumber(const std::string str) {
    // Scratch has whitespace trimming
    while (std::isspace(str[0]) && !str.empty()) {
        str.erase(0, 1);
    }
    while (!str.empty() && std::isspace(str.back())) {
        str.pop_back();
    }

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
        if (base != 0) {
            str = str.substr(2, str.length() - 2);
        }
    }

    for (size_t i = 0; i < str.length(); i++) {
        if (validcharacters.find(str[i]) == std::string::npos) {
            throw std::invalid_argument("");
        }
        if (base == 0) {
            if (str[i] == 'e' && i == str.length() - 1) {
                // implementation differece, "1e" doesn't work in Scratch but works
                // with std::stod()
                throw std::invalid_argument("");
            }
            if (str[i] == 'e' && str.find('.', i + 1) != std::string::npos) {
                // implementation differece, decimal point after e doesn't work in
                // Scratch but works with std::stod()
                throw std::invalid_argument("");
            }
        }
    }

    double conversion;
    std::size_t pos;
    try {
        if (base == 0) {
            conversion = std::stod(str, &pos);
        } else {
            conversion = std::stoull(str, &pos, base);
        }
    } catch (const std::out_of_range &e) {
        if (str[0] == '-') {
            return -std::numeric_limits<double>::infinity();
        } else {
            return std::numeric_limits<double>::infinity();
        }
    } catch (const std::invalid_argument &e) {
        return 0;
    }
    return conversion;
}

bool Math::isNumber(const std::string &str) {
    try {
        parseNumber(str);
        return true;
    } catch (...) {
        return false;
    }
}

double Math::degreesToRadians(double degrees) {
    return degrees * (PI / 180.0);
}

double Math::radiansToDegrees(double radians) {
    return radians * (180.0 / PI);
}

std::string Math::removeDoubleQuotes(std::string_view str) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value.remove_prefix(1);
        value.remove_suffix(1);
    }
    return std::string(value);
}

uint32_t Math::next_pow2(uint32_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}
