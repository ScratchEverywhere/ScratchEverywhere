#include "sb1_squeak.hpp"
#include <cstring>
#include <log.hpp>

namespace {

enum class SqueakFieldType : uint8_t {
    Null = 1,
    True = 2,
    False = 3,
    SmallInt = 4,
    SmallInt16 = 5,
    LargeIntPositive = 6,
    LargeIntNegative = 7,
    Floating = 8,
    String = 9,
    Symbol = 10,
    Bytes = 11,
    Sound = 12,
    Bitmap = 13,
    Utf8 = 14,
    Array = 20,
    OrderedCollection = 21,
    Set = 22,
    IdentitySet = 23,
    Dictionary = 24,
    IdentityDictionary = 25,
    Color = 30,
    TranslucentColor = 31,
    Point = 32,
    Rectangle = 33,
    Form = 34,
    Squeak = 35,
    ObjectRef = 99
};

class ByteCursor {
  public:
    ByteCursor(const uint8_t *data, size_t size) : data(data), size(size) {}

    bool atEnd() const { return pos >= size; }
    size_t position() const { return pos; }
    void skip(size_t n) { pos += n; }

    uint8_t u8() { return pos < size ? data[pos++] : 0; }

    uint16_t u16be() {
        uint16_t v = (u8() << 8);
        v |= u8();
        return v;
    }

    int16_t i16be() { return static_cast<int16_t>(u16be()); }

    uint32_t u32be() {
        uint32_t v = u8();
        v = (v << 8) | u8();
        v = (v << 8) | u8();
        v = (v << 8) | u8();
        return v;
    }

    int32_t i32be() { return static_cast<int32_t>(u32be()); }

    double f64be() {
        uint64_t bits = 0;
        for (int i = 0; i < 8; i++)
            bits = (bits << 8) | u8();
        double out;
        std::memcpy(&out, &bits, sizeof(out));
        return out;
    }

    double largeInt() {
        int16_t count = i16be();
        double num = 0;
        double multiplier = 1;
        for (int i = 0; i < count; i++) {
            num += multiplier * u8();
            multiplier *= 256;
        }
        return num;
    }

    std::string asciiString() {
        uint32_t count = u32be();
        std::string out;
        out.reserve(count);
        for (uint32_t i = 0; i < count; i++)
            out.push_back(static_cast<char>(u8()));
        return out;
    }

    std::string utf8String() {
        uint32_t count = u32be();
        std::string out;
        out.reserve(count);
        for (uint32_t i = 0; i < count; i++)
            out.push_back(static_cast<char>(u8()));
        return out;
    }

    std::vector<uint8_t> bytes() {
        uint32_t count = u32be();
        std::vector<uint8_t> out(count);
        for (uint32_t i = 0; i < count; i++)
            out[i] = u8();
        return out;
    }

    std::vector<uint8_t> soundBytes() {
        uint32_t samples = u32be();
        std::vector<uint8_t> out(samples * 2);
        for (uint32_t i = 0; i < out.size(); i++)
            out[i] = u8();
        return out;
    }

    std::vector<uint32_t> bitmapWords() {
        uint32_t count = u32be();
        std::vector<uint32_t> out(count);
        for (uint32_t i = 0; i < count; i++)
            out[i] = u32be();
        return out;
    }

    uint32_t reference3be() {
        uint32_t a = u8(), b = u8(), c = u8();
        return (a << 16) | (b << 8) | c;
    }

