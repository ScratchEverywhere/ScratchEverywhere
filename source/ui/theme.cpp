#include "theme.hpp"

namespace UI {

std::array<std::string, 2> Theme::fonts;
std::array<Style, Theme::MAX_STYLES> Theme::styles;

// just a default theme for now, later i want this to load from a file
void Theme::load() {
    // #soretro font for low resolution devices
#ifdef __NDS__
    fonts[0] = "gfx/ingame/fonts/Grand9KPixel";
    fonts[1] = "gfx/ingame/fonts/Grand9KPixel";
#else
    fonts[0] = "gfx/menu/RedditSansFudge-Bold";
    fonts[1] = "gfx/menu/RedditSansFudge-Regular";
#endif

    styles[StyleId::None] = {
        .normal = {.color = {0, 0, 0, 0}, .round = 0},
        .has = 0,
    };
    styles[StyleId::Tab] = {
        .normal = {.color = {92, 60, 90, 255}, .round = 12},
        .focused = {.color = {206, 138, 184, 255}, .round = 12},
        .pressed = {.color = {118, 75, 114, 255}, .round = 12},
        .has = StateFlag::Pressed | StateFlag::Focused,
    };
    styles[StyleId::TabPressed] = {
        .normal = {.color = {118, 75, 114, 255}, .round = 8},
    };
    styles[StyleId::ProjectButton] = {
        .normal = {.color = {225, 225, 235, 255}, .round = 8, .borderColor = {92, 60, 90, 255}, .borderThickness = 12},
        .focused = {.color = {225, 225, 235, 255}, .round = 8, .borderColor = {243, 143, 235, 255}, .borderThickness = 12},
        .pressed = {.color = {225, 225, 235, 255}, .round = 8, .borderColor = {0, 0, 0, 255}, .borderThickness = 12},
        .has = StateFlag::Pressed | StateFlag::Focused,
    };
    styles[StyleId::Button] = {
        .normal = {.color = {92, 60, 90, 255}, .round = 8},
        .focused = {.color = {156, 105, 156, 255}, .round = 8},
        .pressed = {.color = {79, 42, 76, 255}, .round = 8},
        .has = StateFlag::Pressed | StateFlag::Focused,
    };
    styles[StyleId::TransparentButton] = {
        .normal = {.color = {0, 0, 0, 0}, .round = 8},
        .focused = styles[StyleId::Button].focused,
        .pressed = styles[StyleId::Button].pressed,
        .has = StateFlag::Pressed | StateFlag::Focused,
    };
    styles[StyleId::AskBox] = {
        .normal = {.color = {255, 255, 255, 255}, .round = 4, .borderColor = {217, 217, 217, 255}, .borderThickness = 3}};
    styles[StyleId::AskTextInput] = {
        .normal = {.color = {255, 255, 255, 255}, .round = 15, .borderColor = {217, 217, 217, 255}, .borderThickness = 2},
        .focused = {.color = {255, 255, 255, 255}, .round = 15, .borderColor = {118, 75, 114, 255}, .borderThickness = 4},
        .pressed = {.color = {255, 255, 255, 255}, .round = 15, .borderColor = {225, 225, 235, 255}, .borderThickness = 8},
        .has = StateFlag::Pressed | StateFlag::Focused,
    };
    styles[StyleId::AskConfirm] = {
        .normal = {.color = {118, 75, 114, 255}, .round = 15},
        .focused = {.color = {225, 225, 235, 255}, .round = 15},
        .pressed = {.color = {79, 42, 76, 255}, .round = 15},
        .has = StateFlag::Pressed | StateFlag::Focused,
    };
    styles[StyleId::KeyboardBackground] = {
        .normal = {.color = {139, 139, 139, 255}},
    };
    styles[StyleId::KeyboardKey] = {
        .normal = {.color = {198, 198, 198, 255}},
        .focused = {.color = {167, 156, 170, 255}},
        .pressed = {.color = {129, 110, 128, 255}},
        .has = StateFlag::Pressed | StateFlag::Focused,
    };
}

} // namespace UI