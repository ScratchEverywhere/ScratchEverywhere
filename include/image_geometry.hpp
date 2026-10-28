#pragma once

namespace ImageGeometry {
inline void updateRotationCenter(double &center, double &previousOffset, float offset, double defaultCenter) {
    if (center == -6767.6767) center = defaultCenter;
    else center += previousOffset - offset;
    previousOffset = offset;
}
}
