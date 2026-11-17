#pragma once
#include <se_export.hpp>
#include <string>


namespace TouchControls {
    enum class Mode {
        Auto,
        On,
        Off
    };

    void init();
    void cleanup(); // release control textures
    void update();
    void render();
    bool isPointOverControls(int x, int y); // when touching a control
    bool consumePauseRequest(); // on pause press
    bool isVisible();

    Mode modeFromString(const std::string &value);
    const char *modeToString(Mode mode);
} // namespace TouchControls
