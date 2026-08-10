#include "value.hpp"
#include "math.hpp"
#include "nlohmann/json.hpp"

#include <cstdint>
#include <string>
#include <string_view>

std::vector<StringEntry> StringPool::pool;
std::unordered_map<std::string_view, uint32_t> StringPool::lookup;
std::vector<uint32_t> StringPool::freeSlots;

bool Value::isNumeric() const {
    if (isDouble() || isBoolean()) {
        return true;
    } else if (isString()) {
        return Math::isNumber(StringPool::get(stringId));
    }
    return false;
}

double Value::asDouble() const {
    switch (type) {
    case ValueType::NUMBER:
        return isNaN() ? 0 : numberValue;
    case ValueType::BOOLEAN:
        return boolValue ? 1 : 0;
    case ValueType::STRING_ID: {
        return Math::parseNumber(StringPool::get(stringId)).value_or(0);
    }
    case ValueType::COLOR: {
        const ColorRGBA rgb = CSBT2RGBA(colorValue);
        return rgb.r * 0x10000 + rgb.g * 0x100 + rgb.b;
    }
    default:
        return 0.0;
    }
}

std::string Value::asString() const {
    switch (type) {
    case ValueType::NUMBER:
        return Math::toString(numberValue);

    case ValueType::STRING_ID:
        return StringPool::get(stringId);

    case ValueType::BOOLEAN:
        return boolValue ? "true" : "false";

    case ValueType::UNDEFINED:
        return "undefined";

    case ValueType::COLOR:
        const ColorRGBA rgb = CSBT2RGBA(colorValue);
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
    switch (type) {
    case ValueType::BOOLEAN:
        return boolValue;
    case ValueType::NUMBER:
        return numberValue != 0.0 && !isNaN();
    case ValueType::UNDEFINED:
        return false;
    case ValueType::COLOR: {
        const ColorRGBA rgb = CSBT2RGBA(colorValue);
        return rgb.r != 0 || rgb.g != 0 || rgb.b != 0 || rgb.a != 0;
    }
    case ValueType::STRING_ID:
        std::string strValue = StringPool::get(stringId);
        std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::tolower);
        return strValue != "" && strValue != "0" && strValue != "false";
    }
    return false;
}

Color Value::asColor() const {
    if (isString()) {
        const std::string str = StringPool::get(stringId);
        if (str.empty() || str[0] != '#') return Color();

        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        uint8_t r, g, b;

        if (str.length() == 4) { // #RGB
            int r1 = hexVal(str[1]), g1 = hexVal(str[2]), b1 = hexVal(str[3]);
            if (r1 < 0 || g1 < 0 || b1 < 0) return {0, 0, 0, 0};
            r = r1 * 17;
            g = g1 * 17;
            b = b1 * 17;
            return RGBA2CSBO({static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), 255.0f});
        } else if (str.length() == 7) { // #RRGGBB
            int r1 = hexVal(str[1]), r2 = hexVal(str[2]);
            int g1 = hexVal(str[3]), g2 = hexVal(str[4]);
            int b1 = hexVal(str[5]), b2 = hexVal(str[6]);
            if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) return {0, 0, 0, 0};
            r = (r1 << 4) | r2;
            g = (g1 << 4) | g2;
            b = (b1 << 4) | b2;
            return RGBA2CSBO({static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), 255.0f});
        }
    }

    return RGBA2CSBO({static_cast<float>(static_cast<unsigned int>(numberValue / 0x10000) % 0x100), static_cast<float>(static_cast<unsigned int>(numberValue / 0x100) % 0x100), static_cast<float>(static_cast<unsigned int>(numberValue) % 0x100), static_cast<float>(static_cast<unsigned int>(numberValue / 0x1000000) % 0x100)});
}

Value Value::operator+(const Value &other) const {
    return Value(this->asDouble() + other.asDouble());
}
Value Value::operator-(const Value &other) const {
    return Value(this->asDouble() - other.asDouble());
}

Value Value::operator*(const Value &other) const {
    return Value(this->asDouble() * other.asDouble());
}

Value Value::operator/(const Value &other) const {
    return Value(this->asDouble() / other.asDouble());
}

