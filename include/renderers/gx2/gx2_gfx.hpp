#pragma once

// lightweight wrapper around GX2

#include <cstddef>
#include <cstdint>

#include <gx2/draw.h>
#include <gx2/enum.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/surface.h>
#include <gx2/texture.h>

struct GX2ContextState;

namespace GX2Gfx {

enum class Blend : uint8_t {
    Unknown,
    None,
    Alpha,
    Premultiplied,
};

enum class Filter : uint8_t {
    Nearest,
    Linear,
};

struct Texture {
    GX2Texture gx2{};
    bool valid = false;
};

struct RenderTarget {
    GX2ColorBuffer colorBuffer{};
    Texture texture;
    int width = 0;
    int height = 0;
    bool valid = false;
};

struct Program {
    GX2VertexShader *vertexShader = nullptr;
    GX2PixelShader *pixelShader = nullptr;
    GX2FetchShader fetchShader{};
    void *fetchShaderMemory = nullptr;
    bool valid = false;
};

struct SpriteProgram : Program {
    uint32_t uModel = 0, uProjection = 0;
    uint32_t uParams0 = 0, uParams1 = 0, uParams2 = 0;
    uint32_t samplerLocation = 0;
};

struct SolidProgram : Program {
    uint32_t uProjection = 0;
};

struct TextProgram : Program {
    uint32_t uProjection = 0;
    uint32_t uColor = 0;
    uint32_t samplerLocation = 0;
};

const char *init(int sceneWidth, int sceneHeight);
void shutdown();
bool isInitialised();

const SpriteProgram &spriteProgram();
const SolidProgram &solidProgram();
const TextProgram &textProgram();

void beginFrame();
bool frameStarted();

void present(RenderTarget &source);

void presentSplit(RenderTarget &source);

void *allocate(size_t size, size_t alignment);
void release(void *memory);

void deferFree(void *memory);

bool createTexture(Texture &out, int width, int height, GX2SurfaceFormat format, const void *pixels, int pitchBytes);

bool replaceTexture(Texture &texture, int width, int height, GX2SurfaceFormat format, const void *pixels, int pitchBytes);

void destroyTexture(Texture &texture);

bool createRenderTarget(RenderTarget &out, int width, int height);
void destroyRenderTarget(RenderTarget &target);

void makeReadable(RenderTarget &target);

void waitIdle();

void setRenderTarget(GX2ColorBuffer *colorBuffer, int width, int height);
void setViewport(int width, int height);
void getViewportSize(int &width, int &height);

void clear(float r, float g, float b, float a);

void setBlend(Blend mode);
void bindProgram(const Program &program);
void bindTexture(const Texture &texture, uint32_t samplerLocation, Filter filter);

void setVertexUniform(uint32_t offsetDwords, const float *data, uint32_t countDwords);
void setPixelUniform(uint32_t offsetDwords, const float *data, uint32_t countDwords);

struct SpriteParams {
    float opacity = 1.0f;
    float brightness = 0.0f;
    float color = 0.0f;
    float fisheye = 0.0f;
    float whirl = 0.0f;
    float pixelate = 0.0f;
    float mosaic = 0.0f;
    float texWidth = 1.0f;
    float texHeight = 1.0f;
};
void setSpriteUniforms(const float model[16], const float projection[16], const SpriteParams &params);

void *streamAllocate(size_t bytes);
void drawStream(const void *vertices, size_t bytes, uint32_t stride, GX2PrimitiveMode mode, uint32_t vertexCount);

void drawUnitQuad();

void buildOrtho(float out[16], float left, float right, float bottom, float top);

} // namespace GX2Gfx
