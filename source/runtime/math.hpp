#pragma once
#include <cstdint>
#include <ryu/d2s.h>
#include <string>
#ifndef M_PI
#define M_PI 3.1415926535897932
#endif

namespace Math {

bool isNumber(const std::string &str);
double parseNumber(std::string str);

std::string toString(double number);

int color(int r, int g, int b, int a);

double degreesToRadians(double degrees);

double radiansToDegrees(double radians);

int16_t radiansToAngle16(float radians);

std::string generateRandomString(int length);

std::string removeQuotations(std::string value);

const uint32_t next_pow2(uint32_t n);
}; // namespace Math
