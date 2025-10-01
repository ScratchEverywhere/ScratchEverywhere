#include "../scratch/render.hpp"
#include <filesystem.h>
#include <gl2d.h>
#include <nds.h>
#include <nf_lib.h>

// Static member initialization
std::chrono::_V2::system_clock::time_point Render::startTime;
std::chrono::_V2::system_clock::time_point Render::endTime;
bool Render::debugMode = false;
bool Render::hasFrameBegan = false;
Render::RenderModes Render::renderMode = Render::RenderModes::TOP_SCREEN_ONLY;
std::unordered_map<std::string, TextObject *> Render::monitorTexts;
std::vector<Monitor> Render::visibleVariables;

#define SCREEN_WIDTH 256
#define BOTTOM_SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192

#define SCREEN_HALF_WIDTH 132.5
#define SCREEN_HALF_HEIGHT 96

bool Render::Init() {
    cpuStartTiming(0);
    consoleDemoInit();
    if (!nitroFSInit(NULL)) {
        Log::logError("NitroFS Could not initialize!");
        while (1)
            swiWaitForVBlank();
    }
    videoSetMode(MODE_0_3D);
    glScreen2D();

    return true;
}

void Render::deInit() {
}

void *Render::getRenderer() {
    return nullptr;
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
}

void Render::endFrame(bool shouldFlush) {
}

int Render::getWidth() {
    return 0;
}

int Render::getHeight() {
    return 0;
}

void Render::renderSprites() {
    glBegin2D();
    glClearColor(31, 31, 31, 31);

    std::vector<Sprite *> spritesByLayer = sprites;
    std::sort(spritesByLayer.begin(), spritesByLayer.end(),
              [](const Sprite *a, const Sprite *b) {
                  // Stage sprite always comes first
                  if (a->isStage && !b->isStage) return true;
                  if (!a->isStage && b->isStage) return false;
                  // Otherwise sort by layer
                  return a->layer < b->layer;
              });

    for (auto &sprite : sprites) {
        if (!sprite->visible || sprite->isStage) continue;
        const int renderX = static_cast<int>(sprite->xPosition);
        const int renderY = static_cast<int>(sprite->yPosition * -1);
        sprite->spriteWidth = 6;
        sprite->spriteHeight = 6;

        glBoxFilled(renderX + (SCREEN_HALF_WIDTH - 3), renderY + (SCREEN_HALF_HEIGHT - 3), renderX + (SCREEN_HALF_WIDTH + 3), renderY + (SCREEN_HALF_HEIGHT + 3), RGB15(0, 0, 0));
    }

    glEnd2D();
    glFlush(0);
    swiWaitForVBlank();
}

void Render::renderVisibleVariables() {
}

void Render::drawBox(int w, int h, int x, int y, int colorR, int colorG, int colorB, int colorA) {
}

bool Render::appShouldRun() {
    return true;
}