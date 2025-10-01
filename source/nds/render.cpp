#include "../scratch/render.hpp"
#include <filesystem.h>
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

//This image stuff is going to be tough; the DS uses memory "banks" of fixed sizes that might not be the best use of space when it comes to images.
//The vram banks add up to 656 KB.
uint8_t freeVram = 0;

bool Render::Init() {
    consoleDemoInit();
    if (!nitroFSInit(NULL)) {
        Log::logError("NitroFS Could not initialize!");
        while (1)
            swiWaitForVBlank();
    }
    NF_Set2D(0, 0);

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
}

void Render::renderVisibleVariables() {
}

void Render::drawBox(int w, int h, int x, int y, int colorR, int colorG, int colorB, int colorA) {
gl2d.box(x,y,w,h,colorR,colorB,colorG,colorA);
}

bool Render::appShouldRun() {
    return true;
}
