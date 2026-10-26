#pragma once
#include "color.hpp"
#include "compiler_hints.hpp"
#include "math.hpp"
#include <cassert>
#include <cstdint>
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

    union Storage {
        double d;
        uint32_t stringId;
        bool b;
        ::Color c;

        Storage() {}
        ~Storage() {}
    } storage;

    SE_FORCEINLINE void destroyActive() {
        if (tag == Tag::STRING_ID) {
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
    inline bool isNaN() const {
        return isDouble() && std::isnan(storage.d);
    }

    double asDouble() const;

    std::string asString() const;

    bool asBoolean() const;

    Color asColor() const;
    ColorRGBA asRGB() const;

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
        if (isString()) return std::make_shared<const std::string>(StringPool::get(storage.stringId));
        return std::make_shared<const std::string>(asString());
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
