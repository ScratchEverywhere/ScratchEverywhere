#include "image_gx2.hpp"

#include <cmath>
#include <cstring>

static constexpr unsigned kMaxTextureSize = 8192;

static void buildModelMatrix(float out[16],
                             float tx, float ty,
                             float sx, float sy,
                             float rotRad,
                             float pivotX, float pivotY) {
    const float c = std::cos(rotRad);
    const float s = std::sin(rotRad);

    out[0] = c * sx;
    out[1] = s * sx;
    out[2] = 0.0f;
    out[3] = 0.0f;

    out[4] = -s * sy;
    out[5] = c * sy;
    out[6] = 0.0f;
    out[7] = 0.0f;

    out[8] = 0.0f;
    out[9] = 0.0f;
    out[10] = 1.0f;
    out[11] = 0.0f;

    out[12] = tx + pivotX - (c * sx * pivotX - s * sy * pivotY);
    out[13] = ty + pivotY - (s * sx * pivotX + c * sy * pivotY);
    out[14] = 0.0f;
    out[15] = 1.0f;
}

void Image_GX2::render(ImageRenderParams &params) {
    if (!texture.valid) return;

    const float x = params.x;
    const float y = params.y;
    const bool centered = params.centered;
    const bool flip = params.flip;

    const float scaleX = flip ? -std::abs(params.scale) : std::abs(params.scale);
    const float scaleY = params.scale;

    int renderWidth, renderHeight;
    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;

    if (params.subrect) {
        renderWidth = params.subrect->w;
        renderHeight = params.subrect->h;
        const float tw = static_cast<float>(imgData.width);
        const float th = static_cast<float>(imgData.height);
        u0 = params.subrect->x / tw;
        v0 = params.subrect->y / th;
        u1 = (params.subrect->x + params.subrect->w) / tw;
        v1 = (params.subrect->y + params.subrect->h) / th;
    } else {
        renderWidth = getWidth();
        renderHeight = getHeight();
    }

    const float pivotX = centered ? 0.5f : 0.0f;
    const float pivotY = centered ? 0.5f : 0.0f;

    float model[16];
    buildModelMatrix(model, x, y,
                     static_cast<float>(renderWidth) * scaleX,
                     static_cast<float>(renderHeight) * scaleY,
                     params.rotation, pivotX, pivotY);

    int viewportWidth, viewportHeight;
    GX2Gfx::getViewportSize(viewportWidth, viewportHeight);
    float proj[16];
    GX2Gfx::buildOrtho(proj, 0.0f, static_cast<float>(viewportWidth), static_cast<float>(viewportHeight), 0.0f);

    GX2Gfx::SpriteParams sprite;
    sprite.opacity = params.opacity;
    sprite.brightness = static_cast<float>(params.brightness);
    sprite.color = params.colorEffect;
    sprite.fisheye = params.fisheyeEffect;
    sprite.whirl = params.whirlEffect;
    sprite.pixelate = params.pixelateEffect;
    sprite.mosaic = params.mosaicEffect;
    sprite.texWidth = static_cast<float>(getWidth());
    sprite.texHeight = static_cast<float>(getHeight());

    const GX2Gfx::SpriteProgram &program = GX2Gfx::spriteProgram();
    GX2Gfx::bindProgram(program);
    GX2Gfx::setBlend(params.blend ? GX2Gfx::Blend::Alpha : GX2Gfx::Blend::None);
    GX2Gfx::setSpriteUniforms(model, proj, sprite);
    GX2Gfx::bindTexture(texture, program.samplerLocation, GX2Gfx::Filter::Nearest);

    if (params.subrect) {
        float *verts = static_cast<float *>(GX2Gfx::streamAllocate(16 * sizeof(float)));
        if (verts) {
            const float quad[16] = {
                0.0f, 0.0f, u0, v0,
                1.0f, 0.0f, u1, v0,
                1.0f, 1.0f, u1, v1,
                0.0f, 1.0f, u0, v1};
            std::memcpy(verts, quad, sizeof(quad));
            GX2Gfx::drawStream(verts, sizeof(quad), 4 * sizeof(float), GX2_PRIMITIVE_MODE_QUADS, 4);
        }
    } else {
        GX2Gfx::drawUnitQuad();
    }

    freeTimer = maxFreeTimer;
}

