#include "value.hpp"
#include "math.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <os.hpp>
#include <regex>
#include <string_view>

using Math::caseInsensitiveEqual;
using Math::caseInsensitiveLess;

std::deque<StringPool::Entry> &StringPool::storageRef() {
    static std::deque<Entry> s = [] {
        std::deque<Entry> d;
        d.push_back({"", 1});
        return d;
    }();
    return s;
}

std::unordered_map<StringPool::Key, uint32_t, StringPool::KeyHash, StringPool::KeyEqual> &StringPool::lookupRef() {
    static std::unordered_map<Key, uint32_t, KeyHash, KeyEqual> m = [] {
        std::unordered_map<Key, uint32_t, KeyHash, KeyEqual> mm;
        mm.emplace(Key{std::string_view(storageRef()[0].data), std::hash<std::string_view>{}(std::string_view())}, 0u);
        return mm;
    }();
    return m;
}

std::vector<uint32_t> &StringPool::freeListRef() {
    static std::vector<uint32_t> f;
    return f;
}

uint32_t StringPool::getOrInsert(std::string_view str) {
    if (str.empty()) return 0u;

    auto &pool = storageRef();
    auto &freeSlots = freeListRef();

    const bool tryDedup = str.size() <= dedupMaxLength;
    size_t hash = 0;

    if (tryDedup) {
        auto &lookup = lookupRef();
        hash = std::hash<std::string_view>{}(str);
        auto it = lookup.find(Key{str, hash});
        if (it != lookup.end()) {
            uint32_t id = it->second;
            pool[id].refCount++;
            return id;
        }
    }

    uint32_t id;
    if (!freeSlots.empty()) {
        id = freeSlots.back();
        freeSlots.pop_back();
        pool[id].data.assign(str.data(), str.size());
        pool[id].refCount = 1;
    } else {
        id = static_cast<uint32_t>(pool.size());
        pool.push_back({std::string(str), 1});
    }

    pool[id].deduped = tryDedup;
    if (tryDedup) {
        pool[id].hash = hash;
        lookupRef().emplace(Key{std::string_view(pool[id].data), hash}, id);
    }
    return id;
}

void StringPool::addRef(uint32_t id) {
    if (id == 0) return;
    auto &pool = storageRef();
    if (id < pool.size()) pool[id].refCount++;
}

void StringPool::release(uint32_t id) {
    if (id == 0) return;
    auto &pool = storageRef();
    auto &freeSlots = freeListRef();

    if (id >= pool.size()) return;
    Entry &e = pool[id];
    if (e.refCount == 0) return;

    if (--e.refCount == 0) {
        if (e.deduped) lookupRef().erase(Key{std::string_view(e.data), e.hash});
        e.data.clear();
        e.deduped = false;
        e.numericState = Entry::NumericState::NOT_COMPUTED;
        e.utf16State = Entry::Utf16State::NOT_BUILT;
        std::vector<uint32_t>().swap(e.utf16Offsets);
        freeSlots.push_back(id);
    }
}

void StringPool::ensureUtf16(Entry &entry) {
    if (entry.utf16State != Entry::Utf16State::NOT_BUILT) return;

    const bool ascii = std::all_of(entry.data.begin(), entry.data.end(), [](unsigned char c) { return c < 0x80; });
    if (ascii) {
        entry.utf16State = Entry::Utf16State::ASCII;
        return;
    }
    entry.utf16Offsets = Math::utf16Index(entry.data);
    entry.utf16State = Entry::Utf16State::INDEXED;
}

void StringPool::computeNumeric(Entry &e) {
    const auto parsed = Math::parseNumber(e.data);
    e.numericState = parsed.has_value() ? Entry::NumericState::NUMERIC : Entry::NumericState::NOT_NUMERIC;
    e.numericValue = parsed.has_value() ? *parsed : 0.0;
    e.hasDot = e.data.find('.') != std::string::npos;
}

bool StringPool::isNumeric(uint32_t id, bool &hasDot) {
    auto &pool = storageRef();
    Entry &e = pool[id < pool.size() ? id : 0];
    if (e.numericState == Entry::NumericState::NOT_COMPUTED) computeNumeric(e);
    hasDot = e.hasDot;
    return e.numericState == Entry::NumericState::NUMERIC;
}

