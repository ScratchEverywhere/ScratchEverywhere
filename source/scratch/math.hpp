#pragma once
#include <string>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Math {
bool isNumber(const std::string &str);
double parseNumber(const std::string &str);

int color(int r, int g, int b, int a);

double degreesToRadians(double degrees);

double radiansToDegrees(double radians);

std::string generateRandomString(int length);

std::string removeQuotations(std::string value);
}; // namespace Math
