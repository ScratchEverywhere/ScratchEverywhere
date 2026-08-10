#include "render.hpp"
#include "blueprint.hpp"
#include "entity_components.hpp"
#include "entity_manager.hpp"
#include "speech_manager.hpp"
#include "speech_manager_sdl2.hpp"
#include <SDL.h>
#include <algorithm>
#include <audio.hpp>
#include <cmath>
#include <image.hpp>
#include <input.hpp>
#include <log.hpp>
#include <render.hpp>
#include <runtime/systems/costume_system.hpp>
#include <runtime/vm/engine_state.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <windowing/sdl2/window.hpp>

#ifdef __WIIU__
#include <coreinit/debug.h>
#include <nn/act.h>
#include <whb/log_udp.h>
#include <whb/sdcard.h>
#endif

#ifdef __SWITCH__
#include <switch.h>

char nickname[0x21];
#endif

#ifdef VITA
#include <psp2/io/fcntl.h>
#include <psp2/net/http.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/touch.h>
#endif

#ifdef __OGC__
#include <fat.h>
#include <ogc/system.h>
#endif

#ifdef __PS4__
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>
#endif

#ifdef GAMECUBE
#include <ogc/consol.h>
#include <ogc/exi.h>
#endif

WindowSE *globalWindow = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_Texture *penTexture = nullptr;

SpeechManagerSDL2 *speechManager = nullptr;

static std::vector<SDL_Vertex> penVerts;

bool Render::Init() {
#ifdef __WIIU__
    int windowWidth = 854;
    int windowHeight = 480;
#elif defined(__SWITCH__)
    int windowWidth = 1280;
    int windowHeight = 720;
#elif defined(__OGC__)
    int windowWidth = 640;
    int windowHeight = 480;
#elif defined(VITA)
    int windowWidth = 960;
    int windowHeight = 544;
#elif defined(__PSP__)
    int windowWidth = 480;
    int windowHeight = 272;
#elif defined(__PS4__)
    int windowWidth = 1280;
    int windowHeight = 720;

    // Freetype has to be initialized before SDL2_ttf
    int rc = sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL);
    if (rc != ORBIS_OK) {
        Log::logError("Failed to init freetype.");
        return false;
    }
#elif defined(WEBOS)
    // SDL has to be initialized before window creation on webOS
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) {
        Log::logError("Failed to initialize SDL2: " + std::string(SDL_GetError()));
        return false;
    }

    int windowWidth = 800;
    int windowHeight = 480;

    SDL_DisplayMode mode;
    SDL_GetDisplayMode(0, 0, &mode);
    if (mode.w > 0 && mode.h > 0) {
        windowWidth = mode.w;
        windowHeight = mode.h;
    }
#else
    int windowWidth = 480;
    int windowHeight = 360;
#endif

    TTF_Init();

    globalWindow = new WindowSDL2();
    if (!globalWindow->init(windowWidth, windowHeight, "Scratch Everywhere!")) {
        delete globalWindow;
        globalWindow = nullptr;
        return false;
    }
#if defined(WEBOS) || defined(__PSP__) || defined(__PS4__)
    uint32_t sdlFlags = SDL_RENDERER_ACCELERATED;
#else
    uint32_t sdlFlags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
#endif
    renderer = SDL_CreateRenderer((SDL_Window *)globalWindow->getHandle(), -1, sdlFlags);
    if (renderer == NULL) {
        Log::logError("Could not create renderer: " + std::string(SDL_GetError()));
        return false;
    }

    debugMode = true;

    return true;
}
void Render::deInit() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }

    SDL_DestroyTexture(penTexture);

    TextObject::cleanupText();
    SDL_DestroyRenderer(renderer);

    if (globalWindow) {
        globalWindow->cleanup();
        delete globalWindow;
        globalWindow = nullptr;
    }

    SoundPlayer::deinit();
    SDL_Quit();
}

void *Render::getRenderer() {
    return static_cast<void *>(renderer);
}

bool Render::createSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerSDL2(renderer);
    return speechManager != nullptr;
}

void Render::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

SpeechManager *Render::getSpeechManager() {
    return speechManager;
}

