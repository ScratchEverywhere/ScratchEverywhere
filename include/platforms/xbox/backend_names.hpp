#pragma once
#ifdef __XBOX__
#include <string>

namespace XboxBackend {
inline std::string rendererName(std::string name) {
    if (name == "NV2A" || name == "SDL3") return "Xbox";
    return name;
}

inline std::string windowingName(std::string name) {
    if (name == "NV2A" || name == "SDL3") return "Xbox";
    return name;
}
}
#endif
