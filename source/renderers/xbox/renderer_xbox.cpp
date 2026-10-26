#include "renderer_xbox.hpp"
#include "speech_manager_xbox.hpp"
#include "window_sdl3.hpp"
#include <os.hpp>
#include <window.hpp>

#ifdef __XBOX__
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <hal/video.h>

#undef min
#undef max

#ifndef VENC_MODE_WIDESCREEN
#define VENC_MODE_WIDESCREEN VIDEO_WIDESCREEN
#endif
#ifndef VENC_MODE_720P
#define VENC_MODE_720P VIDEO_MODE_720P
#endif
#ifndef VENC_MODE_1080I
#define VENC_MODE_1080I VIDEO_MODE_1080I
#endif
#ifndef VENC_MODE_480P
#define VENC_MODE_480P VIDEO_MODE_480P
#endif
#ifndef VENC_MODE_LETTERBOX
#define VENC_MODE_LETTERBOX VIDEO_LETTERBOX
#endif
#endif

#include <algorithm>
#include <audio.hpp>
#include <cmath>
#include <cstdlib>
#include <downloader.hpp>
#include <image.hpp>
#include <image_xbox.hpp>
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <runtime.hpp>
#include <string>
#include <text.hpp>
#include <types.hpp>
#include <unordered_map>
#include <vector>

static SDL_Texture *mainRenderTarget = nullptr;
static bool hasOverrideTarget = false;
static int overrideWidth = 0;
static int overrideHeight = 0;
static SDL_Texture *penTexture = nullptr;

static std::vector<SDL_Vertex> penVerts;

// Flush pending pen geometry once this many vertices have accumulated.
static constexpr size_t PEN_FLUSH_THRESHOLD = 6144;

// Stamps queued since SDL's command queue was last flushed.
static constexpr unsigned PEN_STAMP_FLUSH_INTERVAL = 16;
static unsigned stampsSinceFlush = 0;

// Pen texture metrics, cached when the texture is (re)created instead of queried on every pen call.
static float penTexW = 640.0f;
static float penTexH = 480.0f;
static double penScaleY = 1.0;

static void refreshPenMetrics() {
    if (!penTexture) return;
    SDL_GetTextureSize(penTexture, &penTexW, &penTexH);
    penScaleY = penTexH / static_cast<double>(Scratch::projectHeight);
}

// Render target handling.
// Switching SDL render targets flushes SDL's command queue and makes the GPU wait for idle, so it is
// expensive. The pen texture therefore stays the active target across consecutive pen operations
// (lines, dots, stamps) and the main target is restored lazily, before anything else draws.
static bool penTargetActive = false;

static void ensurePenTarget(SDL_Renderer *renderer) {
    if (penTargetActive || !penTexture) return;
    SDL_SetRenderTarget(renderer, penTexture);
    penTargetActive = true;
    stampsSinceFlush = 0; // switching targets flushes the queue
}

static void ensureMainTarget(SDL_Renderer *renderer) {
    if (!penTargetActive) return;
    SDL_SetRenderTarget(renderer, mainRenderTarget);
    penTargetActive = false;
    stampsSinceFlush = 0;
}

