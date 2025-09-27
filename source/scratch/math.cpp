#include "math.hpp"
#include <algorithm>
#include <boost/regex.hpp>
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
    return boost::regex_match(str, boost::regex("^((0x[\\da-f]+)|(0b[01]+)|(0o[0-7]+)|([+-]?((\\d+(\\.\\d+)?)|((\\d+)?\\.\\d+))(e[+-]?\\d+)?))$", boost::regex::icase)); // I hope I never need to touch this again x2 (it was rewritten, to handle more edge cases).
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