Value &Value::operator=(const Value &other) {
    if (this == &other) return *this;

    if (other.type == ValueType::STRING_ID) {
        StringPool::addRef(other.stringId);
    }

    if (this->type == ValueType::STRING_ID) {
        StringPool::release(this->stringId);
    }

    this->type = other.type;
    this->flags = other.flags;
    this->numberValue = other.numberValue;
    this->stringId = other.stringId;

    return *this;
}

bool Value::equalsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.length() != b.length()) return false;

    for (size_t i = 0; i < a.length(); ++i) {
        unsigned char c1 = static_cast<unsigned char>(a[i]);
        unsigned char c2 = static_cast<unsigned char>(b[i]);
        if (c1 != c2 && std::tolower(c1) != std::tolower(c2)) {
            return false;
        }
    }
    return true;
}

bool Value::lessThanIgnoreCase(std::string_view a, std::string_view b) {
    size_t minLen = std::min(a.length(), b.length());
    for (size_t i = 0; i < minLen; ++i) {
        unsigned char c1 = std::tolower(static_cast<unsigned char>(a[i]));
        unsigned char c2 = std::tolower(static_cast<unsigned char>(b[i]));
        if (c1 != c2) return c1 < c2;
    }
    return a.length() < b.length();
}

bool Value::operator==(const Value &other) const {
    if (this->isNumeric() && other.isNumeric()) {
        if (this->isNaN() || other.isNaN()) return false;
        return this->asDouble() == other.asDouble();
    }

    if (this->isBoolean() && other.isBoolean()) {
        return this->asBoolean() == other.asBoolean();
    }

    if (this->isString() && other.isString()) {
        if (this->stringId == other.stringId) return true;

        std::string_view s1 = StringPool::get(this->stringId);
        std::string_view s2 = StringPool::get(other.stringId);
        return equalsIgnoreCase(s1, s2);
    }

    std::string s1 = this->asString();
    std::string s2 = other.asString();
    return equalsIgnoreCase(s1, s2);
}

bool Value::operator<(const Value &other) const {
    if (isNumeric() && other.isNumeric() && !isNaN() && !other.isNaN()) {
        return asDouble() < other.asDouble();
    }
    return lessThanIgnoreCase(asString(), other.asString());
}

bool Value::operator>(const Value &other) const {
    if (isNumeric() && other.isNumeric() && !isNaN() && !other.isNaN()) {
        return asDouble() > other.asDouble();
    }

    return lessThanIgnoreCase(other.asString(), asString());
}

bool Value::isScratchInt() {
    if (isDouble()) {
        double val = asDouble();
        if (std::isnan(val)) return true;
        return std::fabs(val) < 1e21 && std::floor(val) == val;
    }
    if (isBoolean()) return true;
    if (isString()) return asString().find('.') == std::string::npos;
    return false;
}

bool Value::tryGetDouble(double &outValue) const {
    if (isDouble()) {
        outValue = asDouble();
        return true;
    }
    if (isString()) {
        const std::string &str = asString();
        if (str.empty()) return false;

        size_t idx = 0;
        try {
            outValue = std::stod(str, &idx);
            return idx == str.length();
        } catch (...) {
            return false;
        }
    }
    return false;
}

bool Value::strictEquals(const Value &other) const {
    if (this->type != other.type) return false;

    switch (this->type) {
    case ValueType::UNDEFINED:
        return true;
    case ValueType::BOOLEAN:
        return this->boolValue == other.boolValue;
    case ValueType::NUMBER:
        if (this->isNaN() && other.isNaN()) return true;
        return this->numberValue == other.numberValue;
    case ValueType::STRING_ID:
        return this->stringId == other.stringId;
    case ValueType::COLOR:
        return this->colorValue == other.colorValue;
    }
    return false;
}

Value Value::fromJson(const nlohmann::json &jsonVal) {
    if (jsonVal.is_number()) return Value(jsonVal.get<double>());
    if (jsonVal.is_string()) return Value::makeStringRef(StringPool::getOrInsert(jsonVal.get<std::string>()));
    if (jsonVal.is_boolean()) return Value(jsonVal.get<bool>());
    if (jsonVal.is_array()) {
        if (jsonVal.size() > 1) return fromJson(jsonVal[1]);
        return Value(0);
    }
    return Value(0);
}