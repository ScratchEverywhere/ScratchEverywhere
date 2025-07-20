#pragma once
#include <string>
#include <cmath>
#include <algorithm>
#include <random>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Math{
    bool isNumber(const std::string& str);

    double degreesToRadians(double degrees);
    
    std::string generateRandomString(int length);

    std::string removeQuotations(std::string value);
};