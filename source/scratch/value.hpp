#pragma once
#include "math.hpp"
#include "os.hpp"
#include <cmath>
#include <nlohmann/json.hpp>
#include <string>
#include <variant>

class Value {
  private:
    std::variant<int, double, std::string, bool> value;

  public:
    // constructors
    Value() : value(std::string()) {}

    explicit Value(int val);
    explicit Value(double val);
    explicit Value(std::string val);
    explicit Value(bool val);

    // type checks
    bool isInteger() const;
    bool isDouble() const;
    bool isString() const;
    bool isBoolean() const;
    bool isNumeric() const;

    double asDouble() const;

    int asInt() const;

    std::string asString() const;

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
