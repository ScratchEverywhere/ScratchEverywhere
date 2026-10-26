#include "gx2_gfx.hpp"
#include "gx2_shaders.hpp"

#include <cafeglsl/CafeGLSLCompiler.h>
#include <coreinit/memdefaultheap.h>
#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/state.h>
#include <gx2/swap.h>
#include <gx2/utils.h>
#include <proc_ui/procui.h>
#include <whb/gfx.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <log.hpp>
#include <string>
#include <vector>

namespace GX2Gfx {

namespace {

constexpr size_t kStreamSize = 8 * 1024 * 1024;

struct AttribDesc {
    const char *name;
    uint32_t offset;
    GX2AttribFormat format;
};

struct State {
    bool initialised = false;
    bool compilerReady = false;
    bool frameStarted = false;

    SpriteProgram sprite;
    SolidProgram solid;
    TextProgram text;

    GX2Sampler nearest{};
    GX2Sampler linear{};

    GX2ContextState *context = nullptr;

    uint8_t *stream = nullptr;
    size_t streamHead = 0;

    float *unitQuad = nullptr;

    std::vector<void *> deferred;

    Blend blend = Blend::Unknown;
    const Program *boundProgram = nullptr;
    GX2ColorBuffer *colorBuffer = nullptr;
    int viewportWidth = 0;
    int viewportHeight = 0;
};

State g;

inline size_t alignUp(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

inline GX2InvalidateMode invalidateMode(uint32_t bits) {
    return static_cast<GX2InvalidateMode>(bits);
}

void invalidateStateCache() {
    g.blend = Blend::Unknown;
    g.boundProgram = nullptr;
}

void flushDeferred() {
    for (void *memory : g.deferred)
        release(memory);
    g.deferred.clear();
}

uint32_t attribSelMask(GX2AttribFormat format) {
    switch (format) {
    case GX2_ATTRIB_FORMAT_FLOAT_32_32:
        return GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
    case GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32:
    default:
        return GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_W);
    }
}

bool findVertexUniform(const GX2VertexShader *shader, const char *name, uint32_t &offset) {
    const GX2UniformVar *var = GX2GetVertexUniformVar(shader, name);
    if (!var) {
        Log::logError(std::string("[GX2] Vertex uniform not found: ") + name);
        return false;
    }
    offset = var->offset;
    return true;
}

bool findPixelUniform(const GX2PixelShader *shader, const char *name, uint32_t &offset) {
    const GX2UniformVar *var = GX2GetPixelUniformVar(shader, name);
    if (!var) {
        Log::logError(std::string("[GX2] Pixel uniform not found: ") + name);
        return false;
    }
    offset = var->offset;
    return true;
}

bool findSampler(const GX2PixelShader *shader, const char *name, uint32_t &location) {
    for (uint32_t i = 0; i < shader->samplerVarCount; ++i) {
        if (std::strcmp(shader->samplerVars[i].name, name) == 0) {
            location = shader->samplerVars[i].location;
            return true;
        }
    }
    Log::logError(std::string("[GX2] Sampler not found: ") + name);
    return false;
}

bool buildProgram(Program &program, const char *name, const char *vertexSource, const char *pixelSource,
                  const AttribDesc *attribs, uint32_t attribCount) {
    char log[2048] = {};
    GX2VertexShader *vs = nullptr;
    GX2PixelShader *ps = nullptr;

    const GX2ShaderMode mode = CompileShaderPair(vertexSource, pixelSource, GLSL_COMPILE_AUTO, &vs, &ps,
                                                 log, sizeof(log), GLSL_COMPILER_FLAG_NONE);
    if (mode == GLSL_SHADER_MODE_ERROR) {
        Log::logError(std::string("[GX2] Failed to compile '") + name + "' shaders: " + log);
        return false;
    }
    if (log[0] != '\0') Log::logWarning(std::string("[GX2] '") + name + "' shader log: " + log);

    if (mode != GX2_SHADER_MODE_UNIFORM_REGISTER) {
        Log::logError(std::string("[GX2] '") + name + "' needs uniform blocks, which this backend does not use");
        FreeShaders(vs, ps);
        return false;
    }

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, vs->program, vs->size);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, ps->program, ps->size);

