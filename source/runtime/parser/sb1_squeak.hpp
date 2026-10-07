#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct SqField;
using SqFieldPtr = std::shared_ptr<SqField>;

struct SqField {
    enum class Kind {
        Null,
        Bool,
        Number,
        String,
        Bytes,
        Sound,
        Bitmap,
        Array,
        Object,
        Reference
    };

    Kind kind = Kind::Null;
    int classId = 0;
    int version = 0;
    bool boolValue = false;
    double numberValue = 0;
    std::string stringValue;
    std::vector<uint8_t> bytesValue;
    std::vector<uint32_t> bitmapValue;
    std::vector<SqFieldPtr> items;
    int referenceIndex = 0;

    bool isNil() const { return kind == Kind::Null; }

    SqFieldPtr field(size_t index) const { return index < items.size() ? items[index] : nullptr; }
};

SqFieldPtr parseSb1File(const std::vector<uint8_t> &buffer);
