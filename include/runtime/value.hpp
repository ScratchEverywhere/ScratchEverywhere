#pragma once
#include "color.hpp"
#include "compiler_hints.hpp"
#include "conversions.hpp"
#include "math.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <new>
#include <nlohmann/json.hpp>
#include <se_export.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

class SE_EXPORT StringPool {
  private:
    struct Entry {
        std::string data;
        uint32_t refCount = 0;

        enum class Utf16State : uint8_t { NOT_BUILT,
                                          ASCII,
                                          INDEXED };
        Utf16State utf16State = Utf16State::NOT_BUILT;
        std::vector<uint32_t> utf16Offsets;

        enum class NumericState : uint8_t { NOT_COMPUTED,
                                            NUMERIC,
                                            NOT_NUMERIC };
        NumericState numericState = NumericState::NOT_COMPUTED;
        bool hasDot = false;
        double numericValue = 0.0;

        bool deduped = false;

        size_t hash = 0;
    };

    static constexpr size_t dedupMaxLength = 32;

    struct Key {
        std::string_view sv;
        size_t hash;
    };
    struct KeyHash {
        size_t operator()(const Key &k) const noexcept { return k.hash; }
    };
    struct KeyEqual {
        bool operator()(const Key &a, const Key &b) const noexcept {
            return a.hash == b.hash && a.sv.size() == b.sv.size() && (a.sv.data() == b.sv.data() || a.sv == b.sv);
        }
    };

    static void ensureUtf16(Entry &entry);
    static void computeNumeric(Entry &entry);

    static std::deque<Entry> &storageRef();
    static std::unordered_map<Key, uint32_t, KeyHash, KeyEqual> &lookupRef();
    static std::vector<uint32_t> &freeListRef();

  public:
    static uint32_t getOrInsert(std::string_view str);
    static void addRef(uint32_t id);
    static void release(uint32_t id);
    static const std::string &get(uint32_t id);

    static size_t utf16Length(uint32_t id);
    static std::string utf16CharAt(uint32_t id, size_t index);

    static bool isNumeric(uint32_t id, bool &hasDot);
    static double toNumber(uint32_t id);
    static void resetAll();
};

struct SE_EXPORT Undefined {};

class SE_EXPORT Value {
  public:
    enum class ValueType : uint8_t {
        UNDEFINED,
        BOOLEAN,
        NUMBER,
        STRING_ID,
        COLOR
    };

    using Tag = ValueType;

  private:
    Tag tag;
    mutable uint8_t flags = 0;

    static constexpr uint8_t FLAG_NUMERIC_COMPUTED = 1 << 0;
    static constexpr uint8_t FLAG_IS_NUMERIC = 1 << 1;
    static constexpr uint8_t FLAG_HAS_DOT = 1 << 2;
    static constexpr uint8_t FLAG_STRING_TYPED = 1 << 3;

    union Storage {
        double d;
        uint32_t stringId;
        bool b;
        ::Color c;

        Storage() {}
        ~Storage() {}
    } storage;

    SE_FORCEINLINE void destroyActive() {
        if (tag == Tag::STRING_ID && storage.stringId != 0) {
            StringPool::release(storage.stringId);
        }
    }

    SE_FORCEINLINE void constructFrom(const Value &other) {
        switch (other.tag) {
        case Tag::STRING_ID:
            storage.stringId = other.storage.stringId;
            StringPool::addRef(storage.stringId);
            break;
        case Tag::NUMBER:
            storage.d = other.storage.d;
            break;
        case Tag::BOOLEAN:
            storage.b = other.storage.b;
            break;
        case Tag::COLOR:
            storage.c = other.storage.c;
            break;
        case Tag::UNDEFINED:
            break;
        }
    }