    GX2AttribStream streams[4] = {};
    for (uint32_t i = 0; i < attribCount; ++i) {
        int32_t location = -1;
        for (uint32_t v = 0; v < vs->attribVarCount; ++v) {
            if (std::strcmp(vs->attribVars[v].name, attribs[i].name) == 0) {
                location = static_cast<int32_t>(vs->attribVars[v].location);
                break;
            }
        }
        if (location < 0) {
            Log::logError(std::string("[GX2] '") + name + "' has no attribute " + attribs[i].name);
            FreeShaders(vs, ps);
            return false;
        }

        streams[i].location = static_cast<uint32_t>(location);
        streams[i].buffer = 0;
        streams[i].offset = attribs[i].offset;
        streams[i].format = attribs[i].format;
        streams[i].type = GX2_ATTRIB_INDEX_PER_VERTEX;
        streams[i].aluDivisor = 0;
        streams[i].mask = attribSelMask(attribs[i].format);
        streams[i].endianSwap = GX2_ENDIAN_SWAP_DEFAULT;
    }

    const uint32_t fetchSize = GX2CalcFetchShaderSizeEx(attribCount, GX2_FETCH_SHADER_TESSELLATION_NONE,
                                                        GX2_TESSELLATION_MODE_DISCRETE);
    void *fetchMemory = allocate(fetchSize, GX2_SHADER_PROGRAM_ALIGNMENT);
    if (!fetchMemory) {
        Log::logError(std::string("[GX2] Out of memory for '") + name + "' fetch shader");
        FreeShaders(vs, ps);
        return false;
    }
    GX2InitFetchShaderEx(&program.fetchShader, static_cast<uint8_t *>(fetchMemory), attribCount, streams,
                         GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, fetchMemory, fetchSize);

    program.vertexShader = vs;
    program.pixelShader = ps;
    program.fetchShaderMemory = fetchMemory;
    program.valid = true;
    return true;
}

void destroyProgram(Program &program) {
    if (program.vertexShader || program.pixelShader) FreeShaders(program.vertexShader, program.pixelShader);
    if (program.fetchShaderMemory) release(program.fetchShaderMemory);
    program = Program{};
}

bool buildSpriteProgram() {
    static const AttribDesc attribs[] = {
        {"a_pos", 0, GX2_ATTRIB_FORMAT_FLOAT_32_32},
        {"a_uv", 8, GX2_ATTRIB_FORMAT_FLOAT_32_32},
    };
    SpriteProgram &p = g.sprite;
    if (!buildProgram(p, "sprite", GX2Shaders::kSpriteVert, GX2Shaders::kSpriteFrag, attribs, 2)) return false;

    bool ok = true;
    ok &= findVertexUniform(p.vertexShader, "u_model", p.uModel);
    ok &= findVertexUniform(p.vertexShader, "u_projection", p.uProjection);
    ok &= findPixelUniform(p.pixelShader, "u_params0", p.uParams0);
    ok &= findPixelUniform(p.pixelShader, "u_params1", p.uParams1);
    ok &= findPixelUniform(p.pixelShader, "u_params2", p.uParams2);
    ok &= findSampler(p.pixelShader, "u_tex", p.samplerLocation);
    return ok;
}

bool buildSolidProgram() {
    static const AttribDesc attribs[] = {
        {"a_pos", 0, GX2_ATTRIB_FORMAT_FLOAT_32_32},
        {"a_color", 8, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32},
    };
    SolidProgram &p = g.solid;
    if (!buildProgram(p, "solid", GX2Shaders::kSolidVert, GX2Shaders::kSolidFrag, attribs, 2)) return false;
    return findVertexUniform(p.vertexShader, "u_projection", p.uProjection);
}

bool buildTextProgram() {
    static const AttribDesc attribs[] = {
        {"a_pos", 0, GX2_ATTRIB_FORMAT_FLOAT_32_32},
        {"a_uv", 8, GX2_ATTRIB_FORMAT_FLOAT_32_32},
    };
    TextProgram &p = g.text;
    if (!buildProgram(p, "text", GX2Shaders::kTextVert, GX2Shaders::kTextFrag, attribs, 2)) return false;

    bool ok = true;
    ok &= findVertexUniform(p.vertexShader, "u_projection", p.uProjection);
    ok &= findPixelUniform(p.pixelShader, "u_color", p.uColor);
    ok &= findSampler(p.pixelShader, "u_tex", p.samplerLocation);
    return ok;
}

