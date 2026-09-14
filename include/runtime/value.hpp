#pragma once
#include "color.hpp"
#include "compiler_hints.hpp"
#include "math.hpp"
#include <nlohmann/json.hpp>
#include <se_export.hpp>
#include <string>

#include <type_traits>
#include <variant>

struct SE_EXPORT Undefined {};

class SE_EXPORT Value {
  private:
    std::variant<double, std::string, bool, Color, Undefined> value;

  public:
    // constructors
    Value() : value(std::string()) {}

    explicit Value(int val);
    explicit Value(double val);
    explicit Value(std::string val);
    explicit Value(bool val);
    explicit Value(Color val);
    explicit Value(Undefined val);

    // type checks
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
    inline bool isUndefined() const {
        return std::holds_alternative<Undefined>(value);
    }
    inline bool isNumeric() const {
        if (isDouble() || isBoolean()) {
            return true;
        } else if (isString()) {
            auto &strValue = std::get<std::string>(value);
            return Math::isNumber(strValue);
        }

        return false;
    }
    inline bool isNaN() const {
        return isDouble() && std::isnan(std::get<double>(value));
    }

    double asDouble() const;

    std::string asString() const;

    bool asBoolean() const;

    Color asColor() const;

    template <typename T>
    SE_FORCEINLINE T get() const {
        if constexpr (std::is_same_v<T, double>) {
            SE_LIKELY_IF(isDouble()) {
                const double d = std::get<double>(value);
                SE_UNLIKELY_IF(std::isnan(d)) {
                    return 0.0;
                }
                return d;
            }
            return asDouble();
        } else if constexpr (std::is_same_v<T, float>) {
            SE_LIKELY_IF(isDouble()) {
                const double d = std::get<double>(value);
                SE_UNLIKELY_IF(std::isnan(d)) {
                    return 0.0;
                }
                return static_cast<float>(d);
            }
            return static_cast<float>(asDouble());
        } else if constexpr (std::is_same_v<T, int>) {
            SE_LIKELY_IF(isDouble()) {
                const double d = std::get<double>(value);
                SE_UNLIKELY_IF(std::isnan(d)) {
                    return 0;
                }
                return static_cast<int>(d);
            }
            return static_cast<int>(asDouble());
        } else if constexpr (std::is_same_v<T, bool>) {
            SE_LIKELY_IF(isBoolean()) {
                return std::get<bool>(value);
            }
            return asBoolean();
        } else if constexpr (std::is_same_v<T, std::string>) {
            SE_LIKELY_IF(isString()) {
                return std::get<std::string>(value);
            }
            return asString();
        } else if constexpr (std::is_same_v<T, Color>) {
            return asColor();
        } else {
            static_assert(!sizeof(T), "Value::get<T>() has no implementation for this T");
        }
    }

    SE_FORCEINLINE Value operator+(const Value &other) const {
        return Value(get<double>() + other.get<double>());
    }

    SE_FORCEINLINE Value operator-(const Value &other) const {
        return Value(get<double>() - other.get<double>());
    }

    SE_FORCEINLINE Value operator*(const Value &other) const {
        return Value(get<double>() * other.get<double>());
    }

    SE_FORCEINLINE Value operator/(const Value &other) const {
        const double a = isNumeric() ? get<double>() : 0.0;
        const double b = other.isNumeric() ? other.get<double>() : 0.0;
        return Value(a / b);
    }

    // Comparison operators
    bool operator==(const Value &other) const;

    bool operator<(const Value &other) const;

    bool operator>(const Value &other) const;

    // Used exclusively by the random block
    bool isScratchInt();

    static Value fromJson(const nlohmann::json &jsonVal);
};
