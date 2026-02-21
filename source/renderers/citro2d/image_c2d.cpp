#include "image_c2d.hpp"
#include <algorithm>
#include <math.hpp>
#include <os.hpp>
#include <string>
#include <unzip.hpp>
#include <vector>

const uint32_t Image_C2D::rgbaToAgbr(uint32_t px) {
    uint8_t r = (px & 0xff000000) >> 24;
    uint8_t g = (px & 0x00ff0000) >> 16;
    uint8_t b = (px & 0x0000ff00) >> 8;
    uint8_t a = px & 0x000000ff;
    return (a << 24) | (b << 16) | (g << 8) | r;
}

void Image_C2D::render(ImageRenderParams &params) {

    int x = params.x;
    int y = params.y;
    float scaleX = params.scale;
    const float scaleY = params.scale;
    const int &brightness = params.brightness;
    const double rotation = params.rotation;
    const float &opacity = params.opacity;
    const bool &centered = params.centered;
    bool &flip = params.flip;

    const int drawW = params.subrect ? params.subrect->w : getWidth();
    const int drawH = params.subrect ? params.subrect->h : getHeight();

    if (!centered) {
        x += drawW / 2;
        y += drawH / 2;
    }

    if (flip) {
        x -= drawW * std::abs(scaleX);
        scaleX = -std::abs(scaleX);
    }

    Tex3DS_SubTexture subtex;

    if (params.subrect != nullptr) {
        const float texWidth = static_cast<float>(texture.tex->width);
        const float texHeight = static_cast<float>(texture.tex->height);

        const float x = static_cast<float>(params.subrect->x);
        const float y = static_cast<float>(params.subrect->y);
        const float w = static_cast<float>(params.subrect->w);
        const float h = static_cast<float>(params.subrect->h);

        float top = (texHeight - (y + h)) / texHeight;
        float bottom = (texHeight - y) / texHeight;
        if (top < bottom) std::swap(top, bottom);

        subtex = {
            .width = static_cast<uint16_t>(w),
            .height = static_cast<uint16_t>(h),
            .left = x / texWidth,
            .top = top,
            .right = (x + w) / texWidth,
            .bottom = bottom};
    } else {
        subtex = *texture.subtex;
    }

    C2D_ImageTint tinty;

    if (brightness != 0.0f || opacity != 1.0f) {
        float brightnessEffect = brightness * 0.01f;
        float alpha = 255.0f * (opacity);
        if (brightnessEffect > 0)
            C2D_PlainImageTint(&tinty, C2D_Color32(255, 255, 255, alpha), brightnessEffect);
        else
            C2D_PlainImageTint(&tinty, C2D_Color32(0, 0, 0, alpha), brightnessEffect);
    } else C2D_AlphaImageTint(&tinty, 1.0f);

    C2D_DrawImageAtRotated({texture.tex, &subtex}, x, y, 1, rotation, &tinty, scaleX, scaleY);
    freeTimer = maxFreeTimer;
}

void Image_C2D::renderSubrect(C2D_Image img, uint16_t srcX, uint16_t srcY, uint16_t srcW, uint16_t srcH, float destX, float destY, float destW, float destH, C2D_ImageTint *tint) {
    uint16_t texW = img.subtex->width - 1;
    texW |= texW >> 1;
    texW |= texW >> 2;
    texW |= texW >> 4;
    texW |= texW >> 8;
    texW++;

    uint16_t texH = img.subtex->height - 1;
    texH |= texH >> 1;
    texH |= texH >> 2;
    texH |= texH >> 4;
    texH |= texH >> 8;
    texH++;

    const Tex3DS_SubTexture subtex = {
        srcW,
        srcH,
        static_cast<float>(srcX) / texW,
        1 - static_cast<float>(srcY) / texH,
        static_cast<float>(srcX + srcW) / texW,
        1 - static_cast<float>(srcY + srcH) / texH};

    C2D_DrawImageAt({img.tex, &subtex}, destX, destY, 1, tint, 1.0f / srcW * destW, 1.0f / srcH * destH);
}

