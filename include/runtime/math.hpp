#pragma once
#include <cstdint>
#include <memory>
#include <nonstd/expected.hpp>
#include <ryu/d2s.h>
#include <se_export.hpp>
#include <string>
#ifndef M_PI
#define M_PI 3.1415926535897932
#endif

namespace Math {

SE_EXPORT bool isNumber(const std::string &str);
SE_EXPORT nonstd::expected<double, std::string> parseNumber(std::string_view str);

SE_EXPORT size_t utf16Length(std::string_view str);
SE_EXPORT std::string utf16CharAt(std::string_view str, size_t utf16Index);
SE_EXPORT size_t utf16Length(const std::shared_ptr<const std::string> &str);
SE_EXPORT std::string utf16CharAt(const std::shared_ptr<const std::string> &str, size_t utf16Index);

SE_EXPORT std::string toLowerCaseJs(std::string_view str);

SE_EXPORT bool caseInsensitiveEqual(std::string_view a, std::string_view b);
SE_EXPORT bool caseInsensitiveLess(std::string_view a, std::string_view b);

SE_EXPORT std::string toString(double number);

SE_EXPORT int color(int r, int g, int b, int a);

SE_EXPORT double degreesToRadians(double degrees);

SE_EXPORT double radiansToDegrees(double radians);

SE_EXPORT int16_t radiansToAngle16(float radians);

SE_EXPORT std::string generateRandomString(int length);

SE_EXPORT std::string removeQuotations(std::string value);

SE_EXPORT const uint32_t next_pow2(uint32_t n);
}; // namespace Math
