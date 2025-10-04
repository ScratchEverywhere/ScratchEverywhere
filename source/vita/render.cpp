#include "render.hpp"
#include "audio.hpp"
#include "image.hpp"
#include "input.hpp"
#include "interpret.hpp"
#include "render.hpp"
#include "text.hpp"
#include "unzip.hpp"
#include <chrono>
#ifdef ENABLE_AUDIO
// TODO: non-SDL audio engine
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#endif

#ifdef ENABLE_CLOUDVARS
#include <malloc.h>

#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000
#endif
#include <psp2/touch.h>

#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 544

using u32 = uint32_t;

u32 clrWhite = RGBA8(255, 255, 255, 255);
u32 clrBlack = RGBA8(0, 0, 0, 255);
u32 clrGreen = RGBA8(0, 0, 255, 255);
u32 clrScratchBlue = RGBA8(71, 107, 115, 255);
std::chrono::system_clock::time_point Render::startTime = std::chrono::system_clock::now();
std::chrono::system_clock::time_point Render::endTime = std::chrono::system_clock::now();
bool Render::debugMode = false;
static bool isConsoleInit = false;

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;
bool Render::hasFrameBegan;
static int currentScreen = 0;
std::vector<Monitor> Render::visibleVariables;

#ifdef ENABLE_CLOUDVARS
static uint32_t *SOC_buffer = NULL;
#endif

bool Render::Init() {
	vita2d_init();
	vita2d_set_clear_color(clrWhite);

#ifdef ENABLE_CLOUDVARS
    int ret;

    SOC_buffer = (uint32_t *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (SOC_buffer == NULL) {
        Log::logError("memalign: failed to allocate");
    } else if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
        std::ostringstream err;
        err << "socInit: 0x" << std::hex << std::setw(8) << std::setfill('0') << ret;
        Log::logError(err.str());
    }
#endif

#ifdef ENABLE_AUDIO
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        Log::logWarning(std::string("SDL_mixer could not initialize! Error: ") + Mix_GetError());
    }
    int flags = MIX_INIT_MP3 | MIX_INIT_OGG;
    if (Mix_Init(flags) != flags) {
        Log::logWarning(std::string("SDL_mixer could not initialize MP3/OGG support! SDL_mixer Error: ") + Mix_GetError());
    }
#endif

    return true;
}

bool Render::appShouldRun() {
    if (toExit) return false;
    return true;
}

void *Render::getRenderer() {
    return nullptr;
}

int Render::getWidth() {
    return SCREEN_WIDTH;
}
int Render::getHeight() {
    return SCREEN_HEIGHT;
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        hasFrameBegan = true;
		vita2d_start_drawing();
    }
}

void Render::endFrame(bool shouldFlush) {
	if (hasFrameBegan) {
		vita2d_end_drawing();
		vita2d_swap_buffers();
		vita2d_set_vblank_wait(1);
	}
    if (shouldFlush) Image::FlushImages();
    hasFrameBegan = false;
}

void Render::drawBox(int w, int h, int x, int y, int colorR, int colorG, int colorB, int colorA) {
	vita2d_draw_rectangle(
        x - (w / 2.0f), y - (h / 2.0f), w, h,
        RGBA8(colorR, colorG, colorB, colorA));
}

