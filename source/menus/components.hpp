#pragma once

#include "image.hpp"
#include <clay.h>
#include <map>
#include <memory>
#include <string>

#ifdef RENDERER_CITRO2D
#include <citro2d.h>
#elif defined(RENDERER_SDL2)
#include <renderers/sdl2/clay_renderer.hpp>
#elif defined(RENDERER_SDL3)
#include <renderers/sdl3/clay_renderer.hpp>
#elif defined(RENDERER_SDL1)
#include <renderers/sdl1/clay_renderer.hpp>
#endif

class MenuManager;

namespace components {
#define DEFAULT_TEXT_CONFIG CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = 16})

#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3) || defined(RENDERER_SDL1)
extern SDL_Font fonts[2];
#elif defined(RENDERER_CITRO2D)
extern std::map<unsigned int, C2D_Font> fonts;
#endif

extern uint16_t FONT_ID_BODY_16;
extern uint16_t FONT_ID_BODY_BOLD_48;

struct ProjectInfo {
    std::string name;
    std::string path;
    std::string displayName;
    std::optional<std::string> description;
    std::optional<std::string> source;
    std::shared_ptr<Image> image = nullptr;
    bool unpacked = false;
};

class Sidebar {
  private:
    static const std::array<std::string, 3> tabs;

    struct HoverData {
        MenuManager *&menuManager;
        std::string tab;
    };
    std::map<std::string, HoverData *> hoverDatas;

    static constexpr unsigned int animationDuration = 150; // ms
    std::string selected = "";
    Timer animationTimer;
    std::string unSelectedTab = "";

    std::map<std::string, std::shared_ptr<Image>> images;
    std::shared_ptr<Image> nextTabImage;
    std::shared_ptr<Image> previousTabImage;

    void renderItem(const std::string tab);

  public:
    MenuManager *menuManager = nullptr;

    Sidebar();
    ~Sidebar();
    void render();
};

struct ProjectHoverData {
    MenuManager *menuManager;
    const ProjectInfo *projectInfo;
};

void renderProjectListItem(const ProjectInfo &projectInfo, std::shared_ptr<Image> image, unsigned int i, Clay_SizingAxis width, MenuManager *menuManager, bool selected);

std::shared_ptr<Image> getControllerImage(const std::string button);
} // namespace components