// Draws any queued pen geometry into the pen texture. Leaves the pen texture as the active target.
static void flushPenVerts(SDL_Renderer *renderer) {
    if (penVerts.empty() || !penTexture) return;
    ensurePenTarget(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    constexpr size_t BATCH_SIZE = 2048 * 3; // 6144 vertices
    for (size_t i = 0; i < penVerts.size(); i += BATCH_SIZE) {
        size_t count = std::min(BATCH_SIZE, penVerts.size() - i);
        SDL_RenderGeometry(renderer, nullptr, &penVerts[i], static_cast<int>(count), nullptr, 0);
        SDL_FlushRenderer(renderer);
    }
    penVerts.clear();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    stampsSinceFlush = 0;
}

// Pen circle geometry. cos/sin of the circle points are computed once per segment count.
struct CirclePoint {
    float c, s;
};

static const CirclePoint *getCircleTable(unsigned int segments) {
    static std::vector<std::vector<CirclePoint>> tables;
    if (segments >= tables.size()) tables.resize(segments + 1);
    std::vector<CirclePoint> &table = tables[segments];
    if (table.empty()) {
        table.resize(segments + 1);
        const double step = 2.0 * M_PI / segments;
        for (unsigned int i = 0; i <= segments; ++i) {
            table[i] = {static_cast<float>(std::cos(i * step)), static_cast<float>(std::sin(i * step))};
        }
    }
    return table.data();
}

static unsigned int circleSegmentsFor(double base, double penSize) {
    const double wanted = std::max(8.0, base * (penSize / 150.0));
    return std::min(static_cast<unsigned int>(wanted), 128u);
}

static void emitCircle(float cx, float cy, float radius, const SDL_FColor &color, const CirclePoint *table, unsigned int segments) {
    for (unsigned int i = 0; i < segments; ++i) {
        penVerts.push_back({{cx, cy}, color, {0.0f, 0.0f}});
        penVerts.push_back({{cx + table[i].c * radius, cy + table[i].s * radius}, color, {0.0f, 0.0f}});
        penVerts.push_back({{cx + table[i + 1].c * radius, cy + table[i + 1].s * radius}, color, {0.0f, 0.0f}});
    }
}

// The end cap of the last accurate pen move. When the next move continues from the same point with the
// same opaque color and size, its start cap would only repaint the same pixels, so it is skipped.
static bool lastCapValid = false;
static float lastCapX = 0.0f, lastCapY = 0.0f, lastCapRadius = 0.0f;
static SDL_FColor lastCapColor = {0, 0, 0, 0};

static inline bool sameColor(const SDL_FColor &a, const SDL_FColor &b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static void drawBlackBars(SDL_Renderer *renderer, int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    if (screenAspect > projectAspect) {
        float scale = static_cast<float>(screenHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_FRect leftBar = {0, 0, std::ceil(barWidth), static_cast<float>(screenHeight)};
        SDL_FRect rightBar = {std::floor(screenWidth - barWidth), 0, std::ceil(barWidth), static_cast<float>(screenHeight)};

        SDL_RenderFillRect(renderer, &leftBar);
        SDL_RenderFillRect(renderer, &rightBar);
    } else if (screenAspect < projectAspect) {
        float scale = static_cast<float>(screenWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_FRect topBar = {0, 0, static_cast<float>(screenWidth), std::ceil(barHeight)};
        SDL_FRect bottomBar = {0, std::floor(screenHeight - barHeight), static_cast<float>(screenWidth), std::ceil(barHeight)};

        SDL_RenderFillRect(renderer, &topBar);
        SDL_RenderFillRect(renderer, &bottomBar);
    }
}

static bool videoInitialized = false;
static Renderer_Xbox::VideoDisplaySettings xboxVideoSettings = {
    640, 480, 32, 60, false, false, false, false, 0.04f, 0
};

// Errata: Map raw SMC register 0x04 values (from hardware ID pins on Xbox AV connector)
// to standard AV_PACK_* constants due to incorrect mappings:
// 0x00: SCART
// 0x01: HDTV (Component)
// 0x02: VGA
// 0x03: RFU
// 0x04: S-Video
// 0x06: Standard (Composite)
// 0x07: Missing / Disconnected
static uint32_t mapSmcToAvPack(uint32_t smcVal) {
    switch (smcVal & 0x07) {
        case 0x00: return AV_PACK_SCART;
        case 0x01: return AV_PACK_HDTV;
        case 0x02: return AV_PACK_VGA;
        case 0x03: return AV_PACK_RFU;
        case 0x04: return AV_PACK_SVIDEO;
        case 0x06: return AV_PACK_STANDARD;
        case 0x07: return AV_PACK_NONE;
        default:   return AV_PACK_NONE;
    }
}

void Renderer_Xbox::initVideo() {
    if (videoInitialized) return;
    videoInitialized = true;

#ifdef __XBOX__
    // Read video display settings from Xbox EEPROM / MS Dashboard
    ULONG encoderSettings = XVideoGetEncoderSettings();
    bool isWidescreen = (encoderSettings & VENC_MODE_WIDESCREEN) != 0;
    bool supports720p = (encoderSettings & VENC_MODE_720P) != 0;
    bool supports480p = (encoderSettings & VENC_MODE_480P) != 0;
    bool supports1080i = (encoderSettings & VENC_MODE_1080I) != 0;

    xboxVideoSettings.isWidescreen = isWidescreen;
    xboxVideoSettings.supports720p = supports720p;
    xboxVideoSettings.supports480p = supports480p;
    xboxVideoSettings.supports1080i = supports1080i;

    // Detect physical AV cable / display device from SMC over SMBus (address 0x20, register 0x04)
    ULONG smcAvPack = 0;
    NTSTATUS smbStatus = HalReadSMBusValue(0x20, 0x04, FALSE, &smcAvPack);
    uint32_t resolvedPack = AV_PACK_NONE;
    if (NT_SUCCESS(smbStatus)) {
        resolvedPack = mapSmcToAvPack(smcAvPack);
    }
    if (resolvedPack == AV_PACK_NONE) {
        // Fallback to AV pack bits in encoder settings if SMBus read was unsuccessful
        resolvedPack = static_cast<uint32_t>(encoderSettings & VIDEO_ADAPTER_MASK);
    }
    xboxVideoSettings.avPackType = resolvedPack;

    // Automatic resolution detection:
    // If 720p is supported by the dashboard EEPROM settings and the connected AV pack
    // (HDTV Component or VGA), request 1280x720; otherwise fall back to 640x480.
    bool can720p = false;
    if (supports720p && (xboxVideoSettings.avPackType == AV_PACK_HDTV || xboxVideoSettings.avPackType == AV_PACK_VGA)) {
        VIDEO_MODE testMode;
        void *p = NULL;
        while (XVideoListModes(&testMode, 32, 60, &p)) {
            if (testMode.width == 1280 && testMode.height == 720) {
                can720p = true;
                break;
            }
        }
    }

#ifdef SE_XBOX_FORCE_480P
    can720p = false;
#endif
    xboxVideoSettings.width = can720p ? 1280 : 640;
    xboxVideoSettings.height = can720p ? 720 : 480;
    xboxVideoSettings.bpp = 32;
    xboxVideoSettings.refresh = 60;

    // Automatically determine overscan adjustment based on the hardware environment
    float autoOverscan = 0.0f;
    switch (xboxVideoSettings.avPackType) {
        case AV_PACK_VGA:
            // PC monitors display edge-to-edge with 1:1 pixel mapping (0% overscan)
            autoOverscan = 0.0f;
            break;

        case AV_PACK_HDTV:
            // Component (HDTV):
            // 720p and 1080i are digital/HDTV modes without CRT raster crop (0% overscan).
            // Standard 480i through component may be on early HD CRTs with overscan (4%).
            if (xboxVideoSettings.width == 640 && xboxVideoSettings.height == 480 && !xboxVideoSettings.supports480p) {
                autoOverscan = 0.04f;
            } else {
                autoOverscan = 0.0f;
            }
            break;

        case AV_PACK_STANDARD: // Composite Stereo
        case AV_PACK_SVIDEO:   // S-Video
        case AV_PACK_RFU:      // RF Adapter
        case AV_PACK_SCART:    // Advanced SCART (RGB)
        default:
            // Standard-definition analog CRT formats:
            // Almost all consumer CRTs crop borders by ~3-5%. Apply 4% centered safe margin.
            if (xboxVideoSettings.width == 640 && xboxVideoSettings.height == 480) {
                autoOverscan = 0.04f;
            } else {
                autoOverscan = 0.0f;
            }
            break;
    }
    xboxVideoSettings.overscanRatio = autoOverscan;

    char vidLog[256];
    snprintf(vidLog, sizeof(vidLog),
             "Xbox Video: %s cable detected. Mode %dx%d@%dHz (bpp: %d, widescreen: %s, 720p: %s, overscan: %d%%)",
             getAVPackName(), xboxVideoSettings.width, xboxVideoSettings.height, xboxVideoSettings.refresh,
             xboxVideoSettings.bpp, xboxVideoSettings.isWidescreen ? "yes" : "no",
             xboxVideoSettings.supports720p ? "yes" : "no",
             static_cast<int>(xboxVideoSettings.overscanRatio * 100.0f));
    Log::log(vidLog);
#endif
}

Renderer_Xbox::VideoDisplaySettings Renderer_Xbox::getVideoDisplaySettings() {
    initVideo();
    return xboxVideoSettings;
}

uint32_t Renderer_Xbox::getAVPackType() {
    initVideo();
    return xboxVideoSettings.avPackType;
}

const char *Renderer_Xbox::getAVPackName() {
    initVideo();
    switch (xboxVideoSettings.avPackType) {
        case AV_PACK_STANDARD: return "Composite (Standard)";
        case AV_PACK_RFU:      return "RF Adapter";
        case AV_PACK_SCART:    return "SCART (RGB)";
        case AV_PACK_HDTV:     return "HDTV (Component)";
        case AV_PACK_VGA:      return "VGA";
        case AV_PACK_SVIDEO:   return "S-Video";
        default:               return "None / Unknown";
    }
}

void Renderer_Xbox::setOverscanRatio(float ratio) {
    xboxVideoSettings.overscanRatio = ratio;
}

float Renderer_Xbox::getOverscanRatio() {
    return xboxVideoSettings.overscanRatio;
}

bool Renderer_Xbox::isWidescreen() {
    initVideo();
    return xboxVideoSettings.isWidescreen;
}

bool Renderer_Xbox::supports720p() {
    initVideo();
    return xboxVideoSettings.supports720p;
}

bool Renderer_Xbox::supports480p() {
    initVideo();
    return xboxVideoSettings.supports480p;
}

bool Renderer_Xbox::supports1080i() {
    initVideo();
    return xboxVideoSettings.supports1080i;
}

void Renderer_Xbox::getVideoMode(int &width, int &height, int &bpp, int &refresh) {
    initVideo();
#ifdef __XBOX__
    VIDEO_MODE currentMode = XVideoGetMode();
    width = currentMode.width > 0 ? currentMode.width : xboxVideoSettings.width;
    height = currentMode.height > 0 ? currentMode.height : xboxVideoSettings.height;
    bpp = currentMode.bpp > 0 ? currentMode.bpp : xboxVideoSettings.bpp;
    refresh = currentMode.refresh > 0 ? currentMode.refresh : xboxVideoSettings.refresh;
#else
    width = xboxVideoSettings.width;
    height = xboxVideoSettings.height;
    bpp = xboxVideoSettings.bpp;
    refresh = xboxVideoSettings.refresh;
#endif
}

void Renderer_Xbox::applyAspectRatio(SDL_Renderer *renderer, int logicalWidth, int logicalHeight, bool isWidescreen) {
    if (!renderer) return;
    if (logicalWidth <= 0 || logicalHeight <= 0) {
        logicalWidth = xboxVideoSettings.width;
        logicalHeight = xboxVideoSettings.height;
    }

    if (isWidescreen && logicalWidth == 640 && logicalHeight == 480) {
        // If the Xbox dashboard is outputting 16:9, but canvas is 4:3 (anamorphic 480p/480i),
        // SDL3 can pillarbox it to preserve the 4:3 aspect ratio.
        SDL_SetRenderLogicalPresentation(
            renderer,
            logicalWidth,
            logicalHeight,
            SDL_LOGICAL_PRESENTATION_LETTERBOX
        );
    } else {
        // Render natively at full window resolution without extra scaling overhead
        SDL_SetRenderLogicalPresentation(
            renderer,
            logicalWidth,
            logicalHeight,
            SDL_LOGICAL_PRESENTATION_DISABLED
        );
    }
}

void Renderer_Xbox::applyOverscan(SDL_Renderer *renderer, float overscanRatio) {
    if (!renderer) return;
    if (overscanRatio <= 0.0f) {
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
        SDL_SetRenderViewport(renderer, NULL);
        return;
    }

    int modeW = xboxVideoSettings.width;
    int modeH = xboxVideoSettings.height;
#ifdef __XBOX__
    VIDEO_MODE currentMode = XVideoGetMode();
    if (currentMode.width > 0 && currentMode.height > 0) {
        modeW = currentMode.width;
        modeH = currentMode.height;
    }
#endif

    const float totalW = static_cast<float>(modeW);
    const float totalH = static_cast<float>(modeH);

    const int offsetX = static_cast<int>(totalW * overscanRatio);
    const int offsetY = static_cast<int>(totalH * overscanRatio);
    const int safeW = static_cast<int>(totalW - (offsetX * 2));
    const int safeH = static_cast<int>(totalH - (offsetY * 2));

    const float scaleX = static_cast<float>(safeW) / totalW;
    const float scaleY = static_cast<float>(safeH) / totalH;

    // In SDL3, view->viewport coordinates are in render units and multiplied by current_scale
    // when computing the hardware pixel viewport. To position the hardware pixel viewport at
    // (offsetX, offsetY) with dimensions (safeW, safeH):
    SDL_Rect renderViewport;
    renderViewport.x = static_cast<int>(std::ceil(offsetX / scaleX));
    renderViewport.y = static_cast<int>(std::ceil(offsetY / scaleY));
    renderViewport.w = modeW;
    renderViewport.h = modeH;

    SDL_SetRenderScale(renderer, scaleX, scaleY);
    SDL_SetRenderViewport(renderer, &renderViewport);
}

void Renderer_Xbox::applyVideoSettings(SDL_Renderer *renderer) {
    if (!renderer) return;
    applyAspectRatio(renderer, xboxVideoSettings.width, xboxVideoSettings.height, xboxVideoSettings.isWidescreen);
    applyOverscan(renderer, xboxVideoSettings.overscanRatio);
}

void Renderer_Xbox::applyVideoSettings() {
    applyVideoSettings(Sdlrenderer);
}

void Renderer_Xbox::applyOverscan(float overscanRatio) {
    applyOverscan(Sdlrenderer, overscanRatio);
}

void Renderer_Xbox::applyAspectRatio(int logicalWidth, int logicalHeight, bool isWidescreen) {
    applyAspectRatio(Sdlrenderer, logicalWidth, logicalHeight, isWidescreen);
}

#ifdef SE_XBOX_TRACE_RENDERER
static bool traceStartupPixels(SDL_Renderer *renderer) {
    // Exercise the actual GPU path, including state changes used by games.
    bool passed = true;
    SDL_Rect savedViewport;
    float savedScaleX = 1.0f, savedScaleY = 1.0f;
    SDL_GetRenderViewport(renderer, &savedViewport);
    SDL_GetRenderScale(renderer, &savedScaleX, &savedScaleY);
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_SetRenderViewport(renderer, nullptr);
    auto pixel = [&](SDL_Surface *surface, const char *test, int x, int y,
                     int expectedR, int expectedG, int expectedB, int tolerance = 0) {
        Uint8 r = 0, g = 0, b = 0, a = 0;
        bool read = surface && SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a);
        bool matches = read && std::abs(int(r) - expectedR) <= tolerance &&
                       std::abs(int(g) - expectedG) <= tolerance && std::abs(int(b) - expectedB) <= tolerance;
        Log::log(std::string("GPU probe ") + test + "=" + (matches ? "pass" : "FAIL") +
                 " rgba=" + std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + "," + std::to_string(a));
        passed &= matches;
    };
    auto clearBlue = [&] {
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        passed &= SDL_RenderClear(renderer);
    };
    const SDL_FColor red = {1, 0, 0, 1};
    const SDL_Vertex triangle[] = {
        {{0, 0}, red, {0, 0}}, {{64, 0}, red, {0, 0}}, {{0, 64}, red, {0, 0}}
    };
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 2, 2);
    const Uint32 green[] = {0xff00ff00, 0xff00ff00, 0xff00ff00, 0xff00ff00};
    passed &= texture && SDL_UpdateTexture(texture, nullptr, green, 8);
    const SDL_FRect destination = {64, 0, 64, 64};
    clearBlue();
    passed &= SDL_RenderGeometry(renderer, nullptr, triangle, 3, nullptr, 0);
    if (texture) passed &= SDL_RenderTexture(renderer, texture, nullptr, &destination);
    SDL_Surface *pixels = SDL_RenderReadPixels(renderer, nullptr);
    pixel(pixels, "solid", 8, 8, 255, 0, 0);
    pixel(pixels, "textured", 72, 8, 0, 255, 0);
    pixel(pixels, "clear", 144, 8, 0, 0, 255);
    SDL_DestroySurface(pixels);

    clearBlue();
    const SDL_Rect viewport = {32, 32, 64, 64};
    passed &= SDL_SetRenderViewport(renderer, &viewport);
    passed &= SDL_RenderGeometry(renderer, nullptr, triangle, 3, nullptr, 0);
    passed &= SDL_SetRenderViewport(renderer, nullptr);
    pixels = SDL_RenderReadPixels(renderer, nullptr);
    pixel(pixels, "viewport-offset", 40, 40, 255, 0, 0);
    pixel(pixels, "viewport-outside", 8, 8, 0, 0, 255);
    SDL_DestroySurface(pixels);

    clearBlue();
    SDL_Texture *target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 64, 64);
    passed &= target != nullptr;
    if (target) {
        passed &= SDL_SetRenderTarget(renderer, target);
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        passed &= SDL_RenderClear(renderer);
        passed &= SDL_SetRenderTarget(renderer, nullptr);
        SDL_SetTextureBlendMode(target, SDL_BLENDMODE_NONE);
        const SDL_FRect targetDestination = {128, 0, 64, 64};
        passed &= SDL_RenderTexture(renderer, target, nullptr, &targetDestination);
        pixels = SDL_RenderReadPixels(renderer, nullptr);
        pixel(pixels, "target-restore", 136, 8, 0, 255, 0);
        pixel(pixels, "target-outside", 208, 8, 0, 0, 255);
        SDL_DestroySurface(pixels);
    }
    clearBlue();
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(texture, 128);
        passed &= SDL_RenderTexture(renderer, texture, nullptr, &destination);
        pixels = SDL_RenderReadPixels(renderer, nullptr);
        pixel(pixels, "alpha", 72, 8, 0, 128, 127, 1);
        SDL_DestroySurface(pixels);
    }
    SDL_DestroyTexture(target);
    SDL_DestroyTexture(texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_SetRenderScale(renderer, savedScaleX, savedScaleY);
    SDL_SetRenderViewport(renderer, &savedViewport);
    Log::log(passed ? "GPU startup probe passed." : "GPU startup probe FAILED.");
    return passed;
}
#endif

Renderer_Xbox::Renderer_Xbox() {
    initVideo();
    int windowWidth = xboxVideoSettings.width;
    int windowHeight = xboxVideoSettings.height;

    window = createWindow(windowWidth, windowHeight, "Scratch Everywhere!", WindowingBackends::SDL3);
    if (window->error.has_value()) {
        error = "Failed to init window: " + window->error.value();
        delete window;
        window = nullptr;
        return;
    }
    Sdlrenderer = static_cast<SDL_Renderer *>(static_cast<WindowSDL3 *>(window)->getRenderer());
    if (Sdlrenderer == nullptr) {
        error = "Could not create renderer: " + std::string(SDL_GetError());
        return;
    }
    int outputWidth = 0;
    int outputHeight = 0;
    if (SDL_GetRenderOutputSize(Sdlrenderer, &outputWidth, &outputHeight)) {
        Log::log("Xbox renderer output: " + std::to_string(outputWidth) + "x" + std::to_string(outputHeight));
    }
#ifdef __XBOX__
    VIDEO_MODE actualMode = XVideoGetMode();
    Log::log("Xbox active video mode: " + std::to_string(actualMode.width) + "x" + std::to_string(actualMode.height));
#endif
    applyVideoSettings();
#ifdef SE_XBOX_TRACE_RENDERER
    traceStartupPixels(Sdlrenderer);
#endif
}

Renderer_Xbox::~Renderer_Xbox() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }
    if (penTexture != nullptr) {
        ensureMainTarget(Sdlrenderer);
        SDL_DestroyTexture(penTexture);
        penTexture = nullptr;
    }

    TextObject::cleanupText();
    SDL_DestroyRenderer(Sdlrenderer);

    if (window) {
        window->cleanup();
        delete window;
        window = nullptr;
    }

    SDL_Quit();
}

