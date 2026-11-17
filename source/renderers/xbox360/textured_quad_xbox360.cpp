#include "costume_shaders_xbox360.hpp"
#include "texture_shaders_xbox360.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <log.hpp>
#include <stdexcept>
#include <textured_quad_xbox360.hpp>

static void checked(NTSTATUS status, const char *operation) {
    if (!NT_SUCCESS(status)) throw std::runtime_error(std::string(operation) + ": " + std::to_string(uint32_t(status)));
}
TexturedQuadXbox360::TexturedQuadXbox360(const ImageData &image, int, int, bool linear) {
    if (!image.pixels || image.width <= 0 || image.height <= 0 || image.width > XSDK_TEXTURE_MAX_SIZE || image.height > XSDK_TEXTURE_MAX_SIZE || image.format != IMAGE_FORMAT_RGBA32 || int64_t(image.pitch) < int64_t(image.width) * 4)
        throw std::invalid_argument("Invalid RGBA32 texture data/dimensions/pitch");
    try {
        checked(xsdk_texture_create(&texture_, XSDK_FORMAT_R8G8B8A8, image.width, image.height, 1, false), "texture_create");
        // This SDK format explicitly consumes R,G,B,A bytes, not host-native packed words.
        xsdk_texture_upload(&texture_, 0, image.pixels, image.pitch);
        checked(xsdk_shader_load(&vertex_, texture_vertex_shader, sizeof texture_vertex_shader), "vertex shader");
        checked(xsdk_shader_load(&pixel_, costume_frag.container, costume_frag.container_bytes), "pixel shader");
        XsdkVertexElement layout[2]{};
        layout[0].usage = XSDK_USAGE_POSITION;
        layout[0].format = XSDK_VERTEX_FLOAT4;
        layout[1].usage = XSDK_USAGE_TEXCOORD;
        layout[1].format = XSDK_VERTEX_FLOAT2;
        layout[1].offset_bytes = 16;
        const uint32_t stride = 24;
        checked(xsdk_shader_set_vertex_layout(&vertex_, layout, 2, &stride), "vertex layout");
        XsdkSampler sampler{};
        sampler.mag_filter = sampler.min_filter = linear ? XSDK_FILTER_LINEAR : XSDK_FILTER_POINT;
        sampler.mip_filter = XSDK_FILTER_POINT;
        sampler.address_u = sampler.address_v = XSDK_ADDRESS_CLAMP;
        xsdk_texture_sampler_state(&sampler_, &sampler);
        XsdkBlend blend{};
        blend.color_source = XSDK_BLEND_SRC_ALPHA;
        blend.color_dest = XSDK_BLEND_ONE_MINUS_SRC_ALPHA;
        blend.alpha_source = XSDK_BLEND_ONE;
        blend.alpha_dest = XSDK_BLEND_ONE_MINUS_SRC_ALPHA;
        blend.color_op = blend.alpha_op = XSDK_BLEND_OP_ADD;
        blend.write_mask = XSDK_WRITE_ALL;
        xsdk_gpu_blend_state(&blend_, &blend);
        XsdkDepthStencil depth{};
        xsdk_gpu_depth_stencil_state(&depth_, &depth);
        XsdkRaster raster{};
        raster.cull = XSDK_CULL_NONE;
        xsdk_gpu_raster_state(&raster_, &raster);
        constexpr uint32_t vertexBytes = 6 * 6 * sizeof(float);
        vertices_ = xsdk_gpu_alloc(vertexBytes * vertexSlots_);
        if (!vertices_) throw std::bad_alloc();
    } catch (...) {
        release();
        throw;
    }
}
void TexturedQuadXbox360::render() {
    xsdk_gpu_set_shaders(&vertex_, &pixel_);
    xsdk_built_shader_set_literals(&costume_frag);
    const float effects[4] = {opacity_, brightness_, 0, 0};
    xsdk_gpu_set_pixel_constants(COSTUME_FRAG_PUSH, effects, COSTUME_FRAG_PUSH_REGISTERS);
    xsdk_gpu_set_blend(&blend_);
    xsdk_gpu_set_depth_stencil(&depth_);
    xsdk_gpu_set_raster(&raster_);
    xsdk_gpu_set_texture(0, &texture_, &sampler_);
    xsdk_gpu_set_vertices(0, xsdk_gpu_address(vertices_ + vertexSlot_ * 36), 144);
    xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_LIST, 6);
    reuseFences_[vertexSlot_] = xsdk_gpu_fence();
    usedSlots_[vertexSlot_] = true;
}
void TexturedQuadXbox360::release() {
    xsdk_gpu_free(vertices_);
    vertices_ = nullptr;
    xsdk_shader_free(&pixel_);
    xsdk_shader_free(&vertex_);
    xsdk_texture_free(&texture_);
}
TexturedQuadXbox360::~TexturedQuadXbox360() {
    if (!NT_SUCCESS(xsdk_gpu_wait(xsdk_gpu_fence(), 5000))) {
        Log::logError("Texture GPU wait failed; retaining resources until title termination");
        return;
    }
    release();
}

