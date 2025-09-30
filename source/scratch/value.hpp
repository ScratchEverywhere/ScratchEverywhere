#pragma once
#include "color.hpp"
#include "math.hpp"
#include "os.hpp"
#include <cmath>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <string>

enum class ValueType {
    INTEGER,
    DOUBLE,
    BOOLEAN,
    STRING,
    COLOR
};

class Value {
  private:
    ValueType type;
    union {
        int intValue;
        double doubleValue;
        std::string *stringValue;
        Color colorValue;
    };

  public:
    // constructors
    Value() : type(ValueType::STRING) {
        stringValue = new std::string("");
    }

    explicit Value(int val) : type(ValueType::INTEGER), intValue(val) {}

    explicit Value(double val) : type(ValueType::DOUBLE), doubleValue(val) {}

    explicit Value(Color val) : type(ValueType::COLOR), colorValue(val) {}

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
        case ValueType::COLOR:
            colorValue = other.colorValue;
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
            case ValueType::COLOR:
                colorValue = other.colorValue;
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
    bool isColor() const { return type == ValueType::COLOR; }
    bool isNumeric() const {
        return type == ValueType::INTEGER || type == ValueType::DOUBLE || type == ValueType::BOOLEAN ||
               (type == ValueType::STRING && (*stringValue == "Infinity" || *stringValue == "-Infinity")) ||
               (type == ValueType::STRING && Math::isNumber(*stringValue));
    }

    double asDouble() const {
        switch (type) {
        case ValueType::INTEGER:
            return static_cast<double>(intValue);
        case ValueType::DOUBLE:
            return doubleValue;
        case ValueType::STRING:
            if (*stringValue == "Infinity") return std::numeric_limits<double>::max();
            if (*stringValue == "-Infinity") return -std::numeric_limits<double>::max();
            return Math::isNumber(*stringValue) ? ((*stringValue)[0] == '0' ? ((*stringValue)[1] == 'x' ? std::stoi((*stringValue).substr(2, (*stringValue).size() - 2), 0, 16) : (*stringValue)[1] == 'b' ? std::stoi((*stringValue).substr(2, (*stringValue).size() - 2), 0, 2)
                                                                                                                                                                              : (*stringValue)[1] == 'o'   ? std::stoi((*stringValue).substr(2, (*stringValue).size() - 2), 0, 8)
                                                                                                                                                                                                           : std::stod(*stringValue))
                                                                            : std::stod(*stringValue))
                                                : 0.0; // clang-format really cooked here...
        case ValueType::BOOLEAN:
            return *stringValue == "true" ? 1.0 : 0.0;
        case ValueType::COLOR:
            return static_cast<double>(asInt());
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
            if (*stringValue == "Infinity") return std::numeric_limits<int>::max();
            if (*stringValue == "-Infinity") return -std::numeric_limits<int>::max();
            if (Math::isNumber(*stringValue)) {
                double d;
                if ((*stringValue)[0] == '0') {
                    switch ((*stringValue)[1]) {
                    case 'x':
                        d = std::stoi((*stringValue).substr(2, (*stringValue).size() - 2), 0, 16);
                        break;
                    case 'b':
                        d = std::stoi((*stringValue).substr(2, (*stringValue).size() - 2), 0, 2);
                        break;
                    case 'o':
                        d = std::stoi((*stringValue).substr(2, (*stringValue).size() - 2), 0, 8);
                        break;
                    default:
                        d = std::stod(*stringValue);
                        break;
                    }
                } else d = std::stod(*stringValue);
                return static_cast<int>(std::round(d));
            }
        case ValueType::BOOLEAN:
            return *stringValue == "true" ? 1 : 0;
        case ValueType::COLOR:
            const ColorRGB rgb = HSB2RGB(colorValue);
            return rgb.r * 0x10000 + rgb.g * 0x100 + rgb.b;
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
        case ValueType::COLOR: {
            const ColorRGB rgb = HSB2RGB(colorValue);
            const char hex_chars[] = "0123456789abcdef";
            const unsigned char r = static_cast<unsigned char>(rgb.r);
            const unsigned char g = static_cast<unsigned char>(rgb.g);
            const unsigned char b = static_cast<unsigned char>(rgb.b);
            std::string hex_str = "#";
            hex_str += hex_chars[r >> 4];
            hex_str += hex_chars[r & 0x0F];
            hex_str += hex_chars[g >> 4];
            hex_str += hex_chars[g & 0x0F];
            hex_str += hex_chars[b >> 4];
            hex_str += hex_chars[b & 0x0F];
            return hex_str;
        }
        }
        return "";
    }

    Color asColor() const {
        switch (type) {
        case ValueType::INTEGER:
            return RGB2HSB({static_cast<float>(intValue / 0x10000), static_cast<float>((intValue / 0x100) % 0x100), static_cast<float>(intValue % 0x100)});
        case ValueType::DOUBLE:
            return RGB2HSB({static_cast<float>(doubleValue / 0x10000), static_cast<float>(static_cast<int>(doubleValue / 0x100) % 0x100), static_cast<float>(static_cast<int>(doubleValue) % 0x100)});
        case ValueType::BOOLEAN:
            return {0, 0, static_cast<float>((*stringValue == "true" ? 1 : 0) * 100)}; // IDK what the correct thing actually is here...
        case ValueType::COLOR:
            return colorValue;
        case ValueType::STRING:
            if (!std::regex_match(*stringValue, std::regex("^#[\\dabcdef]{6}$"))) return {0, 0, 0};
            int intValue = std::stoi(stringValue->substr(1), 0, 16);
            return RGB2HSB({static_cast<float>(intValue / 0x10000), static_cast<float>((intValue / 0x100) % 0x100), static_cast<float>(intValue % 0x100)});
        }
        return {0, 0, 0};
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
            case ValueType::COLOR:
                return colorValue == colorValue;
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

            if (strVal == "Infinity" || strVal == "-Infinity") return Value(strVal);

            if (Math::isNumber(strVal)) {
                double numVal;
                try {
                    if (strVal[0] == '0') {
                        switch (strVal[1]) {
                        case 'x':
                            numVal = std::stoi(strVal.substr(2, strVal.size() - 2), 0, 16);
                            break;
                        case 'b':
                            numVal = std::stoi(strVal.substr(2, strVal.size() - 2), 0, 2);
                            break;
                        case 'o':
                            numVal = std::stoi(strVal.substr(2, strVal.size() - 2), 0, 8);
                            break;
                        default:
                            numVal = std::stod(strVal);
                            break;
                        }
                    } else numVal = std::stod(strVal);
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