void Renderer_Xbox::setRenderTarget(void *renderTarget, int width, int height) {
    mainRenderTarget = static_cast<SDL_Texture *>(renderTarget);
    overrideWidth = width;
    overrideHeight = height;
    hasOverrideTarget = true;
    penTargetActive = false; // the line below makes the main target current
    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
}

void Renderer_Xbox::clearRenderTarget() {
    mainRenderTarget = nullptr;
    hasOverrideTarget = false;
    penTargetActive = false;
    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
}

bool Renderer_Xbox::createSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerXbox();
    return speechManager != nullptr;
}

void Renderer_Xbox::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

SpeechManager *Renderer_Xbox::getSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerXbox();
    return speechManager;
}

int Renderer_Xbox::getWidth() {
    if (hasOverrideTarget) return overrideWidth;
    if (window) return window->getWidth();
    return 480;
}

int Renderer_Xbox::getHeight() {
    if (hasOverrideTarget) return overrideHeight;
    if (window) return window->getHeight();
    return 360;
}

float Renderer_Xbox::getPixelDensity() {
    if (window) return window->getPixelDensity();
    return 1.0f;
}

bool Renderer_Xbox::initPen() {
    if (penTexture != nullptr) return true;

    constexpr auto penFormat = SDL_PIXELFORMAT_ARGB8888;

    if (Scratch::hqpen) {
        if (Scratch::projectWidth / static_cast<double>(getWidth()) < Scratch::projectHeight / static_cast<double>(getHeight()))
            penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth * (getHeight() / static_cast<double>(Scratch::projectHeight)), getHeight());
        else
            penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, getWidth(), Scratch::projectHeight * (getWidth() / static_cast<double>(Scratch::projectWidth)));
    } else penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth, Scratch::projectHeight);

    if (!penTexture) {
        Scratch::freeUnusedCostumeImages();
        if (Scratch::hqpen) {
            if (Scratch::projectWidth / static_cast<double>(getWidth()) < Scratch::projectHeight / static_cast<double>(getHeight()))
                penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth * (getHeight() / static_cast<double>(Scratch::projectHeight)), getHeight());
            else
                penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, getWidth(), Scratch::projectHeight * (getWidth() / static_cast<double>(Scratch::projectWidth)));
        } else penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth, Scratch::projectHeight);
    }
    if (!penTexture) {
        Log::logError(std::string("Failed to create pen texture: ") + SDL_GetError());
        return false;
    }

    // Clear the texture
    SDL_SetTextureBlendMode(penTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(penTexture, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderTarget(Sdlrenderer, penTexture);
    SDL_SetRenderDrawColor(Sdlrenderer, 0, 0, 0, 0);
    SDL_RenderClear(Sdlrenderer);
    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
    penTargetActive = false;

    refreshPenMetrics();
    lastCapValid = false;
    penVerts.clear();
    penVerts.reserve(PEN_FLUSH_THRESHOLD + 1024);

    return true;
}

void Renderer_Xbox::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
    if (!penTexture) return;
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    const float alpha = (100.0f - sprite->penData.color.transparency) / 100.0f;

    const double scale = penScaleY;

    const float sx1 = static_cast<float>(x1 * scale + penTexW / 2.0);
    const float sy1 = static_cast<float>(-y1 * scale + penTexH / 2.0);
    const float sx2 = static_cast<float>(x2 * scale + penTexW / 2.0);
    const float sy2 = static_cast<float>(-y2 * scale + penTexH / 2.0);

    const double dx = sx2 - sx1;
    const double dy = sy2 - sy1;

    const double length = sqrt(dx * dx + dy * dy);
    const double drawWidth = (sprite->penData.size / 2.0f) * scale;

    const SDL_FColor sdlColor = {
        rgbColor.r / 255.0f,
        rgbColor.g / 255.0f,
        rgbColor.b / 255.0f,
        alpha};

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

    const unsigned int circleSegments = circleSegmentsFor(8.0, sprite->penData.size);
    const CirclePoint *table = getCircleTable(circleSegments);
    const float radius = static_cast<float>(drawWidth);

    // Skip the start cap when this move continues the previous one (opaque pens only).
    const bool continuesChain = alpha >= 1.0f && lastCapValid && lastCapX == sx1 && lastCapY == sy1 &&
                                lastCapRadius == radius && sameColor(lastCapColor, sdlColor);
    if (!continuesChain) emitCircle(sx1, sy1, radius, sdlColor, table, circleSegments);
    emitCircle(sx2, sy2, radius, sdlColor, table, circleSegments);

    lastCapValid = true;
    lastCapX = sx2;
    lastCapY = sy2;
    lastCapRadius = radius;
    lastCapColor = sdlColor;

    if (penVerts.size() >= PEN_FLUSH_THRESHOLD) {
        flushPenVerts(Sdlrenderer);
    }
}

