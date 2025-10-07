#pragma once
#include "color.hpp"
#include "math.hpp"
#include "os.hpp"
#include <cmath>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <string>

#include <variant>

class Value {
  private:
    std::variant<int, double, std::string, bool, Color> value;

  public:
    // constructors
    Value() : value(std::string()) {}

    explicit Value(int val);
    explicit Value(double val);
    explicit Value(std::string val);
    explicit Value(bool val);
    explicit Value(Color val);

    // type checks
    bool isInteger() const;
    bool isDouble() const;
    bool isString() const;
    bool isBoolean() const;
    bool isNumeric() const;
    bool isColor() const;

    double asDouble() const;

    int asInt() const;

    std::string asString() const;

    Color asColor() const;

    // Arithmetic operations
    Value operator+(const Value &other) const;

    Value operator-(const Value &other) const;

    Value operator*(const Value &other) const;

    Value operator/(const Value &other) const;

    // Comparison operators
    bool operator==(const Value &other) const;

    bool operator<(const Value &other) const;

    bool operator>(const Value &other) const;

    static Value fromJson(const nlohmann::json &jsonVal);
};
