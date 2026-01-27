#include "image.hpp"
#include "speech_manager_gl2d.hpp"
#include <audio.hpp>
#include <fat.h>
#include <filesystem.h>
#include <gl2d.h>
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
    speechManager = new SpeechManagerGL2D();
    return true;
}

void Render::deInit() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }
    Image::cleanupImages();
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
    if (shouldFlush) Image::FlushImages();
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

void Render::penMove(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Render::penDot(Sprite *sprite) {
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
        Sprite *sprite = *it;
        if (!sprite->visible) continue;

        auto imgFind = images.find(sprite->costumes[sprite->currentCostume].id);
        if (imgFind != images.end()) {
            imagePAL8 &data = imgFind->second;
            glImage *image = &data.image;
            glBindTexture(GL_TEXTURE_2D, data.textureID);
            imgFind->second.freeTimer = data.maxFreeTimer;

            // Set sprite dimensions
            sprite->spriteWidth = data.originalWidth >> 1;
            sprite->spriteHeight = data.originalHeight >> 1;
            // TODO: put this in calculateRenderPosition() for all platforms since they all do this anyway
            sprite->rotationCenterX = sprite->costumes[sprite->currentCostume].rotationCenterX;
            sprite->rotationCenterY = sprite->costumes[sprite->currentCostume].rotationCenterY;
            if (sprite->ghostEffect > 75) continue;

            calculateRenderPosition(sprite, sprite->costumes[sprite->currentCostume].isSVG);

            int renderScale = sprite->renderInfo.renderScaleY * (1 << 12);
            if (data.scaleX != 1 << 12 || data.scaleY != 1 << 12) {
                renderScale = (renderScale * data.scaleX) >> 12;
            }

            // Do rotation
            int16_t renderRotation = 0;
            GL_FLIP_MODE flip = GL_FLIP_NONE;
            if (sprite->rotationStyle == sprite->ALL_AROUND && sprite->rotation != 90) {
                // convert Scratch rotation to whatever tf this is (-32768 to 32767)
                renderRotation = ((sprite->rotation - 90) * 91);
            } else if (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0) {
                flip = GL_FLIP_H;
            }

            glSpriteRotateScale(sprite->renderInfo.renderX, sprite->renderInfo.renderY, renderRotation, renderScale, flip, image);

            // draw collision points (debug)
            // auto collisionPoints = Scratch::getCollisionPoints(sprite);
            // for (const auto &point : collisionPoints) {
            //     int drawX = (int)((point.first * Render::renderScale) + SCREEN_HALF_WIDTH);
            //     int drawY = (int)((-point.second * Render::renderScale) + SCREEN_HALF_HEIGHT);
            //     glBoxFilled(
            //         drawX - 1, drawY - 1,
            //         drawX + 1, drawY + 1,
            //         RGB15(31, 0, 0)); // Red box
            // }
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
    Image::FlushImages();
    SoundPlayer::flushAudio();
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {

    glBoxFilled(x - w / 2, y - h / 2, x + w / 2, y + h / 2, Math::color(colorR, colorG, colorB, colorA));
}

bool Render::appShouldRun() {
    return true;
}