void Image_C2D::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {

    int renderPositionX = xPos;
    int renderPositionY = yPos;

    if (centered) {
        renderPositionX -= width / 2;
        renderPositionY -= height / 2;
    }

    // To anyone who needs to edit this, I hope you have an ultra-wide monitor
    renderSubrect(texture, 0, 0, padding, padding, renderPositionX, renderPositionY, padding, padding, nullptr);                                                                                                               // Top Left
    renderSubrect(texture, padding, 0, this->imgData.width - padding * 2, padding, renderPositionX + padding, renderPositionY, width - padding * 2, padding, nullptr);                                                         // Top
    renderSubrect(texture, this->imgData.width - padding, 0, padding, padding, renderPositionX + width - padding, renderPositionY, padding, padding, nullptr);                                                                 // Top Right
    renderSubrect(texture, 0, padding, padding, this->imgData.height - padding * 2, renderPositionX, renderPositionY + padding, padding, height - padding * 2, nullptr);                                                       // Left
    renderSubrect(texture, padding, padding, this->imgData.width - padding * 2, this->imgData.height - padding * 2, renderPositionX + padding, renderPositionY + padding, width - padding * 2, height - padding * 2, nullptr); // Center
    renderSubrect(texture, this->imgData.width - padding, padding, padding, this->imgData.height - padding * 2, renderPositionX + width - padding, renderPositionY + padding, padding, height - padding * 2, nullptr);         // Right
    renderSubrect(texture, 0, this->imgData.height - padding, padding, padding, renderPositionX, renderPositionY + height - padding, padding, padding, nullptr);                                                               // Bottom Left
    renderSubrect(texture, padding, this->imgData.height - padding, this->imgData.width - padding * 2, padding, renderPositionX + padding, renderPositionY + height - padding, width - padding * 2, padding, nullptr);         // Bottom
    renderSubrect(texture, this->imgData.width - padding, this->imgData.height - padding, padding, padding, renderPositionX + width - padding, renderPositionY + height - padding, padding, padding, nullptr);                 // Bottom Right
    freeTimer = maxFreeTimer;
}

void Image_C2D::setInitialTexture() {

    uint32_t *rgba_raw = reinterpret_cast<uint32_t *>(imgData.pixels);
    const int texWidth = getWidth();
    const int texHeight = getHeight();

    C3D_Tex *tex = new C3D_Tex();

    // Texture dimensions must be square powers of two between 64x64 and 1024x1024
    tex->width = std::clamp(Math::next_pow2(texWidth), static_cast<uint32_t>(64), static_cast<uint32_t>(1024));
    tex->height = std::clamp(Math::next_pow2(texHeight), static_cast<uint32_t>(64), static_cast<uint32_t>(1024));

    // Subtexture
    Tex3DS_SubTexture *subtex = new Tex3DS_SubTexture();

    subtex->width = texWidth;
    subtex->height = texHeight;

    subtex->left = 0.0f;
    subtex->top = 1.0f;
    subtex->right = (float)texWidth / (float)tex->width;
    subtex->bottom = 1.0 - ((float)texHeight / (float)tex->height);

    if (!C3D_TexInit(tex, tex->width, tex->height, GPU_RGBA8)) {
        delete tex;
        delete subtex;
        throw std::runtime_error("Texture initializing failed!");
    }

    C3D_TexSetFilter(tex, GPU_NEAREST, GPU_LINEAR);

    if (!tex->data) {
        C3D_TexDelete(tex);
        delete subtex;
        throw std::runtime_error("Texture data is null!");
    }

    size_t textureSize = tex->width * tex->height * 4;
    memset(tex->data, 0, textureSize);
    for (uint32_t i = 0; i < (uint32_t)texWidth; i++) {
        for (uint32_t j = 0; j < (uint32_t)texHeight; j++) {
            uint32_t src_idx = (j * texWidth) + i;
            uint32_t rgba_px = rgba_raw[src_idx];
            uint32_t abgr_px = rgbaToAgbr(rgba_px);

            // Swizzle magic to convert into a t3x format
            uint32_t dst_ptr_offset = ((((j >> 3) * (tex->width >> 3) + (i >> 3)) << 6) +
                                       ((i & 1) | ((j & 1) << 1) | ((i & 2) << 1) |
                                        ((j & 2) << 2) | ((i & 4) << 2) | ((j & 4) << 3)));
            ((uint32_t *)tex->data)[dst_ptr_offset] = abgr_px;
        }
    }

    texture.tex = tex;
    texture.subtex = subtex;
    free(imgData.pixels);

    // errr this MIGHT be risky but it seems to be fine
    imgData.pixels = texture.tex->data;
}