void Renderer_Xbox::penDotAccurate(Sprite *sprite) {
    if (!penTexture) return;
    lastCapValid = false;
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    const float alpha = (100.0f - sprite->penData.color.transparency) / 100.0f;

    const double scale = penScaleY;

    const float sx = static_cast<float>(sprite->xPosition * scale + penTexW / 2.0);
    const float sy = static_cast<float>(-sprite->yPosition * scale + penTexH / 2.0);

    const float radius = static_cast<float>((sprite->penData.size / 2.0f) * scale);

    const SDL_FColor sdlColor = {
        rgbColor.r / 255.0f,
        rgbColor.g / 255.0f,
        rgbColor.b / 255.0f,
        alpha};

    const unsigned int circleSegments = circleSegmentsFor(16.0, sprite->penData.size);
    emitCircle(sx, sy, radius, sdlColor, getCircleTable(circleSegments), circleSegments);

    if (penVerts.size() >= PEN_FLUSH_THRESHOLD) {
        flushPenVerts(Sdlrenderer);
    }
}

void Renderer_Xbox::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
    if (!penTexture) return;
    lastCapValid = false;
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0 - sprite->penData.color.transparency) / 100.0;

    const double scale = penScaleY;

    const float sx1 = static_cast<float>(x1 * scale + penTexW / 2.0);
    const float sy1 = static_cast<float>(-y1 * scale + penTexH / 2.0);
    const float sx2 = static_cast<float>(x2 * scale + penTexW / 2.0);
    const float sy2 = static_cast<float>(-y2 * scale + penTexH / 2.0);

    const double dx = sx2 - sx1;
    const double dy = sy2 - sy1;

    const double length = sqrt(dx * dx + dy * dy);
    const double drawWidth = (sprite->penData.size / 2.0f) * scale;

    if (length <= 0) return;

    const float nx = static_cast<float>((-dy / length) * drawWidth);
    const float ny = static_cast<float>((dx / length) * drawWidth);

    const SDL_FColor sdlColor = {
        static_cast<float>(rgbColor.r / 255.0f),
        static_cast<float>(rgbColor.g / 255.0f),
        static_cast<float>(rgbColor.b / 255.0f),
        static_cast<float>(alpha)};

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

    if (penVerts.size() >= PEN_FLUSH_THRESHOLD) {
        flushPenVerts(Sdlrenderer);
    }
}

