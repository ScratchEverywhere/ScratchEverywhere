
#pragma once
#include "types.hpp"
#include <array>
#include <string>

namespace UI {

enum StyleId : uint8_t {
    None,
    Tab,
    TabPressed,
    ProjectButton,
    Button,
    TransparentButton,
    AskBox,
    AskTextInput,
    AskConfirm,
};

struct StateStyle {
    Color color;
    Color textColor;
    uint8_t round = 0;
    Color borderColor;
    uint8_t borderThickness = 0;
};

struct Style {
    StateStyle normal, focused, pressed;
    uint8_t has = 0;

    const StateStyle &get(uint8_t state) const {
        if ((state & StateFlag::Pressed) && (has & StateFlag::Pressed)) return pressed;
        if ((state & (StateFlag::Focused | StateFlag::Pressed)) && (has & StateFlag::Focused)) return focused;
        return normal;
    }
};

class Theme {
  public:
    static constexpr size_t MAX_STYLES = 32;

    Theme() = delete;

    // TODO: load from a file
    static void load();

    static const Style &get(uint8_t id) {
        return styles[id < MAX_STYLES ? id : static_cast<uint8_t>(StyleId::None)];
    }

    static std::array<std::string, 2> fonts;
    static std::array<Style, MAX_STYLES> styles;
};

} // namespace UI
