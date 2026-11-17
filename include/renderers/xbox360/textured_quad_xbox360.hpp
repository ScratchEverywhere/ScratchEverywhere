#pragma once
#include <image.hpp>
#include <xsdk/gpu.h>
#include <xsdk/shader.h>
#include <xsdk/texture.h>

// Native GPU resource consuming the engine byte-array image format.
class TexturedQuadXbox360 {
    XsdkTexture texture_{};
    XsdkShader vertex_{}, pixel_{};
    XsdkSamplerState sampler_{};
    XsdkBlendState blend_{};
    XsdkBlendState blendOpaque_{};
    XsdkDepthStencilState depth_{};
    XsdkRasterState raster_{};
    volatile uint32_t *vertices_ = nullptr;
    static constexpr uint32_t vertexSlots_ = 32;
    uint32_t vertexSlot_ = 0;
    int texWidth_ = 0, texHeight_ = 0;
    bool blendEnabled_ = true;
    float opacity_ = 1, brightness_ = 0, colorEffect_ = 0;
    float mosaicEffect_ = 0, pixelateEffect_ = 0, whirlEffect_ = 0, fisheyeEffect_ = 0;
    uint32_t reuseFences_[vertexSlots_]{};
    bool usedSlots_[vertexSlots_]{};
    void release();
    void render();

  public:
    void renderRegion(const ImageRenderParams &params, float width, float height, int screenWidth, int screenHeight, float u0, float v0, float u1, float v1);
    TexturedQuadXbox360(const ImageData &image, int screenWidth, int screenHeight, bool linear = false);
    ~TexturedQuadXbox360();
    TexturedQuadXbox360(const TexturedQuadXbox360 &) = delete;
    TexturedQuadXbox360 &operator=(const TexturedQuadXbox360 &) = delete;
    void render(const ImageRenderParams &params, int imageWidth, int imageHeight, int screenWidth, int screenHeight);
};