int Render::getWidth() {
    if (globalWindow) return globalWindow->getWidth();
    return 540;
}

int Render::getHeight() {
    if (globalWindow) return globalWindow->getHeight();
    return 405;
}

float Render::getPixelDensity() {
    if (globalWindow) return globalWindow->getPixelDensity();
    return 1.0f;
}

bool Render::initPen() {
    if (penTexture != nullptr) return true;

    if (EngineState::hqPen) {
        if (EngineState::projectWidth / static_cast<double>(getWidth()) < EngineState::projectHeight / static_cast<double>(getHeight()))
            penTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, EngineState::projectWidth * (getHeight() / static_cast<double>(EngineState::projectHeight)), getHeight());
        else
            penTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, getWidth(), EngineState::projectHeight * (getWidth() / static_cast<double>(EngineState::projectWidth)));
    } else penTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, EngineState::projectWidth, EngineState::projectHeight);

    // Clear the texture
    SDL_SetTextureBlendMode(penTexture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, penTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, nullptr);
    return true;
}

void Render::penMoveFast(double x1, double y1, double x2, double y2, uint32_t spriteID) {
    PenState &pen = EntityManager::pen[spriteID];
    const ColorRGBA rgbColor = CSBT2RGBA(pen.color);
    const uint8_t alpha = (100.0 - pen.color.transparency) / 100.0 * 255.0;

    int penWidth = 640;
    int penHeight = 480;
    SDL_QueryTexture(penTexture, NULL, NULL, &penWidth, &penHeight);

    const double scale = (penHeight / static_cast<double>(EngineState::projectHeight));

    const float sx1 = static_cast<float>(x1 * scale + penWidth / 2.0);
    const float sy1 = static_cast<float>(-y1 * scale + penHeight / 2.0);
    const float sx2 = static_cast<float>(x2 * scale + penWidth / 2.0);
    const float sy2 = static_cast<float>(-y2 * scale + penHeight / 2.0);

    const double dx = sx2 - sx1;
    const double dy = sy2 - sy1;

    const double length = sqrt(dx * dx + dy * dy);
    const double drawWidth = (pen.size / 2.0f) * scale;

    if (length <= 0) return;

    const float nx = static_cast<float>((-dy / length) * drawWidth);
    const float ny = static_cast<float>((dx / length) * drawWidth);

    const SDL_Color sdlColor = {
        static_cast<Uint8>(rgbColor.r),
        static_cast<Uint8>(rgbColor.g),
        static_cast<Uint8>(rgbColor.b),
        static_cast<Uint8>(alpha)};

    const SDL_Vertex v0 = {{sx1 + nx, sy1 + ny}, sdlColor, {0.0f, 0.0f}}; // top left
    const SDL_Vertex v1 = {{sx1 - nx, sy1 - ny}, sdlColor, {0.0f, 0.0f}}; // bottom left
    const SDL_Vertex v2 = {{sx2 + nx, sy2 + ny}, sdlColor, {0.0f, 0.0f}}; // top right
    const SDL_Vertex v3 = {{sx2 - nx, sy2 - ny}, sdlColor, {0.0f, 0.0f}}; // bottom right

    // squarey
    penVerts.push_back(v0);
    penVerts.push_back(v1);
    penVerts.push_back(v2);

    penVerts.push_back(v1);
    penVerts.push_back(v3);
    penVerts.push_back(v2);
}

