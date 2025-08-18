#include "math.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <math.h>
#include <random>
#include <string>
#ifdef __3DS__
#include <citro2d.h>
#endif

int Math::color(int r, int g, int b, int a) {
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    a = std::clamp(a, 0, 255);

#ifdef __3DS__
    return C2D_Color32(r, g, b, a);
#elif defined(SDL_BUILD)
    return (r << 24) |
           (g << 16) |
           (b << 8) |
           a;
#endif
}

bool Math::isNumber(const std::string &str) {
    // i rewrote this function like 5 times vro if ts dont work...
    if (str.empty()) return false;

    size_t start = 0;
    if (str[0] == '-') {                   // Allow negative numbers
        if (str.size() == 1) return false; // just "-" alone is invalid
        start = 1;                         // Skip '-' for digit checking
    }

    return std::count(str.begin() + start, str.end(), '.') <= 1 &&   // At most one decimal point
           std::any_of(str.begin() + start, str.end(), ::isdigit) && // At least one digit
           std::all_of(str.begin() + start, str.end(), [](char c) { return std::isdigit(c) || c == '.'; });
}

double Math::degreesToRadians(double degrees) {
    return degrees * (M_PI / 180.0);
}

double Math::radiansToDegrees(double radians) {
    return radians * (180.0 / M_PI);
}

std::string Math::generateRandomString(int length) {
    std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-=[];',./_+{}|:<>?~`";
    std::string result;

    static std::random_device rd;
    static std::mt19937 generator(rd());
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