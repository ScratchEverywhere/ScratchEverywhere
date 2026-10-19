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
#ifdef __XBOX__
    float renderX = 0.0f;
    float renderY = 0.0f;
    float renderScaleX = 1.0f;
    float renderScaleY = 1.0f;
    float renderRotation = 0.0f;

    float oldX = -999999.0f;
    float oldY = -999999.0f;
    float oldSize = -999999.0f;
    float oldRotation = -999999.0f;
#else
    float renderX;
    float renderY;
    float renderScaleX;
    float renderScaleY;
    float renderRotation;

    float oldX, oldY;
    float oldSize, oldRotation;
#endif
    int oldCostumeID = -1;
    bool forceUpdate = false;
};