void Renderer_Xbox::penDotFast(Sprite *sprite) {
    if (!penTexture) return;
    lastCapValid = false;
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0 - sprite->penData.color.transparency) / 100.0;

    const double scale = penScaleY;

    const float sx = static_cast<float>(sprite->xPosition * scale + penTexW / 2.0);
    const float sy = static_cast<float>(-sprite->yPosition * scale + penTexH / 2.0);

    const float halfSize = static_cast<float>((sprite->penData.size / 2.0f) * scale);

    const SDL_FColor sdlColor = {
        static_cast<float>(rgbColor.r / 255.0f),
        static_cast<float>(rgbColor.g / 255.0f),
        static_cast<float>(rgbColor.b / 255.0f),
        alpha};

    const SDL_Vertex v0 = {{sx - halfSize, sy - halfSize}, sdlColor, {0.0f, 0.0f}};
    const SDL_Vertex v1 = {{sx - halfSize, sy + halfSize}, sdlColor, {0.0f, 0.0f}};
    const SDL_Vertex v2 = {{sx + halfSize, sy - halfSize}, sdlColor, {0.0f, 0.0f}};
    const SDL_Vertex v3 = {{sx + halfSize, sy + halfSize}, sdlColor, {0.0f, 0.0f}};

    penVerts.push_back(v0);
    penVerts.push_back(v1);
    penVerts.push_back(v2);

    penVerts.push_back(v1);
    penVerts.push_back(v3);
    penVerts.push_back(v2);

    if (penVerts.size() >= PEN_FLUSH_THRESHOLD) {
        flushPenVerts(Sdlrenderer);
    }
}

