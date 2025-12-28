#include <render.hpp>
#include <window.hpp>
#include <windowing/headless/window.hpp>

// Static member initialization
std::chrono::_V2::system_clock::time_point Render::startTime;
std::chrono::_V2::system_clock::time_point Render::endTime;
bool Render::debugMode = false;
bool Render::hasFrameBegan = false;
Render::RenderModes Render::renderMode = Render::RenderModes::TOP_SCREEN_ONLY;
std::unordered_map<std::string, std::pair<TextObject *, TextObject *>> Render::monitorTexts;
std::unordered_map<std::string, Render::ListMonitorRenderObjects> Render::listMonitors;
std::vector<Monitor> Render::visibleVariables;
float Render::renderScale;

Window *globalWindow = nullptr;

bool Render::Init() {
    globalWindow = new WindowHeadless();
    globalWindow->init(0, 0, "");
    return true;
}

void Render::deInit() {
    if (globalWindow) {
        globalWindow->cleanup();
        delete globalWindow;
        globalWindow = nullptr;
    }
}

void *Render::getRenderer() {
    return nullptr;
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
}

void Render::endFrame(bool shouldFlush) {
}

bool Render::initPen() {
    return false;
}

void Render::penMove(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Render::penDot(Sprite *sprite) {
}

void Render::penStamp(Sprite *sprite) {
}

void Render::penClear() {
}

int Render::getWidth() {
    return 0;
}

int Render::getHeight() {
    return 0;
}

void Render::renderSprites() {
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
}

bool Render::appShouldRun() {
    return true;
}
