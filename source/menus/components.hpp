#pragma once

#include "image.hpp"
#include "os.hpp"
#include <clay.h>
#include <map>
#include <string>

#ifdef RENDERER_CITRO2D
#include <citro2d.h>
#elif defined(RENDERER_SDL2)
#include <renderers/sdl2/clay_renderer.h>
#endif

class MenuManager;

namespace components {
#define DEFAULT_TEXT_CONFIG CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = 16})

#ifdef RENDERER_SDL2
extern SDL2_Font fonts[2];
#elif defined(RENDERER_CITRO2D)
extern std::map<unsigned int, C2D_Font> fonts;
#endif

extern uint16_t FONT_ID_BODY_16;
extern uint16_t FONT_ID_BODY_BOLD_48;

struct ProjectInfo {
    std::string name;
    std::string path;
    std::optional<std::string> description;
    std::optional<std::string> source;
};

class Sidebar {
  private:
    static const std::array<std::string, 3> tabs;

    struct HoverData {
        MenuManager *menuManager;
        std::string tab;
    };
    std::map<std::string, HoverData> hoverData;

    static constexpr unsigned int animationDuration = 150; // ms
    std::string selected = "";
    Timer animationTimer;
    std::string unSelectedTab = "";

    std::map<std::string, std::unique_ptr<Image>> images;
    std::unique_ptr<Image> nextTabImage;
    std::unique_ptr<Image> previousTabImage;

    void renderItem(const std::string tab);

  public:
    MenuManager *menuManager = nullptr;

    Sidebar();
    void render();
};

struct ProjectHoverData {
    MenuManager *menuManager;
    const ProjectInfo *projectInfo;
};

extern std::vector<ProjectHoverData> projectHoverData;

void renderProjectListItem(const ProjectInfo &projectInfo, void *image, unsigned int i, Clay_SizingAxis width, float textScroll, MenuManager *menuManager, bool selected);

std::unique_ptr<Image> getControllerImage(const std::string button);
} // namespace components
