#include "value.hpp"

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
        return strValue == "Infinity" || strValue == "-Infinity" || Math::isNumber(strValue);
    }

    return false;
}

double Value::asDouble() const {
    if (isInteger()) {
        return static_cast<double>(std::get<int>(value));
    } else if (isDouble()) {
        return std::get<double>(value);
    } else if (isString()) {
        auto &strValue = std::get<std::string>(value);

        if (strValue == "Infinity") {
            return std::numeric_limits<double>::max();
        }

        if (strValue == "-Infinity") {
            return -std::numeric_limits<double>::max();
        }

        if (Math::isNumber(strValue)) {
            return Math::parseNumber(strValue);
        }
    } else if (isBoolean()) {
        return std::get<bool>(value) ? 1.0 : 0.0;
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
            return std::numeric_limits<int>::max();
        }

        if (strValue == "-Infinity") {
            return -std::numeric_limits<int>::max();
        }

        if (Math::isNumber(strValue)) {
            return static_cast<int>(std::round(Math::parseNumber(strValue)));
        }
    } else if (isBoolean()) {
        return std::get<bool>(value) ? 1 : 0;
    }

    return 0;
}

std::string Value::asString() const {
    if (isInteger()) {
        return std::to_string(std::get<int>(value));
    } else if (isDouble()) {
        double doubleValue = std::get<double>(value);
        // handle whole numbers too, because scratch i guess
        if (std::floor(doubleValue) == doubleValue) {
            return std::to_string(static_cast<int>(doubleValue));
        }
    } else if (isString()) {
        return std::get<std::string>(value);
    } else if (isBoolean()) {
        return std::get<bool>(value) ? "true" : "false";
    }

    return "";
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

    double bVal = b.asDouble();
    if (bVal == 0.0) return Value(0); // Division by zero
    return Value(a.asDouble() / bVal);
}

bool Value::operator==(const Value &other) const {
    if (value.index() == other.value.index()) {
        return value == other.value;
    }

    // Different types - compare as strings (Scratch behavior)
    return asString() == other.asString();
}

bool Value::operator<(const Value &other) const {
    if (isNumeric() && other.isNumeric()) {
        return asDouble() < other.asDouble();
    }
    return asString() < other.asString();
}

bool Value::operator>(const Value &other) const {
    if (isNumeric() && other.isNumeric()) {
        return asDouble() > other.asDouble();
    }
    return asString() > other.asString();
}

Value Value::fromJson(const nlohmann::json &jsonVal) {
    if (jsonVal.is_null()) return Value();

    if (jsonVal.is_number_integer()) {
        return Value(jsonVal.get<int>());
    } else if (jsonVal.is_number_float()) {
        return Value(jsonVal.get<double>());
    } else if (jsonVal.is_string()) {
        std::string strVal = jsonVal.get<std::string>();

        if (strVal == "Infinity" || strVal == "-Infinity")
            return Value(strVal);

        if (Math::isNumber(strVal)) {
            double numVal;
            try {
                numVal = Math::parseNumber(strVal);
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
        return Value(jsonVal.get<bool>());
    } else if (jsonVal.is_array()) {
        if (jsonVal.size() > 1) {
            return fromJson(jsonVal[1]);
        }
        return Value(0);
    }
    return Value(0);
}
