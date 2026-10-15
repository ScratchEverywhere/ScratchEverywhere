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

std::unordered_map<std::string_view, uint32_t> &StringPool::lookupRef() {
    static std::unordered_map<std::string_view, uint32_t> m = [] {
        std::unordered_map<std::string_view, uint32_t> mm;
        mm.emplace(std::string_view(storageRef()[0].data), 0u);
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
    auto &lookup = lookupRef();
    auto &freeSlots = freeListRef();

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
        pool[id].data.assign(str.data(), str.size());
        pool[id].refCount = 1;
    } else {
        id = static_cast<uint32_t>(pool.size());
        pool.push_back({std::string(str), 1});
    }

    lookup.emplace(std::string_view(pool[id].data), id);
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
    auto &lookup = lookupRef();
    auto &freeSlots = freeListRef();

    if (id >= pool.size()) return;
    Entry &e = pool[id];
    if (e.refCount == 0) return;

    if (--e.refCount == 0) {
        lookup.erase(std::string_view(e.data));
        e.data.clear();
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
    lookup.emplace(std::string_view(pool[0].data), 0u);
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

    const std::string &str = StringPool::get(storage.stringId);
    bool numeric = Math::isNumber(str);
    bool hasDot = str.find('.') != std::string::npos;

    flags |= FLAG_NUMERIC_COMPUTED;
    if (numeric) flags |= FLAG_IS_NUMERIC;
    if (hasDot) flags |= FLAG_HAS_DOT;

    return numeric;
}

double Value::asDouble() const {
    if (isDouble()) {
        if (isNaN()) return 0.0;
        return storage.d;
    } else if (isString()) {
        return Math::parseNumber(StringPool::get(storage.stringId)).value_or(0);
    } else if (isColor()) {
        const ColorRGBA rgb = CSBT2RGBA(storage.c);
        return rgb.r * 0x10000 + rgb.g * 0x100 + rgb.b;
    } else if (isBoolean()) {
        return storage.b ? 1 : 0;
    }

    return 0.0;
}

std::string Value::asString() const {
    if (isDouble()) {
        return Math::toString(storage.d);
    } else if (isString()) {
        return StringPool::get(storage.stringId);
    } else if (isBoolean()) {
        return storage.b ? "true" : "false";
    } else if (isUndefined()) {
        return "undefined";
    } else if (isColor()) {
        const ColorRGBA rgb = CSBT2RGBA(storage.c);
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
        return storage.b;
    }
    if (isDouble()) {
        return storage.d != 0.0 && !isNaN();
    }
    if (isString()) {
        std::string_view sv = StringPool::get(storage.stringId);
        if (sv.empty() || sv == "0") return false;
        if (sv.size() == 5) {
            char f0 = std::tolower(static_cast<unsigned char>(sv[0]));
            char f1 = std::tolower(static_cast<unsigned char>(sv[1]));
            char f2 = std::tolower(static_cast<unsigned char>(sv[2]));
            char f3 = std::tolower(static_cast<unsigned char>(sv[3]));
            char f4 = std::tolower(static_cast<unsigned char>(sv[4]));
            if (f0 == 'f' && f1 == 'a' && f2 == 'l' && f3 == 's' && f4 == 'e') {
                return false;
            }
        }
        return true;
    }
    if (isColor()) {
        const ColorRGBA rgb = CSBT2RGBA(storage.c);
        return rgb.r != 0 || rgb.g != 0 || rgb.b != 0 || rgb.a != 0;
    }
    return false;
}

namespace {
inline int parseHexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline bool parseHexByte(char c1, char c2, float &out) {
    int d1 = parseHexDigit(c1);
    int d2 = parseHexDigit(c2);
    if (d1 < 0 || d2 < 0) return false;
    out = static_cast<float>((d1 << 4) | d2);
    return true;
}
} // namespace

Color Value::asColor() const {
    if (isString()) {
        std::string_view sv = StringPool::get(storage.stringId);
        if (!sv.empty() && sv[0] == '#') {
            if (sv.size() == 4) {
                int r_digit = parseHexDigit(sv[1]);
                int g_digit = parseHexDigit(sv[2]);
                int b_digit = parseHexDigit(sv[3]);
                if (r_digit >= 0 && g_digit >= 0 && b_digit >= 0) {
                    float r = static_cast<float>((r_digit << 4) | r_digit);
                    float g = static_cast<float>((g_digit << 4) | g_digit);
                    float b = static_cast<float>((b_digit << 4) | b_digit);
                    return RGBA2CSBO({r, g, b, 255.0f});
                }
                return {0, 0, 0, 0};
            } else if (sv.size() == 7) {
                float r, g, b;
                if (parseHexByte(sv[1], sv[2], r) &&
                    parseHexByte(sv[3], sv[4], g) &&
                    parseHexByte(sv[5], sv[6], b)) {
                    return RGBA2CSBO({r, g, b, 255.0f});
                }
                return {0, 0, 0, 0};
            } else {
                return {0, 0, 0, 0};
            }
        }
    }
    const double RGBA = asDouble();
    return RGBA2CSBO({static_cast<float>(static_cast<unsigned int>(RGBA / 0x10000) % 0x100),
                      static_cast<float>(static_cast<unsigned int>(RGBA / 0x100) % 0x100),
                      static_cast<float>(static_cast<unsigned int>(RGBA) % 0x100),
                      static_cast<float>(static_cast<unsigned int>(RGBA / 0x1000000) % 0x100)});
}

ColorRGBA Value::asRGB() const {
    if (isString()) {
        std::string_view sv = StringPool::get(storage.stringId);
        if (!sv.empty() && sv[0] == '#') {
            if (sv.size() == 4) {
                int r_digit = parseHexDigit(sv[1]);
                int g_digit = parseHexDigit(sv[2]);
                int b_digit = parseHexDigit(sv[3]);
                if (r_digit >= 0 && g_digit >= 0 && b_digit >= 0) {
                    float r = static_cast<float>((r_digit << 4) | r_digit);
                    float g = static_cast<float>((g_digit << 4) | g_digit);
                    float b = static_cast<float>((b_digit << 4) | b_digit);
                    return {r, g, b, 255.0f};
                }
                return {0, 0, 0, 255.0f};
            } else if (sv.size() == 7) {
                float r, g, b;
                if (parseHexByte(sv[1], sv[2], r) &&
                    parseHexByte(sv[3], sv[4], g) &&
                    parseHexByte(sv[5], sv[6], b)) {
                    return {r, g, b, 255.0f};
                }
                return {0, 0, 0, 255.0f};
            } else {
                return {0, 0, 0, 255.0f};
            }
        }
    }
    const double RGBA = asDouble();
    return {static_cast<float>(static_cast<unsigned int>(RGBA / 0x10000) % 0x100),
            static_cast<float>(static_cast<unsigned int>(RGBA / 0x100) % 0x100),
            static_cast<float>(static_cast<unsigned int>(RGBA) % 0x100),
            static_cast<float>(static_cast<unsigned int>(RGBA / 0x1000000) % 0x100)};
}

std::string_view Value::asStringView(std::string &scratch) const {
    if (isString()) {
        return StringPool::get(storage.stringId);
    }
    scratch = asString();
    return scratch;
}

bool Value::operator==(const Value &other) const {
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

    if (!std::all_of(svA.begin(), svA.end(), [](unsigned char c) { return (std::isspace(c) && c != '\t'); }) &&
        !std::all_of(svB.begin(), svB.end(), [](unsigned char c) { return (std::isspace(c) && c != '\t'); })) {
        if (isNumeric() && other.isNumeric() && !isNaN() && !other.isNaN()) {
            return asDouble() == other.asDouble();
        }
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
