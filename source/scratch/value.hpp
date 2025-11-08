#pragma once
#include "color.hpp"
#include "math.hpp"
#include <nlohmann/json.hpp>
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
    inline bool isInteger() const {
        return std::holds_alternative<int>(value);
    }
    inline bool isDouble() const {
        return std::holds_alternative<double>(value);
    }
    inline bool isString() const {
        return std::holds_alternative<std::string>(value);
    }
    inline bool isBoolean() const {
        return std::holds_alternative<bool>(value);
    }
    inline bool isColor() const {
        return std::holds_alternative<Color>(value);
    }
    inline bool isNumeric() const {
        if (isInteger() || isDouble() || isBoolean()) {
            return true;
        } else if (isString()) {
            auto &strValue = std::get<std::string>(value);
            return Math::isNumber(strValue);
        }

        return false;
    }

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

    // Used exclusively by the random block
    bool isScratchInt();

    static Value fromJson(const nlohmann::json &jsonVal);
};
