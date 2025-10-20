#pragma once

#include "os.hpp"
#include <clay.h>
#include <map>
#include <string>

#ifdef SDL_BUILD
#include <SDL2/SDL.h>
#endif

#if defined(SDL_BUILD) && !defined(HAS_SDL2_FONT)
#include <SDL2/SDL_ttf.h>

typedef struct
{
    uint32_t fontId;
    TTF_Font *font;
} SDL2_Font;

#define HAS_SDL2_FONT
#endif

namespace components {
extern Clay_TextElementConfig *defaultTextConfig;

#ifdef SDL_BUILD
extern SDL2_Font fonts[2];

static constexpr unsigned int FONT_ID_BODY_16 = 0;
static constexpr unsigned int FONT_ID_BODY_BOLD_48 = 1;
#endif

class Sidebar {
  private:
    static constexpr std::string tabs[] = {"home", "projects", "settings"};

    static constexpr int animationDuration = 100; // ms
    std::string hovered = "";
    Timer hoverTimer;
    std::string unhoverTab = "";
    Timer unhoverTimer;

#ifdef SDL_BUILD
    std::map<std::string, SDL_Surface *> images;
#endif

    void renderItem(const std::string tab);

  public:
    Sidebar();
    ~Sidebar();
    void render();
};
} // namespace components