double StringPool::toNumber(uint32_t id) {
    auto &pool = storageRef();
    Entry &e = pool[id < pool.size() ? id : 0];
    if (e.numericState == Entry::NumericState::NOT_COMPUTED) computeNumeric(e);
    return e.numericState == Entry::NumericState::NUMERIC ? e.numericValue : 0.0;
}

size_t StringPool::utf16Length(uint32_t id) {
    auto &pool = storageRef();
    Entry &e = pool[id < pool.size() ? id : 0];
    ensureUtf16(e);
    if (e.utf16State == Entry::Utf16State::ASCII) return e.data.size();
    return e.utf16Offsets.empty() ? 0 : e.utf16Offsets.size() - 1;
}

std::string StringPool::utf16CharAt(uint32_t id, size_t index) {
    auto &pool = storageRef();
    Entry &e = pool[id < pool.size() ? id : 0];
    ensureUtf16(e);
    if (e.utf16State == Entry::Utf16State::ASCII) return index < e.data.size() ? std::string(1, e.data[index]) : std::string();
    if (index + 1 >= e.utf16Offsets.size()) return "";
    return e.data.substr(e.utf16Offsets[index], e.utf16Offsets[index + 1] - e.utf16Offsets[index]);
}

const std::string &StringPool::get(uint32_t id) {
    auto &pool = storageRef();
    if (id >= pool.size()) return pool[0].data;
    return pool[id].data;
}

void StringPool::resetAll() {
    auto &pool = storageRef();
    auto &lookup = lookupRef();
    auto &freeSlots = freeListRef();

    pool.clear();
    pool.push_back({"", 1});
    lookup.clear();
    lookup.emplace(Key{std::string_view(pool[0].data), std::hash<std::string_view>{}(std::string_view())}, 0u);
    freeSlots.clear();
}

Value::Value(int val) : tag(Tag::NUMBER), flags(0) { storage.d = static_cast<double>(val); }

Value::Value(double val) : tag(Tag::NUMBER), flags(0) { storage.d = val; }

Value::Value(std::string_view val) : tag(Tag::STRING_ID), flags(0) {
    storage.stringId = StringPool::getOrInsert(val);
}

Value::Value(std::string val) : tag(Tag::STRING_ID), flags(0) {
    storage.stringId = StringPool::getOrInsert(val);
}

Value::Value(std::shared_ptr<const std::string> val) : tag(Tag::STRING_ID), flags(0) {
    storage.stringId = val ? StringPool::getOrInsert(*val) : 0u;
}

Value::Value(bool val) : tag(Tag::BOOLEAN), flags(0) { storage.b = val; }

Value::Value(Color val) : tag(Tag::COLOR), flags(0) { storage.c = val; }

Value::Value(Undefined) : tag(Tag::UNDEFINED), flags(0) {}

Value &Value::operator=(const Value &other) {
    if (this == &other) return *this;

    if (tag == Tag::STRING_ID && other.tag == Tag::STRING_ID) {
        if (storage.stringId == other.storage.stringId) {
            flags = other.flags;
            return *this;
        }
        StringPool::addRef(other.storage.stringId);
        StringPool::release(storage.stringId);
        storage.stringId = other.storage.stringId;
        flags = other.flags;
        return *this;
    }

    destroyActive();
    tag = other.tag;
    flags = other.flags;
    constructFrom(other);
    return *this;
}

Value &Value::operator=(Value &&other) noexcept {
    if (this == &other) return *this;

    if (tag == Tag::STRING_ID && other.tag == Tag::STRING_ID) {
        if (storage.stringId == other.storage.stringId) {
            StringPool::release(other.storage.stringId);
            flags = other.flags;
            other.storage.stringId = 0;
            other.flags = 0;
            return *this;
        }
        StringPool::release(storage.stringId);
        storage.stringId = other.storage.stringId;
        flags = other.flags;
        other.storage.stringId = 0;
        other.flags = 0;
        return *this;
    }

    destroyActive();
    tag = other.tag;
    flags = other.flags;
    constructFrom(std::move(other));
    other.tag = Tag::STRING_ID;
    other.storage.stringId = 0;
    other.flags = 0;
    return *this;
}

