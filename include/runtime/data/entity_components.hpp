#pragma once
#include <se_export.hpp>
#include <cstdint>

enum class Type : uint8_t {
    Value,
    Number,
    String,
    Boolean,
    Color
};

struct SE_EXPORT RenderInfo {
    float renderX;
    float renderY;
    float renderScaleX;
    float renderScaleY;
    float renderRotation;

    float oldX, oldY;
    float oldSize, oldRotation;
    int oldCostumeID = -1;
    bool forceUpdate = false;
};
