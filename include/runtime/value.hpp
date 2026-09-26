#pragma once
#include "color.hpp"
#include "compiler_hints.hpp"
#include "math.hpp"
#include "shared_string.hpp"
#include <cstdint>
#include <new>
#include <nlohmann/json.hpp>
#include <se_export.hpp>
#include <string>
#include <type_traits>
#include <utility>

struct SE_EXPORT Undefined {};

class SE_EXPORT Value {
  private:
    enum class Tag : uint8_t { Double,
                               String,
                               Bool,
                               Color,
                               Undefined };

    // Faster than std::variant :)
    Tag tag;
    union Storage {
        double d;
        SharedString s;
        bool b;
        ::Color c;

        Storage() {}
        ~Storage() {}
    } storage;

    SE_FORCEINLINE void destroyActive() {
        if (tag == Tag::String) storage.s.~SharedString();
    }

    SE_FORCEINLINE void constructFrom(const Value &other) {
        switch (other.tag) {
        case Tag::String:
            new (&storage.s) SharedString(other.storage.s);
            break;
        case Tag::Double:
            storage.d = other.storage.d;
            break;
        case Tag::Bool:
            storage.b = other.storage.b;
            break;
        case Tag::Color:
            storage.c = other.storage.c;
            break;
        case Tag::Undefined:
            break;
        }
    }

    SE_FORCEINLINE void constructFrom(Value &&other) {
        switch (other.tag) {
        case Tag::String:
            new (&storage.s) SharedString(std::move(other.storage.s));
            break;
        case Tag::Double:
            storage.d = other.storage.d;
            break;
        case Tag::Bool:
            storage.b = other.storage.b;
            break;
        case Tag::Color:
            storage.c = other.storage.c;
            break;
        case Tag::Undefined:
            break;
        }
    }

  public:
    // constructors
    Value() : tag(Tag::String) { new (&storage.s) SharedString(); }

    explicit Value(int val);
    explicit Value(double val);
    explicit Value(std::string val);
    explicit Value(bool val);
    explicit Value(Color val);
    explicit Value(Undefined val);

    Value(const Value &other) : tag(other.tag) { constructFrom(other); }
    Value(Value &&other) noexcept : tag(other.tag) { constructFrom(std::move(other)); }

    Value &operator=(const Value &other) {
        if (this == &other) return *this;
        if (tag == Tag::String && other.tag == Tag::String) {
            storage.s = other.storage.s;
            return *this;
        }
        destroyActive();
        tag = other.tag;
        constructFrom(other);
        return *this;
    }

    Value &operator=(Value &&other) noexcept {
        if (this == &other) return *this;
        if (tag == Tag::String && other.tag == Tag::String) {
            storage.s = std::move(other.storage.s);
            return *this;
        }
        destroyActive();
        tag = other.tag;
        constructFrom(std::move(other));
        return *this;
    }

    ~Value() { destroyActive(); }

    // type checks
    inline bool isDouble() const {
        return tag == Tag::Double;
    }
    inline bool isString() const {
        return tag == Tag::String;
    }
    inline bool isBoolean() const {
        return tag == Tag::Bool;
    }
    inline bool isColor() const {
        return tag == Tag::Color;
    }
    inline bool isUndefined() const {
        return tag == Tag::Undefined;
    }
    inline bool isNumeric() const {
        if (isDouble() || isBoolean()) {
            return true;
        } else if (isString()) {
            auto &strValue = *storage.s;
            return Math::isNumber(strValue);
        }

        return false;
    }
    inline bool isNaN() const {
        return isDouble() && std::isnan(storage.d);
    }

    double asDouble() const;

    std::string asString() const;

    bool asBoolean() const;

    Color asColor() const;

    template <typename T>
    SE_FORCEINLINE T get() const {
        if constexpr (std::is_same_v<T, double>) {
            SE_LIKELY_IF(isDouble()) {
                const double d = storage.d;
                SE_UNLIKELY_IF(std::isnan(d)) {
                    return 0.0;
                }
                return d;
            }
            return asDouble();
        } else if constexpr (std::is_same_v<T, float>) {
            SE_LIKELY_IF(isDouble()) {
                const double d = storage.d;
                SE_UNLIKELY_IF(std::isnan(d)) {
                    return 0.0;
                }
                return static_cast<float>(d);
            }
            return static_cast<float>(asDouble());
        } else if constexpr (std::is_same_v<T, int>) {
            SE_LIKELY_IF(isDouble()) {
                const double d = storage.d;
                SE_UNLIKELY_IF(std::isnan(d)) {
                    return 0;
                }
                return static_cast<int>(d);
            }
            return static_cast<int>(asDouble());
        } else if constexpr (std::is_same_v<T, bool>) {
            SE_LIKELY_IF(isBoolean()) {
                return storage.b;
            }
            return asBoolean();
        } else if constexpr (std::is_same_v<T, std::string>) {
            SE_LIKELY_IF(isString()) {
                return *storage.s;
            }
            return asString();
        } else if constexpr (std::is_same_v<T, Color>) {
            return asColor();
        } else {
            static_assert(!sizeof(T), "Value::get<T>() has no implementation for this T");
        }
    }

    SE_FORCEINLINE const std::string *tryGetStringRef() const {
        if (isString()) return storage.s.get();
        return nullptr;
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