void Renderer_Xbox::penStamp(Sprite *sprite) {
    if (!penTexture) return;
    lastCapValid = false;

    // Queued lines must be drawn first so the stamp lands on top of them.
    flushPenVerts(Sdlrenderer);

    auto imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }

    const Costume &costume = sprite->costumes[sprite->currentCostume];

    //  The NV2A Xbox GPU corrupts the texture cache if multiple different
    // textures are stamped into a render target without unbinding it. We must explicitly
    // bind, draw, and immediately unbind back to the main target for EVERY stamp.
    SDL_SetRenderTarget(Sdlrenderer, penTexture);

    Image_Xbox *image = reinterpret_cast<Image_Xbox *>(imgFind->second.get());

    const bool isSVG = costume.isSVG;
    Render::calculateRenderPosition(sprite, isSVG);

    // Pen mapping stuff
    const auto &cords = Scratch::screenToScratchCoords(sprite->renderInfo.renderX, sprite->renderInfo.renderY, getWidth(), getHeight());
    float penX = cords.first + Scratch::projectWidth / 2.0f;
    float penY = -cords.second + Scratch::projectHeight / 2.0f;

    float penScale;
    if (Scratch::hqpen) {
        const double scale = penScaleY;
        penX *= scale;
        penY *= scale;
        penScale = sprite->renderInfo.renderScaleY;
    } else {
        penScale = (sprite->size / 100.0f) / costume.bitmapResolution;
        if (Scratch::bitmapHalfQuality && !costume.isSVG && costume.bitmapResolution == 2) {
            penScale *= 2.0f;
        }
    }

    ImageRenderParams params;
    params.centered = true;
    params.x = penX;
    params.y = penY;
    params.rotation = sprite->renderInfo.renderRotation;
    params.scale = penScale;
    params.flip = (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0);
    params.opacity = 1.0f - (std::clamp(sprite->ghostEffect, 0.0f, 100.0f) * 0.01f);
    params.brightness = sprite->brightnessEffect;

    image->render(params);

    // Force the Xbox GPU to resolve the render target immediately
    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
    penTargetActive = false;
    stampsSinceFlush = 0;
}

