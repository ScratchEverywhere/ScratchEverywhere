#pragma once
#include <lunasvg.h>
#include <cmath>
#include <ryu/d2s.h>
#include <string>
#include <utility>

namespace Scratch2Svg {
// SB2 ignores exported viewport dimensions. Measure in the root's coordinate
// space, before its viewBox transform, so off-viewport drawings are retained.
// Return the crop origin for the costume rotation-center compensation.
inline std::pair<float, float> normalizeMeasurements(lunasvg::Document &document) {
    auto root = document.documentElement();
    const auto box = root.getBoundingBox();
    if (!std::isfinite(box.x) || !std::isfinite(box.y) || !std::isfinite(box.w) || !std::isfinite(box.h) ||
        box.w <= 0 || box.h <= 0) return {0, 0};
    const auto number = [](float value) {
        // Use the runtime's decimal formatter: NXDK's printf float conversions
        // are stubs, so snprintf("%g") silently produces invalid SVG lengths.
        char buffer[32];
        d2s_buffered(static_cast<double>(value), buffer);
        return std::string(buffer);
    };
    root.setAttribute("width", number(box.w));
    root.setAttribute("height", number(box.h));
    root.setAttribute("viewBox", number(box.x) + " " + number(box.y) + " " + number(box.w) + " " + number(box.h));
    document.forceLayout();
    return {box.x, box.y};
}
} // namespace Scratch2Svg
