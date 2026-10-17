#pragma once
#if __has_include(<glad/gl.h>)
#include <glad/gl.h>
#else
#include <glad/glad.h>
#endif

#include "nonstd/expected.hpp"
#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT Renderer_GLCore : public Renderer {
  public:
    Renderer_GLCore();
    ~Renderer_GLCore() override;

    bool initPen() override;

    void setRenderTarget(void *renderTarget) override;
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
};