void applyRasterDefaults() {
    GX2SetDepthStencilControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS,
                              FALSE, FALSE,
                              GX2_COMPARE_FUNC_ALWAYS, GX2_STENCIL_FUNCTION_KEEP, GX2_STENCIL_FUNCTION_KEEP, GX2_STENCIL_FUNCTION_KEEP,
                              GX2_COMPARE_FUNC_ALWAYS, GX2_STENCIL_FUNCTION_KEEP, GX2_STENCIL_FUNCTION_KEEP, GX2_STENCIL_FUNCTION_KEEP);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
}

uint32_t componentMap(GX2SurfaceFormat format) {
    if (format == GX2_SURFACE_FORMAT_UNORM_R8)
        return GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
    return GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
}

uint32_t bytesPerPixel(GX2SurfaceFormat format) {
    return format == GX2_SURFACE_FORMAT_UNORM_R8 ? 1 : 4;
}

void initSurface(GX2Surface &surface, int width, int height, GX2SurfaceFormat format, GX2SurfaceUse use, GX2TileMode tileMode) {
    std::memset(&surface, 0, sizeof(surface));
    surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    surface.width = static_cast<uint32_t>(width);
    surface.height = static_cast<uint32_t>(height);
    surface.depth = 1;
    surface.mipLevels = 1;
    surface.format = format;
    surface.aa = GX2_AA_MODE1X;
    surface.use = use;
    surface.tileMode = tileMode;
    GX2CalcSurfaceSizeAndAlignment(&surface);
}

void initTextureView(GX2Texture &texture, GX2SurfaceFormat format) {
    texture.viewFirstMip = 0;
    texture.viewNumMips = 1;
    texture.viewFirstSlice = 0;
    texture.viewNumSlices = 1;
    texture.compMap = componentMap(format);
    GX2InitTextureRegs(&texture);
}

} // namespace

void *allocate(size_t size, size_t alignment) {
    return MEMAllocFromDefaultHeapEx(static_cast<uint32_t>(size), static_cast<uint32_t>(alignment));
}

void release(void *memory) {
    if (memory) MEMFreeToDefaultHeap(memory);
}

void deferFree(void *memory) {
    if (!memory) return;
    if (!g.frameStarted) release(memory);
    else g.deferred.push_back(memory);
}

const char *init(int /*sceneWidth*/, int /*sceneHeight*/) {
    if (g.initialised) return nullptr;

    InitGLSLCompiler();
    g.compilerReady = true;

    if (!buildSpriteProgram() || !buildSolidProgram() || !buildTextProgram()) {
        shutdown();
        return "Failed to compile shaders (see log)";
    }

    GX2InitSampler(&g.nearest, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
    GX2InitSampler(&g.linear, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    g.stream = static_cast<uint8_t *>(allocate(kStreamSize, GX2_VERTEX_BUFFER_ALIGNMENT));
    if (!g.stream) {
        shutdown();
        return "Out of memory for the vertex stream";
    }

    g.unitQuad = static_cast<float *>(allocate(64, GX2_VERTEX_BUFFER_ALIGNMENT));
    if (!g.unitQuad) {
        shutdown();
        return "Out of memory for the unit quad";
    }
    static const float quad[16] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f};
    std::memcpy(g.unitQuad, quad, sizeof(quad));
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, g.unitQuad, 64);

    g.context = static_cast<GX2ContextState *>(allocate(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT));
    if (!g.context) {
        shutdown();
        return "Out of memory for the render context";
    }
    GX2SetupContextStateEx(g.context, TRUE);
    GX2SetContextState(g.context);
    applyRasterDefaults();

    g.initialised = true;
    return nullptr;
}

void shutdown() {
    if ((g.initialised || g.context) && ProcUIInForeground()) GX2DrawDone();

    flushDeferred();

    destroyProgram(g.sprite);
    destroyProgram(g.solid);
    destroyProgram(g.text);

    if (g.compilerReady) {
        DestroyGLSLCompiler();
        g.compilerReady = false;
    }

    release(g.stream);
    release(g.unitQuad);
    release(g.context);

    g = State{};
}

