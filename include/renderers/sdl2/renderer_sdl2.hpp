#pragma once
#include "nonstd/expected.hpp"
#ifdef __VFLASH__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT Renderer_SDL2 : public Renderer {
  public:
    SDL_Renderer *Sdlrenderer = nullptr;
#ifdef __VFLASH__
    bool loadingStageBackdrop = false;
    SDL_Texture *stageBackdropTexture = nullptr;
    std::string activeStageBackdropId;
    uint32_t activeStageBackdropVersion = 0;
    void setLoadingStageBackdrop(bool loading) override { loadingStageBackdrop = loading; }
#ifdef SE_VFLASH_TELEMETRY
    uint32_t prevGpuLists = 0;
    uint32_t prevGpuSubmitTicks = 0;
    uint32_t prevGpuWaitTicks = 0;
    bool getAtlasTelemetry(uint32_t &usedTexels, uint32_t &totalTexels, uint32_t &liveCount, uint32_t &failCount) override;
    bool getPerfTelemetry(VFlashPerfTelemetry &out) override;
#endif
#endif

    Renderer_SDL2();
    ~Renderer_SDL2() override;

    bool initPen() override;

    void setRenderTarget(void *renderTarget, int width, int height) override;
    void clearRenderTarget() override;

    bool createSpeechManager() override;
    SpeechManager *getSpeechManager() override;
    void destroySpeechManager() override;

    void beginFrame(int screen, int colorR, int colorG, int colorB) override;
    void endFrame(bool shouldFlush = true) override;

    int getWidth() override;
    int getHeight() override;
    float getPixelDensity() override;

    void renderSprites() override;
    void renderPenLayer() override;

    void drawBox(int w, int h, int x, int y, uint8_t colorR = 0, uint8_t colorG = 0, uint8_t colorB = 0, uint8_t colorA = 255) override;
    void drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR = 0, uint8_t colorG = 0, uint8_t colorB = 0, uint8_t colorA = 255) override;

    bool appShouldRun() override;

    void penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) override;
    void penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) override;
    void penDotFast(Sprite *sprite) override;
    void penDotAccurate(Sprite *sprite) override;
    void penStamp(Sprite *sprite) override;
    void penClear() override;
    void cleanupPen() override;
    void preparePenColorQuery(float minX, float minY, float maxX, float maxY) override;
    bool samplePenColor(float x, float y, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a) override;
};