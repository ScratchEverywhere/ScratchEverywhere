#include "value.hpp"
#include "math.hpp"
#include "os.hpp"
#include <regex>

Value::Value(int val) : value(val) {}

Value::Value(double val) : value(val) {}

Value::Value(std::string val) : value(std::move(val)) {}

Value::Value(bool val) : value(val) {}

bool Value::isInteger() const {
    return std::holds_alternative<int>(value);
}

bool Value::isDouble() const {
    return std::holds_alternative<double>(value);
}

bool Value::isString() const {
    return std::holds_alternative<std::string>(value);
}

bool Value::isBoolean() const {
    return std::holds_alternative<bool>(value);
}

bool Value::isNumeric() const {
    if (isInteger() || isDouble() || isBoolean()) {
        return true;
    } else if (isString()) {
        auto &strValue = std::get<std::string>(value);
        return Math::isNumber(strValue);
    }

    return false;
}

bool Value::isColor() const {
    return std::holds_alternative<Color>(value);
}

bool Value::isNaN() const {
    return std::holds_alternative<double>(value) && std::isnan(std::get<double>(value));
}

double Value::asDouble() const {
    if (isDouble()) {
        if (isNaN()) return 0.0;
        return std::get<double>(value);
    } else if (isString()) {
        auto &strValue = std::get<std::string>(value);
        try {
            return Math::parseNumber(strValue);
        } catch (...) {
            return 0.0;
        }
    } else if (isColor() || isInteger() || isBoolean()) {
        return static_cast<double>(asInt());
    }

    return 0.0;
}

int Value::asInt() const {
    if (isInteger()) {
        return std::get<int>(value);
    } else if (isDouble()) {
        auto doubleValue = std::get<double>(value);
        return static_cast<int>(std::round(doubleValue));
    } else if (isString()) {
        auto &strValue = std::get<std::string>(value);

        if (strValue == "Infinity") {
            return std::numeric_limits<int>::infinity();
        }

        if (strValue == "-Infinity") {
            return -std::numeric_limits<int>::infinity();
        }

        if (Math::isNumber(strValue)) {
            return static_cast<int>(std::round(Math::parseNumber(strValue)));
        }
    } else if (isBoolean()) {
        return std::get<bool>(value) ? 1 : 0;
    } else if (isColor()) {
        const ColorRGB rgb = HSB2RGB(std::get<Color>(value));
        return rgb.r * 0x10000 + rgb.g * 0x100 + rgb.b;
    }

    return 0;
}

