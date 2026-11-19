#pragma once
#include <algorithm>
#include <cmath>

#if defined (__DOS__)
#include "dos_math.hpp"
#endif

namespace PixelSnap {

inline float gridScale = 1.0f;
inline float gridOriginX = 0.0f;
inline float gridOriginY = 0.0f;

inline void setGrid(float scale, float originX, float originY) {
    gridScale = scale > 0.0f ? scale : 1.0f;
    gridOriginX = originX;
    gridOriginY = originY;
}

inline void span(float pos, float size, float &outPos, float &outSize, bool vertical = false) {
    const float gs = gridScale;
    if (size >= gs && gs > 0.0f) {
        const float origin = vertical ? gridOriginY : gridOriginX;
    #ifndef __DOS__
        const float start = std::round((pos - origin) / gs);
        const float end = std::round((pos + size - origin) / gs);
    #else
        const float start = DosMath::round((pos - origin) / gs);
        const float end = DosMath::round((pos + size - origin) / gs);
    #endif
        outPos = start * gs + origin;
        outSize = std::max(1.0f, end - start) * gs;
        return;
    }
#ifndef __DOS__
    const float start = std::round(pos);
    const float end = std::round(pos + size);
#else
    const float start = DosMath::round(pos);
    const float end = DosMath::round(pos + size);
#endif
    outPos = start;
    outSize = std::max(1.0f, end - start);
}

inline bool unrotated(double radians) {
    const double turn = 6.283185307179586;
    const double r = std::fabs(std::fmod(radians, turn));
    return r < 1e-4 || (turn - r) < 1e-4;
}
} // namespace PixelSnap
