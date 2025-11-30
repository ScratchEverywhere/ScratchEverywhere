#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace Math {
constexpr double PI = 3.14159265358979323846;

bool isNumber(const std::string &str);
double parseNumber(const std::string str);

uint32_t color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

double degreesToRadians(double degrees);
double radiansToDegrees(double radians);

std::string removeDoubleQuotes(std::string_view str);

uint32_t next_pow2(uint32_t n);
}; // namespace Math
