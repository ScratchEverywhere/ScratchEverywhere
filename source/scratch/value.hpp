#pragma once
#include "math.hpp"
#include <cmath>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

enum class ValueType {
    INTEGER,
    DOUBLE,
    BOOLEAN,
    STRING
};

class Value {
  private:
    ValueType type;
    union {
        int intValue;
        double doubleValue;
        std::string *stringValue;
    };

  public:
    // constructors
    Value() : type(ValueType::STRING) {
        stringValue = new std::string("");
    }

    explicit Value(int val) : type(ValueType::INTEGER), intValue(val) {}

    explicit Value(double val) : type(ValueType::DOUBLE), doubleValue(val) {}

    explicit Value(bool val) : type(ValueType::BOOLEAN) {
        stringValue = new std::string(val ? "true" : "false");
    }

    explicit Value(const std::string &val) : type(ValueType::STRING) {
        stringValue = new std::string(val);
    }
    // copy operator
    Value(const Value &other) : type(other.type) {
        switch (type) {
        case ValueType::INTEGER:
            intValue = other.intValue;
            break;
        case ValueType::DOUBLE:
            doubleValue = other.doubleValue;
            break;
        case ValueType::STRING:
            stringValue = new std::string(*other.stringValue);
            break;
        case ValueType::BOOLEAN:
            stringValue = new std::string(*other.stringValue);
        }
    }
    // Assignment operator
    Value &operator=(const Value &other) {
        if (this != &other) {
            if (type == ValueType::STRING || type == ValueType::BOOLEAN) {
                delete stringValue;
            }
            // copy new value
            type = other.type;
            switch (type) {
            case ValueType::INTEGER:
                intValue = other.intValue;
                break;
            case ValueType::DOUBLE:
                doubleValue = other.doubleValue;
                break;
            case ValueType::STRING:
                stringValue = new std::string(*other.stringValue);
                break;
            case ValueType::BOOLEAN:
                stringValue = new std::string(*other.stringValue);
                break;
            }
        }
        return *this;
    }
    // destructor
    ~Value() {
        if (type == ValueType::STRING || type == ValueType::BOOLEAN) {
            delete stringValue;
        }
    }
    // type checks
    bool isInteger() const { return type == ValueType::INTEGER; }
    bool isDouble() const { return type == ValueType::DOUBLE; }
    bool isString() const { return type == ValueType::STRING; }
    bool isBoolean() const { return type == ValueType::BOOLEAN; }
    bool isNumeric() const {
        return type == ValueType::INTEGER || type == ValueType::DOUBLE || type == ValueType::BOOLEAN ||
               (type == ValueType::STRING && Math::isNumber(*stringValue));
    }

    double asDouble() const {
        switch (type) {
        case ValueType::INTEGER:
            return static_cast<double>(intValue);
        case ValueType::DOUBLE:
            return doubleValue;
        case ValueType::STRING:
            return Math::isNumber(*stringValue) ? std::stod(*stringValue) : 0.0;
        case ValueType::BOOLEAN:
            return *stringValue == "true" ? 1.0 : 0.0;
        }
        return 0.0;
    }

    int asInt() const {
        switch (type) {
        case ValueType::INTEGER:
            return intValue;
        case ValueType::DOUBLE:
            return static_cast<int>(std::round(doubleValue));
        case ValueType::STRING:
            if (Math::isNumber(*stringValue)) {
                double d = std::stod(*stringValue);
                return static_cast<int>(std::round(d));
            }
        case ValueType::BOOLEAN:
            return *stringValue == "true" ? 1 : 0;
        }
        return 0;
    }

    std::string asString() const {
        switch (type) {
        case ValueType::INTEGER:
            return std::to_string(intValue);
        case ValueType::DOUBLE: {
            // handle whole numbers too, because scratch i guess
            if (std::floor(doubleValue) == doubleValue) {
                return std::to_string(static_cast<int>(doubleValue));
            }
            return std::to_string(doubleValue);
        }
        case ValueType::STRING:
            return *stringValue;
        case ValueType::BOOLEAN:
            return *stringValue;
        }
        return "";
    }

