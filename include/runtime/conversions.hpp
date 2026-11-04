#pragma once
#include "color.hpp"
#include "compiler_hints.hpp"
#include "math.hpp"
#include <cmath>
#include <se_export.hpp>
#include <string>
#include <string_view>

class SE_EXPORT Conversions {
  public:
    static SE_FORCEINLINE double numberToNumber(double d) { return std::isnan(d) ? 0.0 : d; }
    static SE_FORCEINLINE double boolToNumber(bool b) { return b ? 1.0 : 0.0; }
    static double stringToNumber(std::string_view s);
    static double colorToNumber(const Color &c);

    static std::string numberToString(double d) { return Math::toString(d); }
    static SE_FORCEINLINE std::string boolToString(bool b) { return b ? "true" : "false"; }
    static std::string colorToString(const Color &c);

    static SE_FORCEINLINE bool numberToBool(double d) { return d != 0.0 && !std::isnan(d); }
    static bool stringToBool(std::string_view s);
    static bool colorToBool(const Color &c);

    static Color stringToColor(std::string_view s);
    static Color numberToColor(double d);

    static ColorRGBA stringToRGB(std::string_view s);
    static ColorRGBA numberToRGB(double d);
};
