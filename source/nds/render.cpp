#include "../scratch/render.hpp"
#include "image.hpp"
#include "swzl.hpp"
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
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankB(VRAM_B_TEXTURE);
    vramSetBankC(VRAM_C_TEXTURE);
    vramSetBankD(VRAM_D_TEXTURE);
    vramSetBankE(VRAM_E_TEX_PALETTE);
    vramSetBankF(VRAM_F_TEX_PALETTE);

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

        auto imgFind = images.find(sprite->costumes[sprite->currentCostume].id);
        if (imgFind != images.end()) {
            glImage *image = &imgFind->second.image;

            // Set sprite dimensions
            sprite->spriteWidth = image->width;
            sprite->spriteHeight = image->height;

            // TODO: look into making sprite->size a float or int for extra performance
            const uint16_t renderScale = ((static_cast<int>(sprite->size) << 12) / 100) >> 1;

            // Do rotation
            int16_t renderRotation = 0;
            GL_FLIP_MODE flip = GL_FLIP_NONE;
            if (sprite->rotationStyle == sprite->ALL_AROUND && sprite->rotation != 90) {
                // convert Scratch rotation to whatever tf this is (-32768 to 32767)
                renderRotation = ((sprite->rotation - 90) * 91);
            } else if (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0) {
                flip = GL_FLIP_H;
            }

            // Center the image
            const int renderX = sprite->xPosition + SCREEN_HALF_WIDTH;
            const int renderY = -sprite->yPosition + SCREEN_HALF_HEIGHT;

            glSpriteRotateScale(renderX, renderY, renderRotation, renderScale, flip, image);

        } else {
            const int renderX = static_cast<int>(sprite->xPosition);
            const int renderY = static_cast<int>(sprite->yPosition * -1);
            sprite->spriteWidth = 6;
            sprite->spriteHeight = 6;

            glBoxFilled(renderX + (SCREEN_HALF_WIDTH - 3), renderY + (SCREEN_HALF_HEIGHT - 3), renderX + (SCREEN_HALF_WIDTH + 3), renderY + (SCREEN_HALF_HEIGHT + 3), RGB15(0, 0, 0));
        }
    }

    glEnd2D();
    glFlush(0);
    swiWaitForVBlank();
}

void Render::renderVisibleVariables() {
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    glBegin2D();
    glBoxFilled(x, y, w, h, RGB15(colorR, colorB, colorG));
    glEnd2D();
    glFlush(0);
    swiWaitForVBlank();
}

bool Render::appShouldRun() {
    return true;
}