bool Value::isNumeric() const {
    if (isDouble() || isBoolean()) return true;
    if (!isString()) return false;

    if (flags & FLAG_NUMERIC_COMPUTED) {
        return (flags & FLAG_IS_NUMERIC) != 0;
    }

    bool hasDot;
    const bool numeric = StringPool::isNumeric(storage.stringId, hasDot);

    flags |= FLAG_NUMERIC_COMPUTED;
    if (numeric) flags |= FLAG_IS_NUMERIC;
    if (hasDot) flags |= FLAG_HAS_DOT;

    return numeric;
}

std::string_view Value::asStringView(std::string &scratch) const {
    if (isString()) {
        return StringPool::get(storage.stringId);
    }
    scratch = asString();
    return scratch;
}

bool Value::equalsSlow(const Value &other) const {
    if (isDouble() && other.isDouble() && !isNaN() && !other.isNaN())
        return storage.d == other.storage.d;
    if (isBoolean() && other.isBoolean())
        return storage.b == other.storage.b;
    if (isColor() && other.isColor())
        return storage.c == other.storage.c;

    if (isString() && other.isString() && storage.stringId == other.storage.stringId)
        return true;

    std::string scratchA, scratchB;
    std::string_view svA = asStringView(scratchA);
    std::string_view svB = other.asStringView(scratchB);

    if (isNumeric() && other.isNumeric() && !isNaN() && !other.isNaN() &&
        !std::all_of(svA.begin(), svA.end(), [](unsigned char c) { return (std::isspace(c) && c != '\t'); }) &&
        !std::all_of(svB.begin(), svB.end(), [](unsigned char c) { return (std::isspace(c) && c != '\t'); })) {
        return asDouble() == other.asDouble();
    }

    return caseInsensitiveEqual(svA, svB);
}

bool Value::operator<(const Value &other) const {
    if (isNumeric() && other.isNumeric() && !isNaN() && !other.isNaN()) {
        return get<double>() < other.get<double>();
    }

    if (isString() && other.isString() && storage.stringId == other.storage.stringId)
        return false;

    std::string scratchA, scratchB;
    std::string_view svA = asStringView(scratchA);
    std::string_view svB = other.asStringView(scratchB);
    return caseInsensitiveLess(svA, svB);
}

bool Value::operator>(const Value &other) const {
    if (isNumeric() && other.isNumeric() && !isNaN() && !other.isNaN()) {
        return get<double>() > other.get<double>();
    }

    if (isString() && other.isString() && storage.stringId == other.storage.stringId)
        return false;

    std::string scratchA, scratchB;
    std::string_view svA = asStringView(scratchA);
    std::string_view svB = other.asStringView(scratchB);
    return caseInsensitiveLess(svB, svA);
}

bool Value::isScratchInt() {
    if (isDouble()) {
        double val = asDouble();
        if (std::isnan(val)) return true;
        return std::fabs(val) < 1e21 && std::floor(val) == val;
    }
    if (isBoolean()) return true;
    if (isString()) {
        if (flags & FLAG_NUMERIC_COMPUTED) {
            return (flags & FLAG_HAS_DOT) == 0;
        }
        const std::string_view sv = StringPool::get(storage.stringId);
        return sv.find('.') == std::string_view::npos;
    }
    return false;
}

Value Value::fromJson(const nlohmann::json &jsonVal) {
    if (jsonVal.is_number()) return Value(jsonVal.get<double>());
    if (jsonVal.is_string()) return Value(jsonVal.get<std::string_view>());
    if (jsonVal.is_boolean()) return Value(jsonVal.get<bool>());
    if (jsonVal.is_array()) {
        if (jsonVal.size() > 1) return fromJson(jsonVal[1]);
        return Value(0);
    }
    return Value(0);
}

size_t Value::utf16Length() const {
    if (isString()) return StringPool::utf16Length(storage.stringId);
    return Math::utf16Length(asString());
}

std::string Value::utf16CharAt(size_t index) const {
    if (isString()) return StringPool::utf16CharAt(storage.stringId, index);
    return Math::utf16CharAt(asString(), index);
}
