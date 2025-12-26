#include "value.hpp"
#include "math.hpp"
#include <os.hpp>
#include <regex>
#include <array>
#include <sstream>
#include <charconv>

Value::Value(int val) : value(val) {}

Value::Value(double val) : value(val) {}

Value::Value(std::string val) : value(std::move(val)) {}

Value::Value(bool val) : value(val) {}

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
        const ColorRGBA rgb = CSBT2RGBA(std::get<Color>(value));
        return rgb.r * 0x10000 + rgb.g * 0x100 + rgb.b;
    }

    return 0;
}

std::string Value::asString() const {
    if (isInteger()) {
        return std::to_string(std::get<int>(value));
    } else if (isDouble()) {
        double doubleValue = std::get<double>(value);
        if (std::isnan(doubleValue)) return "NaN";
        if (std::isinf(doubleValue)) return std::signbit(doubleValue) ? "-Infinity" : "Infinity";
        /*
        std::array<char, 24> buffer;
        std::to_chars_result result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), doubleValue);
        *result.ptr = '\0';
        return buffer.data();
        */
        std::ostringstream oss;
        oss << std::setprecision(17) << doubleValue;
        return oss.str();
    } else if (isString()) {
        return std::get<std::string>(value);
    } else if (isBoolean()) {
        return std::get<bool>(value) ? "true" : "false";
    } else if (isColor()) {
        const ColorRGBA rgb = CSBT2RGBA(std::get<Color>(value));
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

bool Value::asBoolean() const {
    if (isBoolean()) {
        return std::get<bool>(value);
    }
    if (isInteger()) {
        return std::get<int>(value) != 0;
    }
    if (isDouble()) {
        return std::get<double>(value) != 0.0 && !isNaN();
    }
    if (isString()) {
        std::string strValue = std::get<std::string>(value);
        std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::tolower);
        return strValue != "" && strValue != "0" && strValue != "false";
    }
    if (isColor()) {
        const ColorRGBA rgb = CSBT2RGBA(std::get<Color>(value));
        return rgb.r != 0 || rgb.g != 0 || rgb.b != 0 || rgb.a != 0;
    }
    return false;
}

Color Value::asColor() const {
    if (isString()) {
        std::string stringValue = asString();
        if (stringValue[0] == '#') {
            std::string r, g, b;
            if (std::regex_match(stringValue, std::regex("^#[\\dA-Fa-f]{3}$"))) {
                stringValue = "#" + std::string(2, stringValue[1]) + std::string(2, stringValue[2]) + std::string(2, stringValue[3]);
            }
            if (std::regex_match(stringValue, std::regex("^#[\\dA-Fa-f]{6}$"))) {
                r = stringValue.substr(1, 2);
                g = stringValue.substr(3, 2);
                b = stringValue.substr(5, 2);
                return RGBA2CSBO({static_cast<float>(std::stoi(r, 0, 16)), static_cast<float>(std::stoi(g, 0, 16)), static_cast<float>(std::stoi(b, 0, 16)), 255});
            } else return {0, 0, 0, 0};
        }
    }
    const double RGBA = asDouble();
    return RGBA2CSBO({static_cast<float>(static_cast<unsigned int>(RGBA / 0x10000) % 0x100), static_cast<float>(static_cast<unsigned int>(RGBA / 0x100) % 0x100), static_cast<float>(static_cast<unsigned int>(RGBA) % 0x100), static_cast<float>(static_cast<unsigned int>(RGBA / 0x1000000) % 0x100)});
}

Value Value::operator+(const Value &other) const {
    Value a = *this;
    Value b = other;

    return Value(a.asDouble() + b.asDouble());
}

Value Value::operator-(const Value &other) const {
    Value a = *this;
    Value b = other;

    return Value(a.asDouble() - b.asDouble());
}

Value Value::operator*(const Value &other) const {
    Value a = *this;
    Value b = other;

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
    std::string string1 = asString();
    std::string string2 = other.asString();

    if (!std::all_of(string1.begin(), string1.end(), [](unsigned char c) { return (std::isspace(c) && c != '\t'); }) &&
        !std::all_of(string2.begin(), string2.end(), [](unsigned char c) { return (std::isspace(c) && c != '\t'); })) {
        if (isNumeric() && other.isNumeric() && !isNaN() && !other.isNaN()) {
            return asDouble() == other.asDouble();
        }
    }

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
            return std::fabs(asDouble()) < 1e21 && std::floor(asDouble()) == asDouble();
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