static inline uint32_t unswizzleRGBA8(const uint32_t *swizzled, int texWidth, int x, int y) {
    int tileX = x >> 3;
    int tileY = y >> 3;
    int tilesPerRow = texWidth >> 3;

    int tileIndex = tileY * tilesPerRow + tileX;
    int tileBase = tileIndex << 6; // * 64 pixels

    int inX = x & 7;
    int inY = y & 7;

    int pixelIndex =
        ((inX & 1)) |
        ((inY & 1) << 1) |
        ((inX & 2) << 1) |
        ((inY & 2) << 2) |
        ((inX & 4) << 2) |
        ((inY & 4) << 3);

    return swizzled[tileBase + pixelIndex];
}

// this function may be expensive since the data needs to be un-Swizzle Magic'ed.
ImageData Image_C2D::getPixels(ImageSubrect rect) {
    ImageData out{};

    if (!texture.tex || !texture.tex->data) {
        out.format = IMAGE_FORMAT_NONE;
        return out;
    }

    if (rect.x < 0) rect.x = 0;
    if (rect.y < 0) rect.y = 0;
    if (rect.x + rect.w > imgData.width) rect.w = imgData.width - rect.x;
    if (rect.y + rect.h > imgData.height) rect.h = imgData.height - rect.y;

    if (rect.w <= 0 || rect.h <= 0) {
        out.format = IMAGE_FORMAT_NONE;
        return out;
    }

    const int bytesPerPixel = 4;
    const int dstPitch = rect.w * bytesPerPixel;

    uint32_t *dstPixels = new uint32_t[rect.w * rect.h];
    const uint32_t *src = (const uint32_t *)texture.tex->data;

    int texWidth = texture.tex->width;

    for (int y = 0; y < rect.h; ++y) {
        for (int x = 0; x < rect.w; ++x) {
            int sx = rect.x + x;
            int sy = rect.y + y;

            uint32_t abgr = unswizzleRGBA8(src, texWidth, sx, sy);
            uint32_t rgba = rgbaToAgbr(abgr);

            dstPixels[y * rect.w + x] = rgba;
        }
    }

    out.width = rect.w;
    out.height = rect.h;
    out.format = IMAGE_FORMAT_RGBA32;
    out.pitch = dstPitch;
    out.pixels = (unsigned char *)dstPixels;

    return out;
}

void *Image_C2D::getNativeTexture() {
    return &texture;
}

Image_C2D::Image_C2D(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality) : Image(filePath, fromScratchProject, bitmapHalfQuality) {
    setInitialTexture();
}

Image_C2D::Image_C2D(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality) : Image(filePath, zip, bitmapHalfQuality) {
    setInitialTexture();
}

Image_C2D::~Image_C2D() {
    if (texture.tex) {
        imgData.pixels = nullptr;
        C3D_TexDelete(texture.tex);
        delete texture.tex;
        texture.tex = nullptr;
    }
    if (texture.subtex) {
        delete texture.subtex;
    }
}