void TexturedQuadXbox360::render(const ImageRenderParams &p, int iw, int ih, int sw, int sh) {
    if (p.subrect) throw std::logic_error("Xbox 360 subrect transforms are not implemented");
    renderRegion(p, iw * p.scale, ih * p.scale, sw, sh, 0, 0, 1, 1);
}
void TexturedQuadXbox360::renderRegion(const ImageRenderParams &p, float w, float h, int sw, int sh, float u0, float v0, float u1, float v1) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.scale) || !std::isfinite(p.rotation))
        throw std::invalid_argument("Non-finite image transform");
    if (p.scale == 0 || p.opacity <= 0) return;
    if (!p.blend || p.subrect || p.colorEffect || p.fisheyeEffect || p.whirlEffect || p.pixelateEffect || p.mosaicEffect)
        throw std::logic_error("Xbox 360 image distortion/color effects or subrect rendering are not implemented");
    if (!std::isfinite(p.opacity)) throw std::invalid_argument("Non-finite costume opacity");
    opacity_ = std::clamp(p.opacity, 0.0f, 1.0f);
    brightness_ = std::clamp(p.brightness, -100, 100) / 100.0f;
    // A draw owns its slot until its fence passes. Never overwrite in-flight vertices.
    if (usedSlots_[vertexSlot_] && !xsdk_gpu_fence_done(reuseFences_[vertexSlot_]))
        checked(xsdk_gpu_wait(reuseFences_[vertexSlot_], 5000), "image vertex ring reuse fence");
    volatile uint32_t *const slot = vertices_ + vertexSlot_ * 36;
    const float cx = p.centered ? p.x : p.x + w / 2, cy = p.centered ? p.y : p.y + h / 2;
    const float cosine = std::cos(p.rotation), sine = std::sin(p.rotation);
    const int corners[6][2] = {{0, 0}, {1, 0}, {0, 1}, {0, 1}, {1, 0}, {1, 1}};
    float data[6][6];
    for (int i = 0; i < 6; ++i) {
        float x = (corners[i][0] - 0.5f) * w, y = (corners[i][1] - 0.5f) * h;
        float px = cx + x * cosine - y * sine, py = cy + x * sine + y * cosine;
        data[i][0] = 2 * (px - 0.5f) / sw - 1;
        data[i][1] = 1 - 2 * (py - 0.5f) / sh;
        data[i][2] = 0;
        data[i][3] = 1;
        data[i][4] = u0 + (u1 - u0) * (p.flip ? 1 - corners[i][0] : corners[i][0]);
        data[i][5] = v0 + (v1 - v0) * corners[i][1];
    }
    for (unsigned i = 0; i < sizeof data / 4; ++i) {
        uint32_t word;
        memcpy(&word, reinterpret_cast<const uint8_t *>(data) + i * 4, 4);
        slot[i] = word;
    }
    __asm__ volatile("sync" ::: "memory");
    xsdk_gpu_invalidate(xsdk_gpu_address(slot), sizeof data);
    render();
    vertexSlot_ = (vertexSlot_ + 1) % vertexSlots_;
}