void Renderer_Xbox::penClear() {
    if (!penTexture) return;
    penVerts.clear(); // pending lines would be erased by the clear anyway
    lastCapValid = false;

    SDL_SetRenderTarget(Sdlrenderer, penTexture);
    SDL_SetRenderDrawColor(Sdlrenderer, 0, 0, 0, 0);
    SDL_RenderClear(Sdlrenderer);

    // Force the Xbox GPU to resolve the render target immediately
    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
    penTargetActive = false;
}

void Renderer_Xbox::cleanupPen() {
    if (penTexture != nullptr) {
        ensureMainTarget(Sdlrenderer);
        SDL_DestroyTexture(penTexture);
        penTexture = nullptr;
    }
    penTargetActive = false;
    lastCapValid = false;
    penVerts.clear();
    penVerts.shrink_to_fit();
}

void Renderer_Xbox::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        ensureMainTarget(Sdlrenderer);
        SDL_SetRenderDrawColor(Sdlrenderer, colorR, colorG, colorB, 255);
        SDL_RenderClear(Sdlrenderer);
        hasFrameBegan = true;
    }
}

void Renderer_Xbox::endFrame(bool shouldFlush) {
    ensureMainTarget(Sdlrenderer);
    SDL_RenderPresent(Sdlrenderer);
    SDL_Delay(16);
    hasFrameBegan = false;
}

void Renderer_Xbox::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    ensureMainTarget(Sdlrenderer);
    SDL_SetRenderDrawColor(Sdlrenderer, colorR, colorG, colorB, colorA);
    SDL_FRect rect = {x - (w / 2.0f), y - (h / 2.0f), static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderFillRect(Sdlrenderer, &rect);
}

void Renderer_Xbox::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    radius = std::clamp(radius, 0, std::min(w, h) / 2);
    if (radius <= 0) {
        drawBox(w, h, x, y, colorR, colorG, colorB, colorA);
        return;
    }

    ensureMainTarget(Sdlrenderer);

    const SDL_FColor col = {colorR / 255.0f, colorG / 255.0f, colorB / 255.0f, colorA / 255.0f};
    const float left = x - w / 2.0f, right = x + w / 2.0f;
    const float top = y - h / 2.0f, bottom = y + h / 2.0f;
    const float r = static_cast<float>(radius);

    constexpr int cornerSegments = 4;
    std::vector<SDL_FPoint> outline;
    outline.reserve((cornerSegments + 1) * 4);

    auto addArc = [&](float cx, float cy, float startAngle) {
        for (int i = 0; i <= cornerSegments; ++i) {
            const float a = startAngle + (static_cast<float>(M_PI) / 2.0f) * (i / static_cast<float>(cornerSegments));
            outline.push_back({cx + std::cos(a) * r, cy + std::sin(a) * r});
        }
    };

    addArc(right - r, top + r, -static_cast<float>(M_PI) / 2.0f);
    addArc(right - r, bottom - r, 0.0f);
    addArc(left + r, bottom - r, static_cast<float>(M_PI) / 2.0f);
    addArc(left + r, top + r, static_cast<float>(M_PI));

    std::vector<SDL_Vertex> verts;
    verts.reserve((outline.size() - 2) * 3);
    for (size_t i = 1; i + 1 < outline.size(); ++i) {
        verts.push_back({outline[0], col, {0.0f, 0.0f}});
        verts.push_back({outline[i], col, {0.0f, 0.0f}});
        verts.push_back({outline[i + 1], col, {0.0f, 0.0f}});
    }

    SDL_RenderGeometry(Sdlrenderer, nullptr, verts.data(), static_cast<int>(verts.size()), nullptr, 0);
}