    SE_FORCEINLINE void constructFrom(Value &&other) {
        switch (other.tag) {
        case Tag::STRING_ID:
            storage.stringId = other.storage.stringId;
            other.storage.stringId = 0;
            break;
        case Tag::NUMBER:
            storage.d = other.storage.d;
            break;
        case Tag::BOOLEAN:
            storage.b = other.storage.b;
            break;
        case Tag::COLOR:
            storage.c = other.storage.c;
            break;
        case Tag::UNDEFINED:
            break;
        }
    }

    std::string_view asStringView(std::string &scratch) const;
    bool equalsSlow(const Value &other) const;

  public:
    Value() : tag(Tag::STRING_ID), flags(0) {
        storage.stringId = 0;
    }

    explicit Value(int val);
    explicit Value(double val);
    explicit Value(std::string_view val);
    explicit Value(std::string val);
    explicit Value(std::shared_ptr<const std::string> val);
    explicit Value(bool val);
    explicit Value(Color val);
    explicit Value(Undefined val);

    struct Layout {
        size_t size, tag, flags, number;
        uint8_t numberTag, stringTag, stringTypedFlag;
    };
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
    static Layout layout() {
        return {sizeof(Value), offsetof(Value, tag), offsetof(Value, flags), offsetof(Value, storage),
                static_cast<uint8_t>(Tag::NUMBER), static_cast<uint8_t>(Tag::STRING_ID), FLAG_STRING_TYPED};
    }
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    static Value stringTypedNumber(double d) {
        Value v(d);
        v.flags = FLAG_STRING_TYPED;
        return v;
    }

    Value(const Value &other) : tag(other.tag), flags(other.flags) {
        constructFrom(other);
    }

    Value(Value &&other) noexcept : tag(other.tag), flags(other.flags) {
        constructFrom(std::move(other));
        other.tag = Tag::STRING_ID;
        other.storage.stringId = 0;
        other.flags = 0;
    }

    Value &operator=(const Value &other);
    Value &operator=(Value &&other) noexcept;

    ~Value() { destroyActive(); }

    inline Tag getTag() const { return tag; }
    inline uint32_t stringId() const { return tag == Tag::STRING_ID ? storage.stringId : 0; }

    inline std::string_view asStringView() const {
        assert(tag == Tag::STRING_ID && "asStringView() is only valid for STRING_ID values");
        return StringPool::get(storage.stringId);
    }

    // type checks
    inline bool isDouble() const {
        return tag == Tag::NUMBER;
    }
    inline bool isString() const {
        return tag == Tag::STRING_ID;
    }
    inline bool isRealNumber() const {
        return tag == Tag::NUMBER && (flags & FLAG_STRING_TYPED) == 0;
    }
    inline bool isStringTyped() const {
        return tag == Tag::STRING_ID || (tag == Tag::NUMBER && (flags & FLAG_STRING_TYPED) != 0);
    }
    inline bool isBoolean() const {
        return tag == Tag::BOOLEAN;
    }
    inline bool isColor() const {
        return tag == Tag::COLOR;
    }
    inline bool isUndefined() const {
        return tag == Tag::UNDEFINED;
    }
    bool isNumeric() const;
    bool plainNumber(double &out) const;
    static bool compareStrings(int kind, std::string_view a, std::string_view b);
    inline bool isNaN() const {
        return isDouble() && std::isnan(storage.d);
    }

    SE_FORCEINLINE void setNumber(double d) {
        destroyActive();
        tag = Tag::NUMBER;
        flags = 0;
        storage.d = d;
    }

    SE_FORCEINLINE double numberRaw() const {
        return tag == Tag::NUMBER ? storage.d : asDouble();
    }

    SE_FORCEINLINE double asDouble() const {
        switch (tag) {
        case Tag::NUMBER:
            return Conversions::numberToNumber(storage.d);
        case Tag::STRING_ID:
            return StringPool::toNumber(storage.stringId);
        case Tag::COLOR:
            return Conversions::colorToNumber(storage.c);
        case Tag::BOOLEAN:
            return Conversions::boolToNumber(storage.b);
        case Tag::UNDEFINED:
            break;
        }
        return 0.0;
    }

