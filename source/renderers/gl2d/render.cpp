#include "speech_manager_gl2d.hpp"
#include <audio.hpp>
#include <fat.h>
#include <filesystem.h>
#include <gl2d.h>
#include <image_gl2d.hpp>
#include <input.hpp>
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <render.hpp>
#include <window.hpp>
#include <windowing/nds/window.hpp>

// Static member initialization
bool Render::debugMode = false;
bool Render::hasFrameBegan = false;
float Render::renderScale = 1.0f;
Render::RenderModes Render::renderMode = Render::RenderModes::TOP_SCREEN_ONLY;
std::unordered_map<std::string, std::pair<std::unique_ptr<TextObject>, std::unique_ptr<TextObject>>> Render::monitorTexts;
std::unordered_map<std::string, Render::ListMonitorRenderObjects> Render::listMonitors;
std::vector<Monitor> Render::visibleVariables;

Window *globalWindow = nullptr;
SpeechManagerGL2D *speechManager = nullptr;

#define SCREEN_WIDTH 256
#define BOTTOM_SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192

#define SCREEN_HALF_WIDTH 132.5
#define SCREEN_HALF_HEIGHT 96

bool Render::Init() {
    globalWindow = new WindowNDS();
    if (!globalWindow->init(256, 192, "Scratch Everywhere!")) {
        delete globalWindow;
        globalWindow = nullptr;
        return false;
    }
    return true;
}

void Render::deInit() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }
    TextObject::cleanupText();

    if (globalWindow) {
        globalWindow->cleanup();
        delete globalWindow;
        globalWindow = nullptr;
    }
}

void *Render::getRenderer() {
    return nullptr;
}

SpeechManager *Render::getSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerGL2D();
    return speechManager;
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (hasFrameBegan) return;
    SoundPlayer::flushAudio();
    glBegin2D();
    int r5 = colorR >> 3;
    int g5 = colorG >> 3;
    int b5 = colorB >> 3;
    glClearColor(r5, g5, b5, 31);
    hasFrameBegan = true;
}

void Render::endFrame(bool shouldFlush) {
    glEnd2D();
    glFlush(GL_TRANS_MANUALSORT);
    hasFrameBegan = false;
}

int Render::getWidth() {
    return SCREEN_WIDTH;
}

int Render::getHeight() {
    return SCREEN_HEIGHT;
}

bool Render::initPen() {
    return false;
}

void Render::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Render::penDotAccurate(Sprite *sprite) {
}

void Render::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Render::penDotFast(Sprite *sprite) {
}

void Render::penStamp(Sprite *sprite) {
}

void Render::penClear() {
}

void Render::renderSprites() {
    if (renderMode == BOTTOM_SCREEN_ONLY) lcdMainOnBottom();
    glBegin2D();
    glClearColor(31, 31, 31, 31);

    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;
        if (!currentSprite->visible) continue;

        auto imgFind = Scratch::costumeImages.find(currentSprite->costumes[currentSprite->currentCostume].fullName);
        if (imgFind != Scratch::costumeImages.end()) {
            Image_GL2D *image = reinterpret_cast<Image_GL2D *>(imgFind->second.get());
            glBindTexture(GL_TEXTURE_2D, image->textureID);

            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            calculateRenderPosition(currentSprite, isSVG);
            if (!currentSprite->visible) continue;

            ImageRenderParams params;
            params.centered = true;
            params.x = currentSprite->renderInfo.renderX;
            params.y = currentSprite->renderInfo.renderY;
            params.rotation = currentSprite->renderInfo.renderRotation;
            params.scale = currentSprite->renderInfo.renderScaleY;
            params.flip = (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0);

            image->render(params);
        }
    }

    if (speechManager) {
        speechManager->render();
    }

    if (Input::mousePointer.isMoving) {
        glBoxFilled((Input::mousePointer.x * Render::renderScale) + (SCREEN_HALF_WIDTH - 3), (-Input::mousePointer.y * Render::renderScale) + (SCREEN_HALF_HEIGHT - 3),
                    (Input::mousePointer.x * Render::renderScale) + (SCREEN_HALF_WIDTH + 3), (-Input::mousePointer.y * Render::renderScale) + (SCREEN_HALF_HEIGHT + 3),
                    RGB15(0, 0, 15));
        Input::mousePointer.x = std::clamp((float)Input::mousePointer.x, -Scratch::projectWidth * 0.5f, Scratch::projectWidth * 0.5f);
        Input::mousePointer.y = std::clamp((float)Input::mousePointer.y, -Scratch::projectHeight * 0.5f, Scratch::projectHeight * 0.5f);
    }

    renderVisibleVariables();
    glEnd2D();
    glFlush(GL_TRANS_MANUALSORT);
    SoundPlayer::flushAudio();
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {

    glBoxFilled(x - w / 2, y - h / 2, x + w / 2, y + h / 2, Math::color(colorR, colorG, colorB, colorA));
}

bool Render::appShouldRun() {
    return true;
}
