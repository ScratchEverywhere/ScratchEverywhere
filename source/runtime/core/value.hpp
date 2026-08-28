#pragma once
#include "color.hpp"

#include "nlohmann/json.hpp"
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct StringEntry {
    std::string data;
    uint32_t refCount = 0;
};

class StringPool {
  private:
    static std::vector<StringEntry> pool;
    static std::unordered_map<std::string_view, uint32_t> lookup;
    static std::vector<uint32_t> freeSlots;

    static void updateLookupPointers() {
        lookup.clear();
        for (size_t i = 0; i < pool.size(); ++i) {
            if (pool[i].refCount > 0) {
                lookup[pool[i].data] = static_cast<uint32_t>(i);
            }
        }
    }

  public:
    StringPool() {
        pool.reserve(256);
        freeSlots.reserve(32);
        getOrInsert("");
    }
    static const std::vector<StringEntry> &getPool() { return pool; }
    static uint32_t getOrInsert(std::string_view str) {
        auto it = lookup.find(str);
        if (it != lookup.end()) {
            uint32_t id = it->second;
            pool[id].refCount++;
            return id;
        }

        uint32_t id;
        if (!freeSlots.empty()) {
            id = freeSlots.back();
            freeSlots.pop_back();
            pool[id].data = std::string(str);
            pool[id].refCount = 1;
            lookup[pool[id].data] = id;
        } else {
            id = static_cast<uint32_t>(pool.size());

            size_t oldCap = pool.capacity();
            pool.push_back({std::string(str), 1});

            if (pool.capacity() != oldCap) {
                updateLookupPointers();
            } else {
                lookup[pool[id].data] = id;
            }
        }

        return id;
    }

    static void release(uint32_t id) {
        if (id == 0 || id >= pool.size() || pool[id].refCount == 0) return;

        pool[id].refCount--;
        if (pool[id].refCount == 0) {
            lookup.erase(pool[id].data);
            pool[id].data.clear();
            freeSlots.push_back(id);
        }
    }

    static void addRef(uint32_t id) {
        if (id > 0 && id < pool.size()) pool[id].refCount++;
    }

    static inline const std::string &get(uint32_t id) {
        return pool[id].data;
    }

    static void resetAll() {
        pool.clear();
        lookup.clear();
        freeSlots.clear();
        getOrInsert("");
    }
};

enum class ValueType : uint8_t {
    UNDEFINED,
    BOOLEAN,
    NUMBER,
    STRING_ID,
    COLOR
};

class Value {
  private:
    ValueType type{ValueType::UNDEFINED};
    uint8_t flags = 0;

    union {
        double numberValue;
        bool boolValue;
        Color colorValue;
        uint32_t stringId;
    };

  public:
    Value() : type(ValueType::UNDEFINED), numberValue(0.0) {}
    explicit Value(int val) : type(ValueType::NUMBER), numberValue(static_cast<double>(val)) {}
    explicit Value(double val) : type(ValueType::NUMBER), numberValue(val) {}
    explicit Value(bool val) : type(ValueType::BOOLEAN), boolValue(val) {}
    explicit Value(Color val) : type(ValueType::COLOR), colorValue(val) {}
    explicit Value(std::string_view val) : type(ValueType::STRING_ID), stringId(StringPool::getOrInsert(val)) {}

    ~Value() {
        if (type == ValueType::STRING_ID) {
            StringPool::release(stringId);
        }
    }

    Value(const Value &other) {
        this->type = other.type;
        this->flags = other.flags;
        this->numberValue = other.numberValue;
        this->stringId = other.stringId;

        if (this->type == ValueType::STRING_ID) {
            StringPool::addRef(this->stringId);
        }
    }

    static Value makeStringRef(uint16_t id) {
        Value v;
        v.type = ValueType::STRING_ID;
        v.stringId = id;
        return v;
    }

    inline bool isDouble() const { return type == ValueType::NUMBER; }
    inline bool isString() const { return type == ValueType::STRING_ID; }
    inline bool isBoolean() const { return type == ValueType::BOOLEAN; }
    inline bool isColor() const { return type == ValueType::COLOR; }
    inline bool isUndefined() const { return type == ValueType::UNDEFINED; }

    inline bool isNaN() const {
        return type == ValueType::NUMBER && std::isnan(numberValue);
    }

    bool isNumeric() const;
    double asDouble() const;
    std::string asString() const;
    bool asBoolean() const;
    Color asColor() const;

    static inline bool equalsIgnoreCase(std::string_view a, std::string_view b);
    static inline bool lessThanIgnoreCase(std::string_view a, std::string_view b);

    bool lessThan(const Value &other) const;
    bool greaterThan(const Value &other) const;

    // Arithmetic operations
    Value operator+(const Value &other) const;

    Value operator-(const Value &other) const;

    Value operator*(const Value &other) const;

    Value operator/(const Value &other) const;

    // Comparison operators
    bool operator==(const Value &other) const;

    bool operator<(const Value &other) const;

    bool operator>(const Value &other) const;

    Value &operator=(const Value &other);

    // Used exclusively by the random block
    bool isScratchInt();
    bool tryGetDouble(double &outValue) const;

    inline void dropValue(Value &val) {
        if (val.isString()) {
            StringPool::release(val.stringId);
        }
    }

    bool strictEquals(const Value &other) const;

    static Value fromJson(const nlohmann::json &jsonVal);
    static nlohmann::json toJson(const Value &val);
};