void Renderer_Xbox::renderSprites() {
    // Everything below draws to the main target; pen operations may have left the pen texture active.
    ensureMainTarget(Sdlrenderer);

    SDL_SetRenderDrawColor(Sdlrenderer, 0, 0, 0, 255);
    SDL_RenderClear(Sdlrenderer);

    float screenW = static_cast<float>(getWidth());
    float screenH = static_cast<float>(getHeight());
    float screenAspect = screenW / screenH;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;
    SDL_FRect stageRect = {0, 0, screenW, screenH};
    if (screenAspect > projectAspect) {
        float scale = screenH / Scratch::projectHeight;
        float scaledW = Scratch::projectWidth * scale;
        stageRect.x = (screenW - scaledW) / 2.0f;
        stageRect.w = scaledW;
    } else if (screenAspect < projectAspect) {
        float scale = screenW / Scratch::projectWidth;
        float scaledH = Scratch::projectHeight * scale;
        stageRect.y = (screenH - scaledH) / 2.0f;
        stageRect.h = scaledH;
    }
    SDL_SetRenderDrawColor(Sdlrenderer, 255, 255, 255, 255);
    SDL_RenderFillRect(Sdlrenderer, &stageRect);

    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;
        auto imgFind = Scratch::costumeImages.find(currentSprite->costumes[currentSprite->currentCostume].fullName);
        if (imgFind != Scratch::costumeImages.end()) {
            Image *image = imgFind->second.get();

            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            Render::calculateRenderPosition(currentSprite, isSVG);
            if (!currentSprite->visible) continue;

            ImageRenderParams params;
            params.centered = true;
            params.x = currentSprite->renderInfo.renderX;
            params.y = currentSprite->renderInfo.renderY;
            params.rotation = currentSprite->renderInfo.renderRotation;
            params.scale = currentSprite->renderInfo.renderScaleY;
            params.flip = (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0);
            params.opacity = 1.0f - (std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f) * 0.01f);
            params.brightness = currentSprite->brightnessEffect;

            image->render(params);
        }

        if (currentSprite->isStage) renderPenLayer();
    }

    if (speechManager) {
        speechManager->render();
    }

    drawBlackBars(Sdlrenderer, getWidth(), getHeight());
    Render::renderMonitors();

#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
    if (Input::mousePointer.isMoving) {
        SDL_SetRenderDrawColor(Sdlrenderer, 0, 0, 255, 255);
        SDL_FRect rect;
        rect.w = rect.h = 5;
        rect.x = (Input::mousePointer.x * Render::renderScale) + (window->getWidth() * 0.5);
        rect.y = (Input::mousePointer.y * -1 * Render::renderScale) + (window->getHeight() * 0.5);
        Input::mousePointer.x = std::clamp((float)Input::mousePointer.x, -Scratch::projectWidth * 0.5f, Scratch::projectWidth * 0.5f);
        Input::mousePointer.y = std::clamp((float)Input::mousePointer.y, -Scratch::projectHeight * 0.5f, Scratch::projectHeight * 0.5f);
        SDL_RenderRect(Sdlrenderer, &rect);
    }
#endif

    SDL_RenderPresent(Sdlrenderer);
}

void Renderer_Xbox::renderPenLayer() {
    if (!penTexture) return;
    flushPenVerts(Sdlrenderer);
    ensureMainTarget(Sdlrenderer);

    SDL_FRect renderRect = {0, 0, 0, 0};

    if (static_cast<float>(getWidth()) / getHeight() > static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight) {
        renderRect.x = std::ceil((getWidth() - Scratch::projectWidth * (static_cast<float>(getHeight()) / Scratch::projectHeight)) / 2.0f);
        renderRect.w = getWidth() - renderRect.x * 2;
        renderRect.h = getHeight();
    } else {
        renderRect.y = std::ceil((getHeight() - Scratch::projectHeight * (static_cast<float>(getWidth()) / Scratch::projectWidth)) / 2.0f);
        renderRect.h = getHeight() - renderRect.y * 2;
        renderRect.w = getWidth();
    }

    SDL_RenderTexture(Sdlrenderer, penTexture, nullptr, &renderRect);
}

bool Renderer_Xbox::appShouldRun() {
    if (OS::toExit) return false;
    if (window) {
        // Make sure nothing outside the pen code draws into the pen texture.
        ensureMainTarget(Sdlrenderer);

        window->pollEvents();

        static int lastW = 0, lastH = 0;
        int currentW = getWidth();
        int currentH = getHeight();

        if (lastW != currentW || lastH != currentH) {
            lastW = currentW;
            lastH = currentH;

            if (Scratch::hqpen) {
                // Recreate pen texture
                SDL_DestroyTexture(penTexture);
                penTexture = nullptr;
                initPen();
            }
        }

        return !window->shouldClose();
    }
    return false;
}
