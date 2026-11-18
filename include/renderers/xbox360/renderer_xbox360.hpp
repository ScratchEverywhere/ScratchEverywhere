#pragma once
#include "nonstd/expected.hpp"
#include <renderer.hpp>
#include <se_export.hpp>

#include <speech_manager_xbox360.hpp>
#include <vector>
#include <window_xbox360.hpp>
#include <xsdk/gpu.h>
#include <xsdk/shader.h>
#include <xsdk/texture.h>

class SE_EXPORT Renderer_Xbox360 final : public Renderer {
    WindowXbox360 *surface_ = nullptr;
    SpeechManagerXbox360 *speechManager_ = nullptr;
    bool textOpen_ = false;
    uint32_t background_ = 0;
    int targetWidth_ = 0, targetHeight_ = 0;
    XsdkTexture *overrideTarget_ = nullptr;

    struct PenVertex {
        float x, y, z, w;
        float r, g, b, a;
    };
    bool penInitialized_ = false;
    int penW_ = 0, penH_ = 0;
    float penScaleY_ = 1.0f;
    XsdkTexture penTexture_{};
    XsdkShader penVert_{}, penFrag_{};
    XsdkShader penTexVert_{}, penTexFrag_{};
    XsdkSamplerState penSampler_{};
    XsdkSamplerState penPointSampler_{};
    XsdkBlendState penBlend_{}, penBlendOpaque_{};
    XsdkDepthStencilState penDepth_{};
    XsdkRasterState penRaster_{};
    static constexpr size_t kPenQuadSlots = 16;
    volatile uint32_t *penQuadVerts_ = nullptr;
    volatile uint32_t *penDrawVerts_ = nullptr;
    uint32_t penQuadFences_[kPenQuadSlots]{};
    bool penQuadUsed_[kPenQuadSlots]{};
    size_t penQuadSlot_ = 0;
    uint32_t penDrawFence_ = 0;
    bool penDrawUsed_ = false;
    bool penTargetActive_ = false;
    std::vector<PenVertex> penVerts_;
    std::vector<uint8_t> penReadback_;
    bool penReadbackDirty_ = true;
    int penReadbackRowStart_ = 0;
    int penReadbackRowEnd_ = -1;

    void releasePen();
    void flushPen();
    void drawPenQuad(float x, float y, float w, float h, int sw, int sh, bool opaque);
    void pushPenTri(const PenVertex &a, const PenVertex &b, const PenVertex &c);
    void pushPenQuad(const PenVertex &v0, const PenVertex &v1, const PenVertex &v2, const PenVertex &v3);
    void addPenCircle(float cx, float cy, float radius, float r, float g, float b, float a, unsigned segments);
    void fillRect(int left, int top, int w, int h, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA);
    [[noreturn]] void unsupported(const char *feature);

  public:
    Renderer_Xbox360();
    ~Renderer_Xbox360() override;

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
    void preparePenColorQuery(float minX, float minY, float maxX, float maxY) override;
    bool samplePenColor(float x, float y, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a) override;
};