void Render::penDotFast(uint32_t spriteID) {
    PenState &pen = EntityManager::pen[spriteID];
    SpriteTransform &transform = EntityManager::transforms[spriteID];
    const ColorRGBA rgbColor = CSBT2RGBA(pen.color);
    const uint8_t alpha = (100.0 - pen.color.transparency) / 100.0 * 255.0;

    int penWidth = 640;
    int penHeight = 480;
    SDL_QueryTexture(penTexture, NULL, NULL, &penWidth, &penHeight);

    const double scale = (penHeight / static_cast<double>(EngineState::projectHeight));

    const float sx = static_cast<float>(transform.x * scale + penWidth / 2.0);
    const float sy = static_cast<float>(-transform.y * scale + penHeight / 2.0);

    const float halfSize = static_cast<float>((pen.size / 2.0f) * scale);

    const SDL_Color sdlColor = {
        static_cast<Uint8>(rgbColor.r),
        static_cast<Uint8>(rgbColor.g),
        static_cast<Uint8>(rgbColor.b),
        alpha};

    const SDL_Vertex v0 = {{sx - halfSize, sy - halfSize}, sdlColor, {0.0f, 0.0f}}; // top left
    const SDL_Vertex v1 = {{sx - halfSize, sy + halfSize}, sdlColor, {0.0f, 0.0f}}; // bottom left
    const SDL_Vertex v2 = {{sx + halfSize, sy - halfSize}, sdlColor, {0.0f, 0.0f}}; // top right
    const SDL_Vertex v3 = {{sx + halfSize, sy + halfSize}, sdlColor, {0.0f, 0.0f}}; // bottom right

    // squarey
    penVerts.push_back(v0);
    penVerts.push_back(v1);
    penVerts.push_back(v2);

    penVerts.push_back(v1);
    penVerts.push_back(v3);
    penVerts.push_back(v2);
}