bool isInitialised() { return g.initialised; }

const SpriteProgram &spriteProgram() { return g.sprite; }
const SolidProgram &solidProgram() { return g.solid; }
const TextProgram &textProgram() { return g.text; }

void beginFrame() {
    if (g.frameStarted) return;

    WHBGfxBeginRender();
    g.streamHead = 0;
    GX2SetContextState(g.context);
    invalidateStateCache();
    g.frameStarted = true;
}

bool frameStarted() { return g.frameStarted; }

namespace {

void blitRegion(RenderTarget &source, GX2ColorBuffer *target, float v0, float v1) {
    const int tw = static_cast<int>(target->surface.width);
    const int th = static_cast<int>(target->surface.height);
    const float regionHeight = static_cast<float>(source.height) * (v1 - v0);
    const float sourceAspect = static_cast<float>(source.width) / regionHeight;

    invalidateStateCache();
    setViewport(tw, th);
    applyRasterDefaults();
    setBlend(Blend::None);
    bindProgram(g.sprite);

    float destW = static_cast<float>(tw);
    float destH = destW / sourceAspect;
    if (destH > static_cast<float>(th)) {
        destH = static_cast<float>(th);
        destW = destH * sourceAspect;
    }
    const float destX = (static_cast<float>(tw) - destW) * 0.5f;
    const float destY = (static_cast<float>(th) - destH) * 0.5f;

    float projection[16];
    buildOrtho(projection, 0.0f, static_cast<float>(tw), static_cast<float>(th), 0.0f);

    float model[16] = {};
    model[0] = destW;
    model[5] = destH;
    model[10] = 1.0f;
    model[12] = destX;
    model[13] = destY;
    model[15] = 1.0f;

    SpriteParams params;
    params.texWidth = static_cast<float>(source.width);
    params.texHeight = regionHeight;
    setSpriteUniforms(model, projection, params);

    const bool scaled = static_cast<float>(tw) != source.width || static_cast<float>(th) != regionHeight;
    bindTexture(source.texture, g.sprite.samplerLocation, scaled ? Filter::Linear : Filter::Nearest);

    float *verts = static_cast<float *>(streamAllocate(16 * sizeof(float)));
    if (verts) {
        const float quad[16] = {
            0.0f, 0.0f, 0.0f, v0,
            1.0f, 0.0f, 1.0f, v0,
            1.0f, 1.0f, 1.0f, v1,
            0.0f, 1.0f, 0.0f, v1};
        std::memcpy(verts, quad, sizeof(quad));
        drawStream(verts, sizeof(quad), 4 * sizeof(float), GX2_PRIMITIVE_MODE_QUADS, 4);
    }
}

} // namespace

void present(RenderTarget &source) {
    if (!g.frameStarted) beginFrame();

    makeReadable(source);

    WHBGfxBeginRenderTV();
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    blitRegion(source, WHBGfxGetTVColourBuffer(), 0.0f, 1.0f);
    WHBGfxFinishRenderTV();

    WHBGfxBeginRenderDRC();
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    blitRegion(source, WHBGfxGetDRCColourBuffer(), 0.0f, 1.0f);
    WHBGfxFinishRenderDRC();

    WHBGfxFinishRender();

    flushDeferred();
    g.frameStarted = false;
    GX2SetContextState(g.context);
    invalidateStateCache();
}

void presentSplit(RenderTarget &source) {
    if (!g.frameStarted) beginFrame();

    makeReadable(source);

    WHBGfxBeginRenderTV();
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    blitRegion(source, WHBGfxGetTVColourBuffer(), 0.0f, 0.5f);
    WHBGfxFinishRenderTV();

    WHBGfxBeginRenderDRC();
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    blitRegion(source, WHBGfxGetDRCColourBuffer(), 0.5f, 1.0f);
    WHBGfxFinishRenderDRC();

    WHBGfxFinishRender();

    flushDeferred();
    g.frameStarted = false;
    GX2SetContextState(g.context);
    invalidateStateCache();
}

