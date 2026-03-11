#include "os.hpp"
#include <pdcpp/pdnewlib.h>
#include <render.hpp>
#include <speech_manager.hpp>
#include <unordered_map>
#include <window.hpp>
#include <windowing/headless/window.hpp>

// Static member initialization
bool Render::debugMode = false;
bool Render::hasFrameBegan = false;
Render::RenderModes Render::renderMode = Render::RenderModes::TOP_SCREEN_ONLY;
std::unordered_map<std::string, std::pair<std::unique_ptr<TextObject>, std::unique_ptr<TextObject>>> Render::monitorTexts;
std::unordered_map<std::string, Render::ListMonitorRenderObjects> Render::listMonitors;
std::unordered_map<std::string, Monitor> Render::visibleVariables;
float Render::renderScale;

Window *globalWindow = nullptr;

std::vector<std::string> Render::logs;
int scrollOffset = 0;
constexpr int lineHeight = 18;

extern PlaydateAPI *pd;
LCDFont *font;

void renderScrollableLogs() {
    pd->graphics->clear(kColorWhite);
    pd->graphics->setFont(font);

    int y = -scrollOffset;
    for (const auto &line : Render::logs) {
        if (y > -lineHeight) pd->graphics->drawText(line.c_str(), line.length(), kUTF8Encoding, 2, y);
        y += lineHeight;
        if (y >= 240) break;
    }

    const unsigned int contentHeight = Render::logs.size() * lineHeight;
    const unsigned int scrollBarHeight = (240.0f / contentHeight) * 240;
    const float scrollRatio = (float)scrollOffset / (contentHeight - 240);
    pd->graphics->fillRect(398, scrollRatio * (240 - scrollBarHeight), 2, scrollBarHeight, kColorBlack);

    int maxScroll = contentHeight - 240;
    if (maxScroll < 0) maxScroll = 0;

    scrollOffset += pd->system->getCrankChange();
    if (scrollOffset < 0) scrollOffset = 0;
    else if (scrollOffset > maxScroll) scrollOffset = maxScroll;
}

bool Render::Init() {
    globalWindow = new WindowHeadless();
    globalWindow->init(0, 0, "");

    const char *err = nullptr;
    font = pd->graphics->loadFont("/System/Fonts/Asheville-Sans-14-Bold.pft", &err);
    if (font == nullptr) {
        Log::logError("Failed to load font: " + std::string(err));
    }

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

bool Render::createSpeechManager() {
    return false;
}

void Render::destroySpeechManager() {
}

SpeechManager *Render::getSpeechManager() {
    return nullptr;
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
}

void Render::endFrame(bool shouldFlush) {
}

bool Render::initPen() {
    return false;
}

void Render::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Render::penDotFast(Sprite *sprite) {
}

void Render::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Render::penDotAccurate(Sprite *sprite) {
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
    renderScrollableLogs();
    // pd->system->drawFPS(0, 0);
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
}

bool Render::appShouldRun() {
    return true;
}
