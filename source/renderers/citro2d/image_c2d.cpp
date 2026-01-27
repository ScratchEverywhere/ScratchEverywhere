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
        subtex = {
            .width = static_cast<uint16_t>(getWidth()),
            .height = static_cast<uint16_t>(getHeight()),
            .left = 0.0f,
            .top = 1.0f,
            .right = (float)getWidth() / (float)texture.tex->width,
            .bottom = 1.0f - ((float)getHeight() / (float)texture.tex->height)};
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
    renderSubrect(texture, 0, 0, padding, padding, renderPositionX, renderPositionY, padding, padding, nullptr);                                                                                               // Top Left
    renderSubrect(texture, padding, 0, this->width - padding * 2, padding, renderPositionX + padding, renderPositionY, width - padding * 2, padding, nullptr);                                                 // Top
    renderSubrect(texture, this->width - padding, 0, padding, padding, renderPositionX + width - padding, renderPositionY, padding, padding, nullptr);                                                         // Top Right
    renderSubrect(texture, 0, padding, padding, this->height - padding * 2, renderPositionX, renderPositionY + padding, padding, height - padding * 2, nullptr);                                               // Left
    renderSubrect(texture, padding, padding, this->width - padding * 2, this->height - padding * 2, renderPositionX + padding, renderPositionY + padding, width - padding * 2, height - padding * 2, nullptr); // Center
    renderSubrect(texture, this->width - padding, padding, padding, this->height - padding * 2, renderPositionX + width - padding, renderPositionY + padding, padding, height - padding * 2, nullptr);         // Right
    renderSubrect(texture, 0, this->height - padding, padding, padding, renderPositionX, renderPositionY + height - padding, padding, padding, nullptr);                                                       // Bottom Left
    renderSubrect(texture, padding, this->height - padding, this->width - padding * 2, padding, renderPositionX + padding, renderPositionY + height - padding, width - padding * 2, padding, nullptr);         // Bottom
    renderSubrect(texture, this->width - padding, this->height - padding, padding, padding, renderPositionX + width - padding, renderPositionY + height - padding, padding, padding, nullptr);                 // Bottom Right
    freeTimer = maxFreeTimer;
}

void Image_C2D::setInitialTexture() {

    uint32_t *rgba_raw = reinterpret_cast<uint32_t *>(pixels);
    const int texWidth = getWidth();
    const int texHeight = getHeight();

    C3D_Tex *tex = new C3D_Tex();

    // Texture dimensions must be square powers of two between 64x64 and 1024x1024
    tex->width = Math::next_pow2(texWidth);
    tex->height = Math::next_pow2(texHeight);

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

    C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);

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
    free(pixels);

    // errr this MIGHT be risky but it seems to be fine
    pixels = texture.tex->data;
}

Image_C2D::Image_C2D(std::string filePath, bool fromScratchProject) : Image(filePath, fromScratchProject) {
    setInitialTexture();
}

Image_C2D::Image_C2D(std::string filePath, mz_zip_archive *zip) : Image(filePath, zip) {
    setInitialTexture();
}

Image_C2D::~Image_C2D() {
    if (texture.tex) {
        pixels = nullptr;
        C3D_TexDelete(texture.tex);
        delete texture.tex;
        texture.tex = nullptr;
    }
    if (texture.subtex) {
        delete texture.subtex;
    }
}