void Image_GX2::renderNineslice(double xPos, double yPos,
                                double width, double height,
                                double padding, bool centered) {
    if (!texture.valid) return;

    float destX = static_cast<float>(xPos);
    float destY = static_cast<float>(yPos);
    if (centered) {
        destX -= static_cast<float>(width) / 2.0f;
        destY -= static_cast<float>(height) / 2.0f;
    }

    const float imgW = static_cast<float>(getWidth());
    const float imgH = static_cast<float>(getHeight());
    const float p = static_cast<float>(padding);
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);

    int viewportWidth, viewportHeight;
    GX2Gfx::getViewportSize(viewportWidth, viewportHeight);
    float proj[16];
    GX2Gfx::buildOrtho(proj, 0.0f, static_cast<float>(viewportWidth), static_cast<float>(viewportHeight), 0.0f);

    float model[16] = {};
    model[0] = model[5] = model[10] = model[15] = 1.0f;

    GX2Gfx::SpriteParams sprite;
    sprite.texWidth = imgW;
    sprite.texHeight = imgH;

    const GX2Gfx::SpriteProgram &program = GX2Gfx::spriteProgram();
    GX2Gfx::bindProgram(program);
    GX2Gfx::setBlend(GX2Gfx::Blend::Alpha);
    GX2Gfx::setSpriteUniforms(model, proj, sprite);
    GX2Gfx::bindTexture(texture, program.samplerLocation, GX2Gfx::Filter::Nearest);

    constexpr int kSlices = 9;
    float *verts = static_cast<float *>(GX2Gfx::streamAllocate(kSlices * 16 * sizeof(float)));
    if (!verts) return;

    int slice = 0;
    auto addSlice = [&](float sx, float sy, float sw, float sh,
                        float dx, float dy, float dw, float dh) {
        const float u0 = sx / imgW, v0 = sy / imgH;
        const float u1 = (sx + sw) / imgW, v1 = (sy + sh) / imgH;
        const float quad[16] = {
            dx, dy, u0, v0,
            dx + dw, dy, u1, v0,
            dx + dw, dy + dh, u1, v1,
            dx, dy + dh, u0, v1};
        std::memcpy(verts + slice * 16, quad, sizeof(quad));
        ++slice;
    };

    addSlice(0, 0, p, p, destX, destY, p, p);
    addSlice(p, 0, imgW - p * 2, p, destX + p, destY, w - p * 2, p);
    addSlice(imgW - p, 0, p, p, destX + w - p, destY, p, p);

    addSlice(0, p, p, imgH - p * 2, destX, destY + p, p, h - p * 2);
    addSlice(p, p, imgW - p * 2, imgH - p * 2, destX + p, destY + p, w - p * 2, h - p * 2);
    addSlice(imgW - p, p, p, imgH - p * 2, destX + w - p, destY + p, p, h - p * 2);

    addSlice(0, imgH - p, p, p, destX, destY + h - p, p, p);
    addSlice(p, imgH - p, imgW - p * 2, p, destX + p, destY + h - p, w - p * 2, p);
    addSlice(imgW - p, imgH - p, p, p, destX + w - p, destY + h - p, p, p);

    GX2Gfx::drawStream(verts, kSlices * 16 * sizeof(float), 4 * sizeof(float), GX2_PRIMITIVE_MODE_QUADS, kSlices * 4);

    freeTimer = maxFreeTimer;
}

void *Image_GX2::getNativeTexture() {
    return &texture;
}

void Image_GX2::setInitialTexture() {
    if (!GX2Gfx::createTexture(texture, imgData.width, imgData.height, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
                               imgData.pixels, imgData.pitch)) {
        error = "Failed to create GX2 texture";
    }
}

nonstd::expected<void, std::string> Image_GX2::refreshTexture() {
    if (!GX2Gfx::replaceTexture(texture, imgData.width, imgData.height, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
                                imgData.pixels, imgData.pitch)) {
        return nonstd::make_unexpected(std::string("Failed to refresh GX2 texture"));
    }
    return {};
}

Image_GX2::Image_GX2(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    maxTextureSize = {kMaxTextureSize, kMaxTextureSize};

    const auto result = init(filePath, fromScratchProject, bitmapHalfQuality, scale);
    if (!result.has_value()) {
        error = result.error();
        return;
    }
    setInitialTexture();
}

Image_GX2::Image_GX2(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
    maxTextureSize = {kMaxTextureSize, kMaxTextureSize};

    const auto result = init(filePath, zip, bitmapHalfQuality, scale);
    if (!result.has_value()) {
        error = result.error();
        return;
    }
    setInitialTexture();
}

Image_GX2::~Image_GX2() {
    GX2Gfx::destroyTexture(texture);
}