    SE_FORCEINLINE std::string asString() const {
        switch (tag) {
        case Tag::NUMBER:
            return Conversions::numberToString(storage.d);
        case Tag::STRING_ID:
            return StringPool::get(storage.stringId);
        case Tag::BOOLEAN:
            return Conversions::boolToString(storage.b);
        case Tag::UNDEFINED:
            return "undefined";
        case Tag::COLOR:
            return Conversions::colorToString(storage.c);
        }
        return "";
    }

    SE_FORCEINLINE bool asBoolean() const {
        switch (tag) {
        case Tag::BOOLEAN:
            return storage.b;
        case Tag::NUMBER:
            return Conversions::numberToBool(storage.d);
        case Tag::STRING_ID:
            return Conversions::stringToBool(StringPool::get(storage.stringId));
        case Tag::COLOR:
            return Conversions::colorToBool(storage.c);
        case Tag::UNDEFINED:
            break;
        }
        return false;
    }

    SE_FORCEINLINE Color asColor() const {
        if (tag == Tag::STRING_ID) return Conversions::stringToColor(StringPool::get(storage.stringId));
        return Conversions::numberToColor(asDouble());
    }

    SE_FORCEINLINE ColorRGBA asRGB() const {
        if (tag == Tag::STRING_ID) return Conversions::stringToRGB(StringPool::get(storage.stringId));
        return Conversions::numberToRGB(asDouble());
    }

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
                return StringPool::get(storage.stringId);
            }
            return asString();
        } else if constexpr (std::is_same_v<T, Color>) {
            return asColor();
        } else {
            static_assert(!sizeof(T), "Value::get<T>() has no implementation for this T");
        }
    }

    size_t utf16Length() const;
    std::string utf16CharAt(size_t index) const;

    SE_FORCEINLINE const std::string *tryGetStringRef() const {
        if (isString()) return &StringPool::get(storage.stringId);
        return nullptr;
    }

    SE_FORCEINLINE std::shared_ptr<const std::string> getStringPtr() const {
        if (isString()) return std::make_shared<std::string>(StringPool::get(storage.stringId));
        return std::make_shared<std::string>(asString());
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
    SE_FORCEINLINE bool operator==(const Value &other) const {
        SE_LIKELY_IF(tag == Tag::NUMBER && other.tag == Tag::NUMBER && !std::isnan(storage.d) && !std::isnan(other.storage.d)) {
            return storage.d == other.storage.d;
        }
        return equalsSlow(other);
    }
    bool operator<(const Value &other) const;
    bool operator>(const Value &other) const;

    inline bool strictEquals(const Value &other) const {
        if (tag != other.tag) return false;
        switch (tag) {
        case Tag::NUMBER:
            return (isNaN() && other.isNaN()) || storage.d == other.storage.d;
        case Tag::STRING_ID:
            return storage.stringId == other.storage.stringId;
        case Tag::BOOLEAN:
            return storage.b == other.storage.b;
        case Tag::COLOR:
            return storage.c == other.storage.c;
        case Tag::UNDEFINED:
            return true;
        }
        return false;
    }

    // Used exclusively by the random block
    bool isScratchInt();

    static Value fromJson(const nlohmann::json &jsonVal);
};

inline void insertValueAt(std::vector<Value> &values, size_t pos, Value &&value) {
    values.emplace_back();
    Value *data = values.data();
    std::memmove(static_cast<void *>(data + pos + 1), static_cast<const void *>(data + pos), (values.size() - 1 - pos) * sizeof(Value));
    ::new (static_cast<void *>(data + pos)) Value(std::move(value));
}

inline void eraseValueAt(std::vector<Value> &values, size_t pos) {
    Value *data = values.data();
    data[pos].~Value();
    std::memmove(static_cast<void *>(data + pos), static_cast<const void *>(data + pos + 1), (values.size() - 1 - pos) * sizeof(Value));
    ::new (static_cast<void *>(data + values.size() - 1)) Value();
    values.pop_back();
}