void Render::penMoveAccurate(double x1, double y1, double x2, double y2, uint32_t spriteID) {
    PenState &pen = EntityManager::pen[spriteID];
    const ColorRGBA rgbColor = CSBT2RGBA(pen.color);
    const uint8_t alpha = (100.0 - pen.color.transparency) / 100.0 * 255.0;

    int penWidth = 640;
    int penHeight = 480;
    SDL_QueryTexture(penTexture, NULL, NULL, &penWidth, &penHeight);

    const double scale = (penHeight / static_cast<double>(EngineState::projectHeight));

    const float sx1 = static_cast<float>(x1 * scale + penWidth / 2.0);
    const float sy1 = static_cast<float>(-y1 * scale + penHeight / 2.0);
    const float sx2 = static_cast<float>(x2 * scale + penWidth / 2.0);
    const float sy2 = static_cast<float>(-y2 * scale + penHeight / 2.0);

    const double dx = sx2 - sx1;
    const double dy = sy2 - sy1;

    const double length = sqrt(dx * dx + dy * dy);
    const double drawWidth = (pen.size / 2.0f) * scale;

    const SDL_Color sdlColor = {
        static_cast<uint8_t>(rgbColor.r),
        static_cast<uint8_t>(rgbColor.g),
        static_cast<uint8_t>(rgbColor.b),
        static_cast<uint8_t>(alpha)};

    if (length > 0) {
        const float nx = static_cast<float>((-dy / length) * drawWidth);
        const float ny = static_cast<float>((dx / length) * drawWidth);

        const SDL_Vertex v0 = {{sx1 + nx, sy1 + ny}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex v1 = {{sx1 - nx, sy1 - ny}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex v2 = {{sx2 + nx, sy2 + ny}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex v3 = {{sx2 - nx, sy2 - ny}, sdlColor, {0.0f, 0.0f}};

        penVerts.push_back(v0);
        penVerts.push_back(v1);
        penVerts.push_back(v2);

        penVerts.push_back(v1);
        penVerts.push_back(v3);
        penVerts.push_back(v2);
    }

    const unsigned int circleSegments = std::max(8.0f, 8.0f * (pen.size / 150.0f));
    const double angleStep = 2.0 * M_PI / circleSegments;

    for (int i = 0; i < circleSegments; ++i) {
        const double angle1 = i * angleStep;
        const double angle2 = (i + 1) * angleStep;

        float x1_c = sx1 + static_cast<float>(cos(angle1) * drawWidth);
        float y1_c = sy1 + static_cast<float>(sin(angle1) * drawWidth);
        float x2_c = sx1 + static_cast<float>(cos(angle2) * drawWidth);
        float y2_c = sy1 + static_cast<float>(sin(angle2) * drawWidth);

        const SDL_Vertex cv0 = {{sx1, sy1}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex cv1 = {{x1_c, y1_c}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex cv2 = {{x2_c, y2_c}, sdlColor, {0.0f, 0.0f}};

        penVerts.push_back(cv0);
        penVerts.push_back(cv1);
        penVerts.push_back(cv2);

        x1_c = sx2 + static_cast<float>(cos(angle1) * drawWidth);
        y1_c = sy2 + static_cast<float>(sin(angle1) * drawWidth);
        x2_c = sx2 + static_cast<float>(cos(angle2) * drawWidth);
        y2_c = sy2 + static_cast<float>(sin(angle2) * drawWidth);

        const SDL_Vertex cv3 = {{sx2, sy2}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex cv4 = {{x1_c, y1_c}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex cv5 = {{x2_c, y2_c}, sdlColor, {0.0f, 0.0f}};

        penVerts.push_back(cv3);
        penVerts.push_back(cv4);
        penVerts.push_back(cv5);
    }
}

void Render::penDotAccurate(uint32_t spriteID) {
    PenState &pen = EntityManager::pen[spriteID];
    SpriteTransform &transform = EntityManager::transforms[spriteID];
    const ColorRGBA rgbColor = CSBT2RGBA(pen.color);
    const uint8_t alpha = static_cast<Uint8>((100.0 - pen.color.transparency) / 100.0 * 255.0);

    int penWidth = 640;
    int penHeight = 480;
    SDL_QueryTexture(penTexture, NULL, NULL, &penWidth, &penHeight);

    const double scale = (penHeight / static_cast<double>(EngineState::projectHeight));

    const float sx = static_cast<float>(transform.x * scale + penWidth / 2.0);
    const float sy = static_cast<float>(-transform.y * scale + penHeight / 2.0);

    const float radius = static_cast<float>((pen.size / 2.0f) * scale);

    const SDL_Color sdlColor = {
        static_cast<uint8_t>(rgbColor.r),
        static_cast<uint8_t>(rgbColor.g),
        static_cast<uint8_t>(rgbColor.b),
        static_cast<uint8_t>(alpha)};

    const unsigned int circleSegments = std::max(16.0f, 16.0f * (pen.size / 150.0f));
    const double angleStep = 2.0 * M_PI / circleSegments;

    for (int i = 0; i < circleSegments; ++i) {
        const double angle1 = i * angleStep;
        const double angle2 = (i + 1) * angleStep;

        const float x1 = sx + static_cast<float>(cos(angle1) * radius);
        const float y1 = sy + static_cast<float>(sin(angle1) * radius);
        const float x2 = sx + static_cast<float>(cos(angle2) * radius);
        const float y2 = sy + static_cast<float>(sin(angle2) * radius);

        const SDL_Vertex v0 = {{sx, sy}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex v1 = {{x1, y1}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex v2 = {{x2, y2}, sdlColor, {0.0f, 0.0f}};

        penVerts.push_back(v0);
        penVerts.push_back(v1);
        penVerts.push_back(v2);
    }
}

void Render::penStamp(uint32_t spriteID) {
    const RenderInfo render = EntityManager::renderInfo[spriteID];
    const TargetDefinition &def = EntityManager::blueprints[EntityManager::blueprintIds[spriteID]];
    auto imgFind = CostumeSystem::costumeImages.find(def.costumes[render.costumeId].fullName);
    if (imgFind == CostumeSystem::costumeImages.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }

    const Costume &costume = def.costumes[render.costumeId];

    SDL_SetRenderTarget(renderer, penTexture);

    // clear line draw queue so stamp can be rendered on top
    if (!penVerts.empty()) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_RenderGeometry(renderer, NULL, penVerts.data(), penVerts.size(), NULL, 0);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        penVerts.clear();
    }

    Image *image = imgFind->second.get();

    calculateRenderPosition(spriteID);

    // Pen mapping stuff
    const auto &cords = Render::screenToScratchCoords(render.x, render.y, getWidth(), getHeight());
    int penX = cords.first + EngineState::projectWidth / 2;
    int penY = -cords.second + EngineState::projectHeight / 2;

    float penScale;
    const SpriteTransform &transform = EntityManager::transforms[spriteID];
    if (EngineState::hqPen) {
        int penWidth;
        int penHeight;
        SDL_QueryTexture(penTexture, NULL, NULL, &penWidth, &penHeight);
        const double scale = (penHeight / static_cast<double>(EngineState::projectHeight));

        penX *= scale;
        penY *= scale;
        penScale = render.scaleY;
    } else {
        penScale = (transform.size / 100.0f) / costume.bitmapResolution;
    }

    ImageRenderParams params;
    params.centered = true;
    params.x = penX;
    params.y = penY;
    params.rotation = render.rotation;
    params.scale = penScale;
    params.flip = (transform.rotationStyle == RotationStyle::LEFT_RIGHT && transform.direction < 0);
    if (render.hasEffects()) {
        GraphicEffects &effect = EntityManager::effects[spriteID];
        params.opacity = 1.0f - (std::clamp(effect.ghost, 0.0f, 100.0f) * 0.01f);
        params.brightness = effect.brightness;
    } else {
        params.opacity = 1.0f;
        params.brightness = 0.0f;
    }

    image->render(params);

    SDL_SetRenderTarget(renderer, NULL);
}

void Render::penClear() {
    if (!penTexture || penTexture == nullptr) return;
    SDL_SetRenderTarget(renderer, penTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, NULL);
    if (!penVerts.empty()) penVerts.clear();
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        SDL_SetRenderDrawColor(renderer, colorR, colorG, colorB, 255);
        SDL_RenderClear(renderer);
        hasFrameBegan = true;
    }
}

void Render::endFrame(bool shouldFlush) {
    SDL_RenderPresent(renderer);
    SDL_Delay(16);
    hasFrameBegan = false;
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    SDL_SetRenderDrawColor(renderer, colorR, colorG, colorB, colorA);
    SDL_Rect rect = {x - (w / 2), y - (h / 2), w, h};
    SDL_RenderFillRect(renderer, &rect);
}

void drawBlackBars(int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(EngineState::projectWidth) / EngineState::projectHeight;

    if (screenAspect > projectAspect) {
        // Vertical bars,,,
        float scale = static_cast<float>(screenHeight) / EngineState::projectHeight;
        float scaledProjectWidth = EngineState::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Rect leftBar = {0, 0, static_cast<int>(std::ceil(barWidth)), screenHeight};
        SDL_Rect rightBar = {static_cast<int>(std::floor(screenWidth - barWidth)), 0, static_cast<int>(std::ceil(barWidth)), screenHeight};

        SDL_RenderFillRect(renderer, &leftBar);
        SDL_RenderFillRect(renderer, &rightBar);
    } else if (screenAspect < projectAspect) {
        // Horizontal bars,,,
        float scale = static_cast<float>(screenWidth) / EngineState::projectWidth;
        float scaledProjectHeight = EngineState::projectHeight * scale;
        float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Rect topBar = {0, 0, screenWidth, static_cast<int>(std::ceil(barHeight))};
        SDL_Rect bottomBar = {0, static_cast<int>(std::floor(screenHeight - barHeight)), screenWidth, static_cast<int>(std::ceil(barHeight))};

        SDL_RenderFillRect(renderer, &topBar);
        SDL_RenderFillRect(renderer, &bottomBar);
    }
}

void Render::renderSprites() {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    for (uint32_t instanceId : EntityManager::renderOrder) {
        if (!EntityManager::activeInstances[instanceId]) continue;

        const auto &rInfo = EntityManager::renderInfo[instanceId];

        if (!rInfo.isVisible()) continue;

        const auto &transform = EntityManager::transforms[instanceId];
        uint32_t defId = EntityManager::blueprintIds[instanceId];
        const auto &blueprint = EntityManager::blueprints[defId];

        if (rInfo.costumeId >= blueprint.costumes.size()) continue;

        const auto &currentCostume = blueprint.costumes[rInfo.costumeId];

        auto imgFind = CostumeSystem::costumeImages.find(currentCostume.fullName);
        if (imgFind != CostumeSystem::costumeImages.end()) {
            Image *image = imgFind->second.get();

            calculateRenderPosition(instanceId);

            const auto &effect = EntityManager::effects[instanceId];

            ImageRenderParams params;
            params.centered = true;
            params.x = rInfo.x;
            params.y = rInfo.y;
            params.rotation = rInfo.rotation;
            params.scale = rInfo.scaleY;

            params.flip = (transform.rotationStyle == RotationStyle::LEFT_RIGHT && transform.direction < 0.0f);

            params.opacity = 1.0f - (std::clamp(effect.ghost, 0.0f, 100.0f) * 0.01f);
            params.brightness = effect.brightness;

            image->render(params);
        }

        if (transform.isStage() || instanceId == EntityManager::stageSprite) {
            renderPenLayer();
        }
    }

    if (speechManager) {
        speechManager->render();
    }

    drawBlackBars(getWidth(), getHeight());
    renderMonitors();

#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
    if (Input::mousePointer.isMoving) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_Rect rect;
        rect.w = rect.h = 5;
        rect.x = (Input::mousePointer.x * renderScale) + (globalWindow->getWidth() * 0.5f);
        rect.y = (Input::mousePointer.y * -1.0f * renderScale) + (globalWindow->getHeight() * 0.5f);

        Input::mousePointer.x = std::clamp((float)Input::mousePointer.x, -EngineState::projectWidth * 0.5f, EngineState::projectWidth * 0.5f);
        Input::mousePointer.y = std::clamp((float)Input::mousePointer.y, -EngineState::projectHeight * 0.5f, EngineState::projectHeight * 0.5f);

        SDL_RenderDrawRect(renderer, &rect);
    }
#endif

    SDL_RenderPresent(renderer);
}

void Render::renderPenLayer() {
    if (!penVerts.empty()) {
        SDL_SetRenderTarget(renderer, penTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        SDL_RenderGeometry(renderer, NULL, penVerts.data(), penVerts.size(), NULL, 0);
        penVerts.clear();

        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    SDL_Rect renderRect = {0, 0, 0, 0};

    if (static_cast<float>(getWidth()) / getHeight() > static_cast<float>(EngineState::projectWidth) / EngineState::projectHeight) {
        renderRect.x = std::ceil((getWidth() - EngineState::projectWidth * (static_cast<float>(getHeight()) / EngineState::projectHeight)) / 2.0f);
        renderRect.w = getWidth() - renderRect.x * 2;
        renderRect.h = getHeight();
    } else {
        renderRect.y = std::ceil((getHeight() - EngineState::projectHeight * (static_cast<float>(getWidth()) / EngineState::projectWidth)) / 2.0f);
        renderRect.h = getHeight() - renderRect.y * 2;
        renderRect.w = getWidth();
    }

    SDL_RenderCopy(renderer, penTexture, NULL, &renderRect);
}

bool Render::appShouldRun() {
    if (OS::toExit) return false;
    if (globalWindow) {
        globalWindow->pollEvents();

        static int lastW = 0, lastH = 0;
        int currentW = globalWindow->getWidth();
        int currentH = globalWindow->getHeight();

        if (lastW != currentW || lastH != currentH) {
            lastW = currentW;
            lastH = currentH;

            if (EngineState::hqPen) {
                SDL_Texture *newTexture;
                if (EngineState::projectWidth / static_cast<double>(currentW) < EngineState::projectHeight / static_cast<double>(currentH))
                    newTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, EngineState::projectWidth * (currentH / static_cast<double>(EngineState::projectHeight)), currentH);
                else
                    newTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, currentW, EngineState::projectHeight * (currentW / static_cast<double>(EngineState::projectWidth)));

                SDL_SetTextureBlendMode(newTexture, SDL_BLENDMODE_NONE);
                SDL_SetTextureBlendMode(penTexture, SDL_BLENDMODE_NONE);
                SDL_SetRenderTarget(renderer, newTexture);
                SDL_RenderCopy(renderer, penTexture, nullptr, nullptr);
                SDL_SetRenderTarget(renderer, nullptr);
                SDL_SetTextureBlendMode(newTexture, SDL_BLENDMODE_BLEND);
                SDL_DestroyTexture(penTexture);
                penTexture = newTexture;
            }
        }

        return !globalWindow->shouldClose();
    }
    return false;
}
