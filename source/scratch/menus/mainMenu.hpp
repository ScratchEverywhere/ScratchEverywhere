#pragma once

#include "menu.hpp"

class MainMenu : public Menu {
    Clay_RenderCommandArray render() override;
};