    uint32_t opaqueColor() {
        uint32_t rgb = u32be();
        uint32_t a = 0xff;
        uint32_t r = (rgb >> 22) & 0xff;
        uint32_t g = (rgb >> 12) & 0xff;
        uint32_t b = (rgb >> 2) & 0xff;
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    uint32_t translucentColor() {
        size_t startPos = pos;
        uint32_t rgb = u32be();
        uint32_t a = data[startPos];
        pos = startPos + 5;
        uint32_t r = (rgb >> 22) & 0xff;
        uint32_t g = (rgb >> 12) & 0xff;
        uint32_t b = (rgb >> 2) & 0xff;
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

  private:
    const uint8_t *data;
    size_t size;
    size_t pos = 0;
};

SqFieldPtr makeScalar(SqField::Kind kind) {
    auto f = std::make_shared<SqField>();
    f->kind = kind;
    return f;
}

SqFieldPtr readField(ByteCursor &cursor) {
    uint8_t classId = cursor.u8();

    switch (static_cast<SqueakFieldType>(classId)) {
    case SqueakFieldType::Null:
        return makeScalar(SqField::Kind::Null);
    case SqueakFieldType::True: {
        auto f = makeScalar(SqField::Kind::Bool);
        f->boolValue = true;
        return f;
    }
    case SqueakFieldType::False:
        return makeScalar(SqField::Kind::Bool);
    case SqueakFieldType::SmallInt: {
        auto f = makeScalar(SqField::Kind::Number);
        f->numberValue = cursor.i32be();
        return f;
    }
    case SqueakFieldType::SmallInt16: {
        auto f = makeScalar(SqField::Kind::Number);
        f->numberValue = cursor.i16be();
        return f;
    }
    case SqueakFieldType::LargeIntPositive:
    case SqueakFieldType::LargeIntNegative: {
        auto f = makeScalar(SqField::Kind::Number);
        f->numberValue = cursor.largeInt();
        return f;
    }
    case SqueakFieldType::Floating: {
        auto f = makeScalar(SqField::Kind::Number);
        f->numberValue = cursor.f64be();
        return f;
    }
    case SqueakFieldType::String:
    case SqueakFieldType::Symbol: {
        auto f = makeScalar(SqField::Kind::String);
        f->stringValue = cursor.asciiString();
        return f;
    }
    case SqueakFieldType::Utf8: {
        auto f = makeScalar(SqField::Kind::String);
        f->stringValue = cursor.utf8String();
        return f;
    }
    case SqueakFieldType::Bytes: {
        auto f = makeScalar(SqField::Kind::Bytes);
        f->bytesValue = cursor.bytes();
        return f;
    }
    case SqueakFieldType::Sound: {
        auto f = makeScalar(SqField::Kind::Sound);
        f->bytesValue = cursor.soundBytes();
        return f;
    }
    case SqueakFieldType::Bitmap: {
        auto f = makeScalar(SqField::Kind::Bitmap);
        f->bitmapValue = cursor.bitmapWords();
        return f;
    }
    case SqueakFieldType::Color: {
        auto f = makeScalar(SqField::Kind::Number);
        f->numberValue = cursor.opaqueColor();
        return f;
    }
    case SqueakFieldType::TranslucentColor: {
        auto f = makeScalar(SqField::Kind::Number);
        f->numberValue = cursor.translucentColor();
        return f;
    }
    case SqueakFieldType::ObjectRef: {
        auto f = makeScalar(SqField::Kind::Reference);
        f->referenceIndex = cursor.reference3be();
        return f;
    }
    case SqueakFieldType::Array:
    case SqueakFieldType::OrderedCollection:
    case SqueakFieldType::Set:
    case SqueakFieldType::IdentitySet: {
        auto f = makeScalar(SqField::Kind::Array);
        f->classId = classId;
        uint32_t count = cursor.u32be();
        f->items.reserve(count);
        for (uint32_t i = 0; i < count; i++)
            f->items.push_back(readField(cursor));
        return f;
    }
    case SqueakFieldType::Dictionary:
    case SqueakFieldType::IdentityDictionary: {
        auto f = makeScalar(SqField::Kind::Array);
        f->classId = classId;
        uint32_t count = cursor.u32be() * 2;
        f->items.reserve(count);
        for (uint32_t i = 0; i < count; i++)
            f->items.push_back(readField(cursor));
        return f;
    }
    case SqueakFieldType::Point: {
        auto f = makeScalar(SqField::Kind::Array);
        f->classId = classId;
        for (int i = 0; i < 2; i++)
            f->items.push_back(readField(cursor));
        return f;
    }
    case SqueakFieldType::Rectangle: {
        auto f = makeScalar(SqField::Kind::Array);
        f->classId = classId;
        for (int i = 0; i < 4; i++)
            f->items.push_back(readField(cursor));
        return f;
    }
    case SqueakFieldType::Form: {
        auto f = makeScalar(SqField::Kind::Array);
        f->classId = classId;
        for (int i = 0; i < 5; i++)
            f->items.push_back(readField(cursor));
        return f;
    }
    case SqueakFieldType::Squeak: {
        auto f = makeScalar(SqField::Kind::Array);
        f->classId = classId;
        for (int i = 0; i < 6; i++)
            f->items.push_back(readField(cursor));
        return f;
    }
    default:
        break;
    }

    if (classId < static_cast<uint8_t>(SqueakFieldType::ObjectRef)) {
        return makeScalar(SqField::Kind::Null);
    }

    uint8_t version = cursor.u8();
    uint8_t size = cursor.u8();
    auto obj = makeScalar(SqField::Kind::Object);
    obj->classId = classId;
    obj->version = version;
    obj->items.reserve(size);
    for (uint8_t i = 0; i < size; i++)
        obj->items.push_back(readField(cursor));
    return obj;
}

void fixReferencesIn(std::vector<SqFieldPtr> &items, const std::vector<SqFieldPtr> &table) {
    for (auto &item : items) {
        if (item && item->kind == SqField::Kind::Reference) {
            int idx = item->referenceIndex - 1;
            if (idx >= 0 && static_cast<size_t>(idx) < table.size()) item = table[idx];
            else item = makeScalar(SqField::Kind::Null);
        }
    }
}

} // namespace

SqFieldPtr parseSb1File(const std::vector<uint8_t> &buffer) {
    ByteCursor header(buffer.data(), buffer.size());

    std::string version;
    for (int i = 0; i < 10; i++)
        version.push_back(static_cast<char>(header.u8()));
    if (version != "ScratchV01" && version != "ScratchV02") {
        Log::logWarning("Not a Scratch 1.x project (bad signature): " + version);
        return nullptr;
    }
    uint32_t infoByteLength = header.u32be();

    const auto readSubHeader = [&]() -> bool {
        std::string objS, stch;
        for (int i = 0; i < 4; i++)
            objS.push_back(static_cast<char>(header.u8()));
        uint8_t objSValue = header.u8();
        for (int i = 0; i < 4; i++)
            stch.push_back(static_cast<char>(header.u8()));
        uint8_t stchValue = header.u8();
        header.u32be();
        return objS == "ObjS" && objSValue == 1 && stch == "Stch" && stchValue == 1;
    };

    if (!readSubHeader()) {
        Log::logWarning("Invalid .sb info section header.");
        return nullptr;
    }
    header.skip(infoByteLength - 14);

    if (!readSubHeader()) {
        Log::logWarning("Invalid .sb data section header.");
        return nullptr;
    }

    size_t dataStart = header.position();
    ByteCursor dataCursor(buffer.data() + dataStart, buffer.size() - dataStart);

    std::vector<SqFieldPtr> table;
    while (!dataCursor.atEnd())
        table.push_back(readField(dataCursor));

    for (auto &entry : table) {
        if (!entry) continue;
        fixReferencesIn(entry->items, table);
    }

    if (table.empty() || !table[0] || table[0]->kind != SqField::Kind::Object) {
        Log::logWarning(".sb data section has no root Stage object.");
        return nullptr;
    }
    return table[0];
}
