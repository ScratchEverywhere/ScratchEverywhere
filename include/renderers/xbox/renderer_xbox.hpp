#pragma once
#include "nonstd/expected.hpp"
#include <SDL3/SDL.h>
#include <renderer.hpp>
#include <se_export.hpp>

#include <cstdint>

#ifndef AV_PACK_NONE
#define AV_PACK_NONE 0
#define AV_PACK_STANDARD 1
#define AV_PACK_RFU 2
#define AV_PACK_SCART 3
#define AV_PACK_HDTV 4
#define AV_PACK_VGA 5
#define AV_PACK_SVIDEO 6
#endif

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

class SE_EXPORT Renderer_Xbox : public Renderer {
  public:
    struct VideoDisplaySettings {
        int width = 640;
        int height = 480;
        int bpp = 32;
        int refresh = 60;
        bool isWidescreen = false;
        bool supports720p = false;
        bool supports480p = false;
        bool supports1080i = false;
        float overscanRatio = 0.05f;
        uint32_t avPackType = 0;
    };

    SDL_Renderer *Sdlrenderer = nullptr;

    Renderer_Xbox();
    ~Renderer_Xbox() override;

    static void initVideo();
    static VideoDisplaySettings getVideoDisplaySettings();
    static uint32_t getAVPackType();
    static const char *getAVPackName();
    static void setOverscanRatio(float ratio);
    static float getOverscanRatio();
    static bool isWidescreen();
    static bool supports720p();
    static bool supports480p();
    static bool supports1080i();
    static void getVideoMode(int &width, int &height, int &bpp, int &refresh);

    void applyVideoSettings();
    void applyOverscan(float overscanRatio);
    void applyAspectRatio(int logicalWidth, int logicalHeight, bool isWidescreen);

    static void applyVideoSettings(SDL_Renderer *renderer);
    static void applyOverscan(SDL_Renderer *renderer, float overscanRatio);
    static void applyAspectRatio(SDL_Renderer *renderer, int logicalWidth, int logicalHeight, bool isWidescreen);

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