std::string Value::asString() const {
    if (isInteger()) {
        return std::to_string(std::get<int>(value));
    } else if (isDouble()) {
        double doubleValue = std::get<double>(value);
        // handle whole numbers too, because scratch i guess
        if (std::isnan(doubleValue)) return "NaN";
        if (std::isinf(doubleValue)) return std::signbit(doubleValue) ? "-Infinity" : "Infinity";
        if (std::floor(doubleValue) == doubleValue) return std::to_string(static_cast<int>(doubleValue));
        return std::to_string(doubleValue);
    } else if (isString()) {
        return std::get<std::string>(value);
    } else if (isBoolean()) {
        return std::get<bool>(value) ? "true" : "false";
    } else if (isColor()) {
        const ColorRGB rgb = HSB2RGB(std::get<Color>(value));
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

    return "";
}

Color Value::asColor() const {
    if (isInteger()) {
        const int &intValue = std::get<int>(value);
        return RGB2HSB({static_cast<float>(intValue / 0x10000), static_cast<float>((intValue / 0x100) % 0x100), static_cast<float>(intValue % 0x100)});
    }
    if (isDouble()) {
        const double &doubleValue = std::get<double>(value);
        return RGB2HSB({static_cast<float>(doubleValue / 0x10000), static_cast<float>(static_cast<int>(doubleValue / 0x100) % 0x100), static_cast<float>(static_cast<int>(doubleValue) % 0x100)});
    }
    if (isColor()) return std::get<Color>(value);
    if (isString()) {
        const std::string &stringValue = std::get<std::string>(value);
        if (!std::regex_match(stringValue, std::regex("^#[\\dabcdef]{6}$"))) return {0, 0, 0};
        const int intValue = std::stoi(stringValue.substr(1), 0, 16);
        return RGB2HSB({static_cast<float>(intValue / 0x10000), static_cast<float>((intValue / 0x100) % 0x100), static_cast<float>(intValue % 0x100)});
    }

    return {0, 0, 0};
}

Value Value::operator+(const Value &other) const {
    Value a = *this;
    Value b = other;
    if (!a.isNumeric()) a = Value(0);
    if (!b.isNumeric()) b = Value(0);

    if (a.isInteger() && b.isInteger()) {
        return Value(a.asInt() + b.asInt());
    }
    return Value(a.asDouble() + b.asDouble());
}

Value Value::operator-(const Value &other) const {
    Value a = *this;
    Value b = other;
    if (!a.isNumeric()) a = Value(0);
    if (!b.isNumeric()) b = Value(0);

    if (a.isInteger() && b.isInteger()) {
        return Value(a.asInt() - b.asInt());
    }
    return Value(a.asDouble() - b.asDouble());
}

Value Value::operator*(const Value &other) const {
    Value a = *this;
    Value b = other;
    if (!a.isNumeric()) a = Value(0);
    if (!b.isNumeric()) b = Value(0);

    if (a.isInteger() && b.isInteger()) {
        return Value(a.asInt() * b.asInt());
    }
    return Value(a.asDouble() * b.asDouble());
}

Value Value::operator/(const Value &other) const {
    Value a = *this;
    Value b = other;
    if (!a.isNumeric()) a = Value(0);
    if (!b.isNumeric()) b = Value(0);

    return Value(a.asDouble() / b.asDouble());
}

bool Value::operator==(const Value &other) const {
    if (isNumeric() && other.isNumeric() && !isNaN() && !other.isNaN()) {
        return asDouble() == other.asDouble();
    }

    std::string string1 = asString();
    std::string string2 = other.asString();
    std::transform(string1.begin(), string1.end(), string1.begin(), ::tolower);
    std::transform(string2.begin(), string2.end(), string2.begin(), ::tolower);
    return string1 == string2;
}

bool Value::operator<(const Value &other) const {
    if (isNumeric() && other.isNumeric() && !isNaN() && !other.isNaN()) {
        return asDouble() < other.asDouble();
    }

    std::string string1 = asString();
    std::string string2 = other.asString();
    std::transform(string1.begin(), string1.end(), string1.begin(), ::tolower);
    std::transform(string2.begin(), string2.end(), string2.begin(), ::tolower);
    return string1 < string2;
}

bool Value::operator>(const Value &other) const {
    if (isNumeric() && other.isNumeric() && !isNaN() && !other.isNaN()) {
        return asDouble() > other.asDouble();
    }

    std::string string1 = asString();
    std::string string2 = other.asString();
    std::transform(string1.begin(), string1.end(), string1.begin(), ::tolower);
    std::transform(string2.begin(), string2.end(), string2.begin(), ::tolower);
    return string1 > string2;
}

bool Value::isScratchInt() {
  if (isDouble()) {
    if (std::isnan(asDouble())) return true;
    try {
      return std::stoi(asString()) == asDouble();
    } catch (...) {
      return false;
    }
  }
  if (isBoolean()) return true;
  if (isString()) return asString().find('.') == std::string::npos;
  return false;
}

Value Value::fromJson(const nlohmann::json &jsonVal) {
    if (jsonVal.is_number_integer()) return Value(jsonVal.get<int>());
    if (jsonVal.is_number_float()) return Value(jsonVal.get<double>());
    if (jsonVal.is_string()) return Value(jsonVal.get<std::string>());
    if (jsonVal.is_boolean()) return Value(jsonVal.get<bool>());
    if (jsonVal.is_array()) {
        if (jsonVal.size() > 1) return fromJson(jsonVal[1]);
        return Value(0);
    }
    return Value(0);
}
