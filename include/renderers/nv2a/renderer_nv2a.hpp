#pragma once
#include "nonstd/expected.hpp"
#include <renderer.hpp>
#include <se_export.hpp>
#include <cstdint>
#include <vector>

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

#ifndef NV_PFB_WC_CACHE
#define NV_PFB_WC_CACHE 0x00100410
#endif
#ifndef NV_PFB_WC_CACHE_FLUSH_TRIGGER
#define NV_PFB_WC_CACHE_FLUSH_TRIGGER 0x00010000
#endif
#ifndef NV_PFB_WC_CACHE_FLUSH_IN_PROGRESS
#define NV_PFB_WC_CACHE_FLUSH_IN_PROGRESS 0x00010000
#endif
#ifndef NV097_BREAK_VERTEX_BUFFER_CACHE
#define NV097_BREAK_VERTEX_BUFFER_CACHE 0x00001710
#endif

enum class NV2AScaleMode {
    Nearest,
    Linear,
    PixelArt,
};

struct NV2ATexture {
    int texWidth = 0;
    int texHeight = 0;
    int dataWidth = 0;
    int dataHeight = 0;
    int bytesPerPixel = 4;
    int pitch = 0;
    bool swizzled = false;
    bool isDXT = false;
    uint32_t format = 0;
    float uScale = 1.0f;
    float vScale = 1.0f;
    NV2AScaleMode scaleMode = NV2AScaleMode::PixelArt;
    uint8_t *data = nullptr;
    uint8_t *dataPhysicalAddress = nullptr;
    size_t allocationSize = 0;
};

struct NV2AVertex {
    float pos[2];     // x, y
    uint8_t color[4]; // r, g, b, a
    float tex[2];     // u, v
};

enum class NV2ABlendMode {
    None,
    Blend,
    Add,
};

class SE_EXPORT Renderer_NV2A : public Renderer {
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

    Renderer_NV2A();
    ~Renderer_NV2A() override;

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

    // NV2A Direct GPU Texture Management
    NV2ATexture *createTexture(int width, int height, uint32_t format, bool swizzled);
    void destroyTexture(NV2ATexture *&texture);
    bool updateTexture(NV2ATexture *texture, const void *pixels, int pitch);
    void setTextureScaleMode(NV2ATexture *texture, NV2AScaleMode scaleMode);
    NV2AScaleMode getTextureScaleMode(const NV2ATexture *texture) const;

    // Direct GPU Rendering Helpers
    void bindTexture(const NV2ATexture *texture, NV2AScaleMode scaleMode, bool wrap = false);
    void bindTexture(const NV2ATexture *texture, bool linear, bool wrap = false);
    void bindTexture(const NV2ATexture *texture);
    void unbindTexture();
    void setBlendMode(NV2ABlendMode mode);
    void setScissor(int x, int y, int w, int h);
    void clearScissor();
    void setViewport(int x, int y, int w, int h);
    void drawTriangles(const NV2AVertex *vertices, size_t count);
    void drawIndexedTriangles(const NV2AVertex *vertices, const uint16_t *indices, size_t count);
    void drawColorTriangles(const NV2AVertex *vertices, size_t count);

    // Incremented once per beginFrame(); textures used during the current frame must not be overwritten.
    static uint32_t getFrameCounter();

    // Renderer Interface
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
};
