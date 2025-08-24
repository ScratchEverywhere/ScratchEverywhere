#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class Math {
  public:
    static bool isNumber(const std::string &str) {
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

    static double degreesToRadians(double degrees) {
        return degrees * (M_PI / 180.0);
    }
};