bool createTexture(Texture &out, int width, int height, GX2SurfaceFormat format, const void *pixels, int pitchBytes) {
    if (width <= 0 || height <= 0) return false;

    Texture texture;
    initSurface(texture.gx2.surface, width, height, format, GX2_SURFACE_USE_TEXTURE, GX2_TILE_MODE_LINEAR_ALIGNED);

    GX2Surface &surface = texture.gx2.surface;
    void *memory = allocate(surface.imageSize, surface.alignment);
    if (!memory) {
        Log::logError("[GX2] Out of memory for a " + std::to_string(width) + "x" + std::to_string(height) + " texture");
        return false;
    }
    std::memset(memory, 0, surface.imageSize);

    if (pixels) {
        const uint32_t bpp = bytesPerPixel(format);
        const size_t srcPitch = pitchBytes > 0 ? static_cast<size_t>(pitchBytes) : static_cast<size_t>(width) * bpp;
        const size_t dstPitch = static_cast<size_t>(surface.pitch) * bpp;
        const size_t rowBytes = static_cast<size_t>(width) * bpp;
        const uint8_t *src = static_cast<const uint8_t *>(pixels);
        uint8_t *dst = static_cast<uint8_t *>(memory);
        for (int y = 0; y < height; ++y)
            std::memcpy(dst + y * dstPitch, src + y * srcPitch, rowBytes);
    }

    surface.image = memory;
    initTextureView(texture.gx2, format);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, memory, surface.imageSize);

    texture.valid = true;
    out = texture;
    return true;
}

bool replaceTexture(Texture &texture, int width, int height, GX2SurfaceFormat format, const void *pixels, int pitchBytes) {
    Texture fresh;
    if (!createTexture(fresh, width, height, format, pixels, pitchBytes)) return false;
    if (texture.valid) deferFree(texture.gx2.surface.image);
    texture = fresh;
    return true;
}

void destroyTexture(Texture &texture) {
    if (texture.valid) deferFree(texture.gx2.surface.image);
    texture = Texture{};
}

bool createRenderTarget(RenderTarget &out, int width, int height) {
    if (width <= 0 || height <= 0) return false;

    RenderTarget target;
    GX2ColorBuffer &cb = target.colorBuffer;
    std::memset(&cb, 0, sizeof(cb));
    initSurface(cb.surface, width, height, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
                static_cast<GX2SurfaceUse>(GX2_SURFACE_USE_TEXTURE | GX2_SURFACE_USE_COLOR_BUFFER),
                GX2_TILE_MODE_DEFAULT);
    cb.viewNumSlices = 1;
    GX2InitColorBufferRegs(&cb);

    void *memory = allocate(cb.surface.imageSize, cb.surface.alignment);
    if (!memory) {
        Log::logError("[GX2] Out of memory for a " + std::to_string(width) + "x" + std::to_string(height) + " render target");
        return false;
    }
    cb.surface.image = memory;
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, memory, cb.surface.imageSize);

    std::memset(&target.texture.gx2, 0, sizeof(GX2Texture));
    target.texture.gx2.surface = cb.surface;
    initTextureView(target.texture.gx2, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8);
    target.texture.valid = true;

    target.width = width;
    target.height = height;
    target.valid = true;
    out = target;
    return true;
}

void makeReadable(RenderTarget &target) {
    GX2Invalidate(invalidateMode(GX2_INVALIDATE_MODE_COLOR_BUFFER | GX2_INVALIDATE_MODE_TEXTURE),
                  target.colorBuffer.surface.image, target.colorBuffer.surface.imageSize);
}

void waitIdle() {
    if (!ProcUIInForeground()) return;
    GX2Flush();
    GX2DrawDone();
}

void destroyRenderTarget(RenderTarget &target) {
    if (target.valid) deferFree(target.colorBuffer.surface.image);
    if (g.colorBuffer == &target.colorBuffer) g.colorBuffer = nullptr;
    target = RenderTarget{};
}

