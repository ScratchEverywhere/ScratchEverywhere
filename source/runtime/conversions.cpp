#include "conversions.hpp"
#include <cctype>

double Conversions::stringToNumber(std::string_view s) {
    return Math::parseNumber(s).value_or(0);
}

double Conversions::colorToNumber(const Color &c) {
    const ColorRGBA rgb = CSBT2RGBA(c);
    return rgb.r * 0x10000 + rgb.g * 0x100 + rgb.b;
}

std::string Conversions::colorToString(const Color &c) {
    const ColorRGBA rgb = CSBT2RGBA(c);
    const char hex_chars[] = "0123456789abcdef";
    const unsigned char r = static_cast<unsigned char>(rgb.r);
    const unsigned char g = static_cast<unsigned char>(rgb.g);
    const unsigned char b = static_cast<unsigned char>(rgb.b);
    std::string hex_str = "#";
    hex_str += hex_chars[r >> 4];
    hex_str += hex_chars[r & 0x0F];
    hex_str += hex_chars[g >> 4];
    hex_str += hex_chars[g & 0x0F];
    hex_str += hex_chars[b >> 4];
    hex_str += hex_chars[b & 0x0F];
    return hex_str;
}

bool Conversions::stringToBool(std::string_view sv) {
    if (sv.empty() || sv == "0") return false;
    if (sv.size() == 5) {
        char f0 = std::tolower(static_cast<unsigned char>(sv[0]));
        char f1 = std::tolower(static_cast<unsigned char>(sv[1]));
        char f2 = std::tolower(static_cast<unsigned char>(sv[2]));
        char f3 = std::tolower(static_cast<unsigned char>(sv[3]));
        char f4 = std::tolower(static_cast<unsigned char>(sv[4]));
        if (f0 == 'f' && f1 == 'a' && f2 == 'l' && f3 == 's' && f4 == 'e') {
            return false;
        }
    }
    return true;
}

bool Conversions::colorToBool(const Color &c) {
    const ColorRGBA rgb = CSBT2RGBA(c);
    return rgb.r != 0 || rgb.g != 0 || rgb.b != 0 || rgb.a != 0;
}

namespace {
inline int parseHexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline bool parseHexByte(char c1, char c2, float &out) {
    int d1 = parseHexDigit(c1);
    int d2 = parseHexDigit(c2);
    if (d1 < 0 || d2 < 0) return false;
    out = static_cast<float>((d1 << 4) | d2);
    return true;
}

enum class HexResult { NotHex,
                       Valid,
                       Malformed };

HexResult parseHexColor(std::string_view sv, ColorRGBA &out) {
    if (sv.empty() || sv[0] != '#') return HexResult::NotHex;

    if (sv.size() == 4) {
        const int r = parseHexDigit(sv[1]);
        const int g = parseHexDigit(sv[2]);
        const int b = parseHexDigit(sv[3]);
        if (r < 0 || g < 0 || b < 0) return HexResult::Malformed;
        out = {static_cast<float>((r << 4) | r), static_cast<float>((g << 4) | g), static_cast<float>((b << 4) | b), 255.0f};
        return HexResult::Valid;
    }
    if (sv.size() == 7) {
        float r, g, b;
        if (!parseHexByte(sv[1], sv[2], r) || !parseHexByte(sv[3], sv[4], g) || !parseHexByte(sv[5], sv[6], b)) return HexResult::Malformed;
        out = {r, g, b, 255.0f};
        return HexResult::Valid;
    }
    return HexResult::Malformed;
}
} // namespace

ColorRGBA Conversions::numberToRGB(double RGBA) {
    return {static_cast<float>(static_cast<unsigned int>(RGBA / 0x10000) % 0x100),
            static_cast<float>(static_cast<unsigned int>(RGBA / 0x100) % 0x100),
            static_cast<float>(static_cast<unsigned int>(RGBA) % 0x100),
            static_cast<float>(static_cast<unsigned int>(RGBA / 0x1000000) % 0x100)};
}

Color Conversions::numberToColor(double d) {
    return RGBA2CSBO(numberToRGB(d));
}

ColorRGBA Conversions::stringToRGB(std::string_view sv) {
    ColorRGBA hex;
    switch (parseHexColor(sv, hex)) {
    case HexResult::Valid:
        return hex;
    case HexResult::Malformed:
        return {0, 0, 0, 255.0f};
    case HexResult::NotHex:
        break;
    }
    return numberToRGB(stringToNumber(sv));
}

Color Conversions::stringToColor(std::string_view sv) {
    ColorRGBA hex;
    switch (parseHexColor(sv, hex)) {
    case HexResult::Valid:
        return RGBA2CSBO(hex);
    case HexResult::Malformed:
        return {0, 0, 0, 0};
    case HexResult::NotHex:
        break;
    }
    return numberToColor(stringToNumber(sv));
}