void drawBlackBars(int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    if (screenAspect > projectAspect) {
        // Screen is wider than project,, vertical bars
        float scale = static_cast<float>(screenHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

		vita2d_draw_rectangle(0, 0, barWidth, screenHeight, clrBlack);                      // Left bar
		vita2d_draw_rectangle(screenWidth - barWidth, 0, barWidth, screenHeight, clrBlack); // Right bar

    } else if (screenAspect < projectAspect) {
        // Screen is taller than project,, horizontal bars
        float scale = static_cast<float>(screenWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

		vita2d_draw_rectangle(0, 0, screenWidth, barHeight, clrBlack);                        // Top bar
		vita2d_draw_rectangle(0, screenHeight - barHeight, screenWidth, barHeight, clrBlack); // Bottom bar
    }
}

void Render::renderSprites() {
	vita2d_clear_screen();
	
	double scaleX = SCREEN_WIDTH / Scratch::projectWidth;
	double scaleY = SCREEN_HEIGHT / Scratch::projectHeight;
	double scale = std::min(scaleX, scaleY);

    // Sort sprites by layer with stage always being first
    std::vector<Sprite *> spritesByLayer = sprites;
    std::sort(spritesByLayer.begin(), spritesByLayer.end(),
              [](const Sprite *a, const Sprite *b) {
                  // Stage sprite always comes first
                  if (a->isStage && !b->isStage) return true;
                  if (!a->isStage && b->isStage) return false;
                  // Otherwise sort by layer
                  return a->layer < b->layer;
              });

    // Draw the sprites
	bool legacyDrawing;
	for (Sprite *currentSprite : spritesByLayer) {
		if (!currentSprite->visible) continue;
		auto imgFind = imageData.find(currentSprite->costumes[currentSprite->currentCostume].id);
		legacyDrawing = (imgFind == imageData.end());
		if (legacyDrawing) {
			drawBox((currentSprite->xPosition * scale) + (SCREEN_WIDTH / 2),
					(currentSprite->yPosition * -1 * scale) + (SCREEN_HEIGHT * 0.5),
					16, 16, 0, 0, 0, 255);
			currentSprite->spriteWidth = 64;
			currentSprite->spriteHeight = 64;
		} else {
			VitaImage image = imgFind->second;
			double rotation = Math::degreesToRadians(currentSprite->rotation - 90.0f);
			double renderRotation = rotation;
			bool flip = false;
			if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT) {
				if (std::cos(rotation) < 0) {
					flip = true;
				}
				renderRotation = 0;
			}
			if (currentSprite->rotationStyle == currentSprite->NONE) {
				renderRotation = 0;
			}
			float ghost = std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f);
			// TODO: brightness effect
			image.render((currentSprite->xPosition * scale) + (SCREEN_WIDTH / 2),
						 (currentSprite->yPosition * -1 * scale) + (SCREEN_HEIGHT * 0.5),
						 scale * (currentSprite->size * 0.01), ghost / 100.0f,
						 renderRotation, flip ? -1 : 1);
		}
	}
	
	drawBlackBars(SCREEN_WIDTH, SCREEN_HEIGHT);
	renderVisibleVariables();

	endFrame(true);
#ifdef ENABLE_AUDIO
    SoundPlayer::flushAudio();
#endif
}

std::unordered_map<std::string, TextObject *> Render::monitorTexts;

void Render::renderVisibleVariables() {

    // get screen scale
    double scaleX = static_cast<double>(SCREEN_WIDTH) / Scratch::projectWidth;
    double scaleY = static_cast<double>(SCREEN_HEIGHT) / Scratch::projectHeight;
    double scale = std::min(scaleX, scaleY);

    // calculate black bar offset
    float screenAspect = static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;
    float barOffsetX = 0.0f;
    float barOffsetY = 0.0f;
    if (screenAspect > projectAspect) {
        float scaledProjectWidth = Scratch::projectWidth * scale;
        barOffsetX = (SCREEN_WIDTH - scaledProjectWidth) / 2.0f;
    } else if (screenAspect < projectAspect) {
        float scaledProjectHeight = Scratch::projectHeight * scale;
        barOffsetY = (SCREEN_HEIGHT - scaledProjectHeight) / 2.0f;
    }

    for (auto &var : visibleVariables) {
        if (var.visible) {

            std::string renderText = BlockExecutor::getMonitorValue(var).asString();

            if (monitorTexts.find(var.id) == monitorTexts.end()) {
                monitorTexts[var.id] = createTextObject(renderText, var.x, var.y);
            } else {
                monitorTexts[var.id]->setText(renderText);
            }
            monitorTexts[var.id]->setColor(RGBA8(0, 0, 0, 255));
            if (var.mode != "large") {
                monitorTexts[var.id]->setCenterAligned(false);
                monitorTexts[var.id]->setScale(0.6);
            } else {
                monitorTexts[var.id]->setCenterAligned(true);
                monitorTexts[var.id]->setScale(1);
            }

            monitorTexts[var.id]->render(var.x + barOffsetX, var.y + barOffsetY);

        } else {
            if (monitorTexts.find(var.id) != monitorTexts.end()) {
                monitorTexts.erase(var.id);
            }
        }
    }
}

void Render::deInit() {
#ifdef ENABLE_CLOUDVARS
    socExit();
#endif
    Image::cleanupImages();
    SoundPlayer::cleanupAudio();
    TextObject::cleanupText();
	SoundPlayer::deinit();
	vita2d_fini();
#ifdef ENABLE_AUDIO
    SDL_Quit();
#endif
}