    // Arithmetic operations
    Value operator+(const Value &other) const {
        Value a = *this;
        Value b = other;
        if (!a.isNumeric()) a = Value(0);
        if (!b.isNumeric()) b = Value(0);

        if (a.type == ValueType::INTEGER && b.type == ValueType::INTEGER) {
            return Value(a.intValue + b.intValue);
        }
        return Value(a.asDouble() + b.asDouble());
    }

    Value operator-(const Value &other) const {
        Value a = *this;
        Value b = other;
        if (!a.isNumeric()) a = Value(0);
        if (!b.isNumeric()) b = Value(0);

        if (a.type == ValueType::INTEGER && b.type == ValueType::INTEGER) {
            return Value(a.intValue - b.intValue);
        }
        return Value(a.asDouble() - b.asDouble());
    }

    Value operator*(const Value &other) const {
        Value a = *this;
        Value b = other;
        if (!a.isNumeric()) a = Value(0);
        if (!b.isNumeric()) b = Value(0);

        if (a.type == ValueType::INTEGER && b.type == ValueType::INTEGER) {
            return Value(a.intValue * b.intValue);
        }
        return Value(a.asDouble() * b.asDouble());
    }

    Value operator/(const Value &other) const {
        Value a = *this;
        Value b = other;
        if (!a.isNumeric()) a = Value(0);
        if (!b.isNumeric()) b = Value(0);

        double bVal = b.asDouble();
        if (bVal == 0.0) return Value(0); // Division by zero
        return Value(a.asDouble() / bVal);
    }

    // Comparison operators
    bool operator==(const Value &other) const {
        if (type == other.type) {
            switch (type) {
            case ValueType::INTEGER:
                return intValue == other.intValue;
            case ValueType::DOUBLE:
                return doubleValue == other.doubleValue;
            case ValueType::STRING:
                return *stringValue == *other.stringValue;
            case ValueType::BOOLEAN:
                return *stringValue == *other.stringValue;
            }
        }
        // Different types - compare as strings (Scratch behavior)
        return asString() == other.asString();
    }

    bool operator<(const Value &other) const {
        if (isNumeric() && other.isNumeric()) {
            return asDouble() < other.asDouble();
        }
        return asString() < other.asString();
    }

    bool operator>(const Value &other) const {
        if (isNumeric() && other.isNumeric()) {
            return asDouble() > other.asDouble();
        }
        return asString() > other.asString();
    }

    static Value fromJson(const nlohmann::json &jsonVal) {
        if (jsonVal.is_null()) return Value();

        if (jsonVal.is_number_integer()) {
            return Value(jsonVal.get<int>());
        } else if (jsonVal.is_number_float()) {
            return Value(jsonVal.get<double>());
        } else if (jsonVal.is_string()) {
            std::string strVal = jsonVal.get<std::string>();

            if (Math::isNumber(strVal)) {
                double numVal;
                try {
                    numVal = std::stod(strVal);
                } catch (const std::invalid_argument &e) {
                    Log::logError("Invalid number format: " + strVal);
                    return Value(0);
                } catch (const std::out_of_range &e) {
                    Log::logError("Number out of range: " + strVal);
                    return Value(0);
                }

                if (std::floor(numVal) == numVal) {
                    return Value(static_cast<int>(numVal));
                }
                return Value(numVal);
            }
            return Value(strVal);
        } else if (jsonVal.is_boolean()) {
            return Value(Math::removeQuotations(jsonVal.dump()));
        } else if (jsonVal.is_array()) {
            if (jsonVal.size() > 1) {
                return fromJson(jsonVal[1]);
            }
            return Value(0);
        }
        return Value(0);
    }

    ValueType
    getType() const {
        return type;
    }
};