void setViewport(int width, int height) {
    g.viewportWidth = width;
    g.viewportHeight = height;
    GX2SetViewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
    GX2SetScissor(0, 0, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

void getViewportSize(int &width, int &height) {
    width = g.viewportWidth;
    height = g.viewportHeight;
}

void setRenderTarget(GX2ColorBuffer *colorBuffer, int width, int height) {
    g.colorBuffer = colorBuffer;
    GX2SetColorBuffer(colorBuffer, GX2_RENDER_TARGET_0);
    setViewport(width, height);
}

void clear(float r, float gr, float b, float a) {
    if (!g.colorBuffer) return;
    GX2ClearColor(g.colorBuffer, r, gr, b, a);
    GX2SetContextState(g.context);
    invalidateStateCache();
}

void setBlend(Blend mode) {
    if (mode == g.blend) return;
    g.blend = mode;

    switch (mode) {
    case Blend::None:
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x00, FALSE, TRUE);
        break;
    case Blend::Alpha:
        GX2SetBlendControl(GX2_RENDER_TARGET_0,
                           GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD,
                           TRUE,
                           GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD);
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
        break;
    case Blend::Premultiplied:
        GX2SetBlendControl(GX2_RENDER_TARGET_0,
                           GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD,
                           TRUE,
                           GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD);
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
        break;
    case Blend::Unknown:
        break;
    }
}

void bindProgram(const Program &program) {
    if (g.boundProgram == &program) return;
    g.boundProgram = &program;

    GX2SetFetchShader(&program.fetchShader);
    GX2SetVertexShader(program.vertexShader);
    GX2SetPixelShader(program.pixelShader);
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);
}

void bindTexture(const Texture &texture, uint32_t samplerLocation, Filter filter) {
    GX2SetPixelTexture(&texture.gx2, samplerLocation);
    GX2SetPixelSampler(filter == Filter::Nearest ? &g.nearest : &g.linear, samplerLocation);
}

void setVertexUniform(uint32_t offsetDwords, const float *data, uint32_t countDwords) {
    GX2SetVertexUniformReg(offsetDwords, countDwords, data);
}

void setPixelUniform(uint32_t offsetDwords, const float *data, uint32_t countDwords) {
    GX2SetPixelUniformReg(offsetDwords, countDwords, data);
}

void setSpriteUniforms(const float model[16], const float projection[16], const SpriteParams &params) {
    const SpriteProgram &p = g.sprite;
    setVertexUniform(p.uModel, model, 16);
    setVertexUniform(p.uProjection, projection, 16);
    const float params0[4] = {params.opacity, params.brightness, params.color, params.fisheye};
    const float params1[4] = {params.whirl, params.pixelate, params.mosaic, params.texWidth};
    const float params2[4] = {params.texHeight, 0.0f, 0.0f, 0.0f};
    setPixelUniform(p.uParams0, params0, 4);
    setPixelUniform(p.uParams1, params1, 4);
    setPixelUniform(p.uParams2, params2, 4);
}

void *streamAllocate(size_t bytes) {
    const size_t aligned = alignUp(bytes, GX2_VERTEX_BUFFER_ALIGNMENT);
    if (aligned > kStreamSize) {
        Log::logError("[GX2] Vertex data too large for the stream (" + std::to_string(bytes) + " bytes)");
        return nullptr;
    }

    if (g.streamHead + aligned > kStreamSize) {
        GX2Flush();
        GX2DrawDone();
        flushDeferred();
        g.streamHead = 0;
    }

    void *memory = g.stream + g.streamHead;
    g.streamHead += aligned;
    return memory;
}

void drawStream(const void *vertices, size_t bytes, uint32_t stride, GX2PrimitiveMode mode, uint32_t vertexCount) {
    const uint32_t aligned = static_cast<uint32_t>(alignUp(bytes, GX2_VERTEX_BUFFER_ALIGNMENT));
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, const_cast<void *>(vertices), aligned);
    GX2SetAttribBuffer(0, aligned, stride, vertices);
    GX2DrawEx(mode, vertexCount, 0, 1);
}

void drawUnitQuad() {
    GX2SetAttribBuffer(0, 64, 16, g.unitQuad);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}

void buildOrtho(float out[16], float l, float r, float b, float t) {
    for (int i = 0; i < 16; ++i)
        out[i] = 0.0f;
    out[0] = 2.0f / (r - l);
    out[5] = 2.0f / (t - b);
    out[10] = -1.0f;
    out[12] = -(r + l) / (r - l);
    out[13] = -(t + b) / (t - b);
    out[15] = 1.0f;
}

} // namespace GX2Gfx
