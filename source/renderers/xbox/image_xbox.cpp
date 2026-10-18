#include "image_xbox.hpp"
#include "nonstd/expected.hpp"
#include "renderer_xbox.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <image.hpp>
#include <log.hpp>
#include <os.hpp>
#include <string>
#include <unordered_map>
#include <unzip.hpp>
#include <vector>
#include <runtime.hpp>

#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

static size_t getAvailablePhysicalMemory() {
    MM_STATISTICS stats;
    memset(&stats, 0, sizeof(stats));
    stats.Length = sizeof(stats);
    if (NT_SUCCESS(MmQueryStatistics(&stats))) {
        return (size_t)stats.AvailablePages * 4096;
    }
    return 0;
}

#define STB_DXT_IMPLEMENTATION
#define STB_DXT_STATIC
#include <stb_dxt.h>

#ifndef SDL_PIXELFORMAT_DXT1
#define SDL_PIXELFORMAT_DXT1 static_cast<SDL_PixelFormat>(SDL_DEFINE_PIXELFOURCC('D', 'X', 'T', '1'))
#endif

static inline int toPowerOfTwo(int v) {
    if (v <= 1) return 1;
    return 1 << (32 - __builtin_clz(v - 1));
}

// 2x2 Bayer dithering matrix centered around 0 to eliminate color banding on 16-bit conversions
static const int ditherMatrix[2][2] = {
    { -3,  1 },
    {  3, -1 }
};

// Images bigger than this (in either dimension) are split into tiles.
// 512 keeps each contiguous GPU allocation small (<=1MB at 32bpp, <=512KB at 16bpp, 128KB at DXT1) and
// avoids the power-of-two padding blowup of large swizzled textures.
static constexpr int TILE_SIZE = 512;

static bool compressAndUploadTileDXT1(SDL_Renderer *sdlRenderer, Image_Xbox::Tile &tile,
                                      const uint8_t *src, int srcPitch, int imgW, int imgH,
                                      int tx, int ty) {
    const int pot_w = toPowerOfTwo(tile.w);
    const int pot_h = toPowerOfTwo(tile.h);
    const int blocks_x = (pot_w + 3) / 4;
    const int blocks_y = (pot_h + 3) / 4;
    std::vector<uint8_t> dxt1Data(static_cast<size_t>(blocks_x) * blocks_y * 8);

    const int max_sy = std::min(ty + tile.h - 1, imgH - 1);
    const int max_sx = std::min(tx + tile.w - 1, imgW - 1);

    uint8_t block[64];
    for (int by = 0; by < blocks_y; by++) {
        for (int bx = 0; bx < blocks_x; bx++) {
            for (int py = 0; py < 4; py++) {
                int sy = ty + by * 4 + py;
                if (sy > max_sy) sy = max_sy;
                const uint8_t *row = src + sy * srcPitch;
                for (int px = 0; px < 4; px++) {
                    int sx = tx + bx * 4 + px;
                    if (sx > max_sx) sx = max_sx;
                    const uint8_t *pix = row + sx * 4;
                    block[(py * 4 + px) * 4 + 0] = pix[0];
                    block[(py * 4 + px) * 4 + 1] = pix[1];
                    block[(py * 4 + px) * 4 + 2] = pix[2];
                    block[(py * 4 + px) * 4 + 3] = 255;
                }
            }
            uint8_t *dst_block = dxt1Data.data() + (by * blocks_x + bx) * 8;
            stb_compress_dxt_block(dst_block, block, 0, STB_DXT_NORMAL);
        }
    }

    tile.texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_DXT1, SDL_TEXTUREACCESS_STATIC, tile.w, tile.h);
    if (!tile.texture) {
        Scratch::freeUnusedCostumeImages();
        tile.texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_DXT1, SDL_TEXTUREACCESS_STATIC, tile.w, tile.h);
    }
    if (!tile.texture) return false;

    SDL_SetTextureBlendMode(tile.texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(tile.texture, SDL_SCALEMODE_LINEAR);
    if (!SDL_UpdateTexture(tile.texture, nullptr, dxt1Data.data(), pot_w * 2)) {
        SDL_DestroyTexture(tile.texture);
        tile.texture = nullptr;
        return false;
    }
    return true;
}

static bool compressAndUploadImageDXT1(SDL_Renderer *sdlRenderer, SDL_Texture *&texture,
                                       const uint8_t *src, int srcPitch, int imgW, int imgH) {
    const int pot_w = toPowerOfTwo(imgW);
    const int pot_h = toPowerOfTwo(imgH);
    const int blocks_x = (pot_w + 3) / 4;
    const int blocks_y = (pot_h + 3) / 4;
    std::vector<uint8_t> dxt1Data(static_cast<size_t>(blocks_x) * blocks_y * 8);

    uint8_t block[64];
    for (int by = 0; by < blocks_y; by++) {
        for (int bx = 0; bx < blocks_x; bx++) {
            for (int py = 0; py < 4; py++) {
                int sy = by * 4 + py;
                if (sy >= imgH) sy = imgH - 1;
                const uint8_t *row = src + sy * srcPitch;
                for (int px = 0; px < 4; px++) {
                    int sx = bx * 4 + px;
                    if (sx >= imgW) sx = imgW - 1;
                    const uint8_t *pix = row + sx * 4;
                    block[(py * 4 + px) * 4 + 0] = pix[0];
                    block[(py * 4 + px) * 4 + 1] = pix[1];
                    block[(py * 4 + px) * 4 + 2] = pix[2];
                    block[(py * 4 + px) * 4 + 3] = 255;
                }
            }
            uint8_t *dst_block = dxt1Data.data() + (by * blocks_x + bx) * 8;
            stb_compress_dxt_block(dst_block, block, 0, STB_DXT_NORMAL);
        }
    }

    texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_DXT1, SDL_TEXTUREACCESS_STATIC, imgW, imgH);
    if (!texture) {
        Scratch::freeUnusedCostumeImages();
        texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_DXT1, SDL_TEXTUREACCESS_STATIC, imgW, imgH);
    }
    if (!texture) return false;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    if (!SDL_UpdateTexture(texture, nullptr, dxt1Data.data(), pot_w * 2)) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
        return false;
    }
    return true;
}

void Image_Xbox::destroyTiles() {
    for (auto &tile : tiles) {
        if (tile.texture) SDL_DestroyTexture(tile.texture);
    }
    tiles.clear();
}

nonstd::expected<void, std::string> Image_Xbox::buildTiles() {
    destroyTiles();

    const int tileSize = TILE_SIZE;
    const uint8_t *src = static_cast<const uint8_t *>(imgData.pixels);

    std::vector<uint16_t> scratch16;
    std::vector<uint32_t> scratch32;

    for (int ty = 0; ty < imgData.height; ty += tileSize) {
        for (int tx = 0; tx < imgData.width; tx += tileSize) {
            Tile tile;
            tile.x = tx;
            tile.y = ty;
            tile.w = std::min(tileSize, imgData.width - tx);
            tile.h = std::min(tileSize, imgData.height - ty);

            // Analyze transparency characteristics in this tile
            bool isOpaque = true;
            bool isBinaryAlpha = true;
            for (int y = 0; y < tile.h; y++) {
                const uint8_t *row = src + (ty + y) * imgData.pitch + tx * 4;
                for (int x = 0; x < tile.w; x++) {
                    const uint8_t a = row[x * 4 + 3];
                    if (a != 255) {
                        isOpaque = false;
                        if (a != 0) {
                            isBinaryAlpha = false;
                        }
                    }
                }
            }

            // Decide whether we can use relaxed (uncompressed 32-bit RGBA) compression:
            // 1) On 128MB systems (OS::isEnhancedPlatform()), RAM is abundant if freeMem > 20MB.
            // 2) For translucent tiles with multi-level alpha (where 16-bit looks degraded), try 32-bit if freeMem > 8MB.
            // 3) For opaque tiles, DXT1 is preferred on 64MB systems (128KB vs 1MB per tile) to prevent OOM.
            const size_t freeMem = getAvailablePhysicalMemory();
            const bool prefer32Bit = OS::isEnhancedPlatform() ? (freeMem > 20 * 1024 * 1024)
                                                              : (!isOpaque && !isBinaryAlpha && freeMem > 8 * 1024 * 1024);

            if (prefer32Bit) {
                tile.texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, tile.w, tile.h);
                if (tile.texture) {
                    scratch32.resize(static_cast<size_t>(tile.w) * tile.h);
                    for (int y = 0; y < tile.h; y++) {
                        const uint8_t *row = src + (ty + y) * imgData.pitch + tx * 4;
                        memcpy(scratch32.data() + y * tile.w, row, tile.w * 4);
                    }
                    SDL_SetTextureBlendMode(tile.texture, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureScaleMode(tile.texture, SDL_SCALEMODE_NEAREST);
                    if (SDL_UpdateTexture(tile.texture, nullptr, scratch32.data(), tile.w * 4)) {
                        tiles.push_back(tile);
                        continue;
                    }
                    SDL_DestroyTexture(tile.texture);
                    tile.texture = nullptr;
                }
            }

            // DXT1 compressed path for opaque tiles (128KB per 512x512 tile, saving 87.5% memory over 32-bit):
            if (isOpaque) {
                if (compressAndUploadTileDXT1(sdlRenderer, tile, src, imgData.pitch, imgData.width, imgData.height, tx, ty)) {
                    tiles.push_back(tile);
                    continue;
                }
            }

            // 16-bit compressed fallback path:
            // - Opaque: RGB565 (65,536 colors, dithered)
            // - Binary alpha (cutout): ARGB1555 (32,768 colors, dithered) - 8x better color than ARGB4444!
            // - Multi-level alpha: ARGB4444
            SDL_PixelFormat fmt;
            scratch16.resize(static_cast<size_t>(tile.w) * tile.h);

            if (isOpaque) {
                fmt = SDL_PIXELFORMAT_RGB565;
                for (int y = 0; y < tile.h; y++) {
                    const uint8_t *row = src + (ty + y) * imgData.pitch + tx * 4;
                    uint16_t *out = scratch16.data() + y * tile.w;
                    for (int x = 0; x < tile.w; x++) {
                        const int dither = ditherMatrix[y & 1][x & 1];
                        const int r = std::clamp(static_cast<int>(row[x * 4 + 0]) + dither, 0, 255) >> 3;
                        const int g = std::clamp(static_cast<int>(row[x * 4 + 1]) + (dither >> 1), 0, 255) >> 2;
                        const int b = std::clamp(static_cast<int>(row[x * 4 + 2]) + dither, 0, 255) >> 3;
                        out[x] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
                    }
                }
            } else if (isBinaryAlpha) {
                fmt = SDL_PIXELFORMAT_ARGB1555;
                for (int y = 0; y < tile.h; y++) {
                    const uint8_t *row = src + (ty + y) * imgData.pitch + tx * 4;
                    uint16_t *out = scratch16.data() + y * tile.w;
                    for (int x = 0; x < tile.w; x++) {
                        const int dither = ditherMatrix[y & 1][x & 1];
                        const int a = (row[x * 4 + 3] >= 128) ? 1 : 0;
                        const int r = std::clamp(static_cast<int>(row[x * 4 + 0]) + dither, 0, 255) >> 3;
                        const int g = std::clamp(static_cast<int>(row[x * 4 + 1]) + dither, 0, 255) >> 3;
                        const int b = std::clamp(static_cast<int>(row[x * 4 + 2]) + dither, 0, 255) >> 3;
                        out[x] = static_cast<uint16_t>((a << 15) | (r << 10) | (g << 5) | b);
                    }
                }
            } else {
                fmt = SDL_PIXELFORMAT_ARGB4444;
                for (int y = 0; y < tile.h; y++) {
                    const uint8_t *row = src + (ty + y) * imgData.pitch + tx * 4;
                    uint16_t *out = scratch16.data() + y * tile.w;
                    for (int x = 0; x < tile.w; x++) {
                        const int dither = ditherMatrix[y & 1][x & 1] * 2;
                        const int a = std::clamp(static_cast<int>(row[x * 4 + 3]) + dither, 0, 255) >> 4;
                        const int r = std::clamp(static_cast<int>(row[x * 4 + 0]) + dither, 0, 255) >> 4;
                        const int g = std::clamp(static_cast<int>(row[x * 4 + 1]) + dither, 0, 255) >> 4;
                        const int b = std::clamp(static_cast<int>(row[x * 4 + 2]) + dither, 0, 255) >> 4;
                        out[x] = static_cast<uint16_t>((a << 12) | (r << 8) | (g << 4) | b);
                    }
                }
            }

            tile.texture = SDL_CreateTexture(sdlRenderer, fmt, SDL_TEXTUREACCESS_STATIC, tile.w, tile.h);
            if (!tile.texture) {
                Scratch::freeUnusedCostumeImages();
                tile.texture = SDL_CreateTexture(sdlRenderer, fmt, SDL_TEXTUREACCESS_STATIC, tile.w, tile.h);
            }
            if (!tile.texture) {
                const std::string err = "Failed to create texture tile: " + std::string(SDL_GetError());
                Log::logError(err);
                destroyTiles();
                return nonstd::make_unexpected(err);
            }

            SDL_SetTextureBlendMode(tile.texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(tile.texture, SDL_SCALEMODE_NEAREST);

            if (!SDL_UpdateTexture(tile.texture, nullptr, scratch16.data(), tile.w * 2)) {
                const std::string err = "Failed to update texture tile: " + std::string(SDL_GetError());
                Log::logError(err);
                SDL_DestroyTexture(tile.texture);
                destroyTiles();
                return nonstd::make_unexpected(err);
            }

            tiles.push_back(tile);
        }
    }
    return {};
}

void Image_Xbox::renderTiled(ImageRenderParams &params) {
    const float &x = params.x;
    const float &y = params.y;
    const int &brightness = params.brightness;
    const double rotation = Math::radiansToDegrees(params.rotation);
    const float &scale = params.scale;
    const float &opacity = params.opacity;
    const SDL_FlipMode flip = params.flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    SDL_FRect renderRect;
    renderRect.w = imgData.width / imgData.scale * scale;
    renderRect.h = imgData.height / imgData.scale * scale;
    renderRect.x = params.centered ? x - renderRect.w / 2.0f : x;
    renderRect.y = params.centered ? y - renderRect.h / 2.0f : y;

    const double rotMod = std::fmod(std::abs(rotation), 90.0);
    const bool cardinal = rotMod < 0.001 || rotMod > 89.999;
    if (cardinal) {
        renderRect.x = std::round(renderRect.x);
        renderRect.y = std::round(renderRect.y);
        renderRect.w = std::round(renderRect.w);
        renderRect.h = std::round(renderRect.h);
    }

    SDL_FRect subRect = {0.0f, 0.0f, static_cast<float>(imgData.width), static_cast<float>(imgData.height)};
    if (params.subrect != nullptr) {
        subRect = {static_cast<float>(params.subrect->x), static_cast<float>(params.subrect->y),
                   static_cast<float>(params.subrect->w), static_cast<float>(params.subrect->h)};
    }
    if (subRect.w <= 0 || subRect.h <= 0) return;

    const float rx = renderRect.w / subRect.w;
    const float ry = renderRect.h / subRect.h;
    // Every tile rotates around the center of the whole image, not its own center.
    const SDL_FPoint imageCenter = {renderRect.x + renderRect.w / 2.0f, renderRect.y + renderRect.h / 2.0f};

    auto drawAll = [&]() {
        for (const auto &tile : tiles) {
            const float ix0 = std::max(static_cast<float>(tile.x), subRect.x);
            const float iy0 = std::max(static_cast<float>(tile.y), subRect.y);
            const float ix1 = std::min(static_cast<float>(tile.x + tile.w), subRect.x + subRect.w);
            const float iy1 = std::min(static_cast<float>(tile.y + tile.h), subRect.y + subRect.h);
            if (ix1 <= ix0 || iy1 <= iy0) continue;

            const SDL_FRect src = {ix0 - tile.x, iy0 - tile.y, ix1 - ix0, iy1 - iy0};

            float dx0 = (ix0 - subRect.x) * rx;
            float dx1 = (ix1 - subRect.x) * rx;
            float dy0 = (iy0 - subRect.y) * ry;
            float dy1 = (iy1 - subRect.y) * ry;
            if (params.flip) {
                const float t = renderRect.w - dx1;
                dx1 = renderRect.w - dx0;
                dx0 = t;
            }
            SDL_FRect dst = {renderRect.x + dx0, renderRect.y + dy0, dx1 - dx0, dy1 - dy0};
            if (cardinal) {
                // Snap shared edges so neighbouring tiles never leave seams.
                const float l = std::round(dst.x), t = std::round(dst.y);
                const float r = std::round(dst.x + dst.w), b = std::round(dst.y + dst.h);
                dst = {l, t, r - l, b - t};
            }
            const SDL_FPoint center = {imageCenter.x - dst.x, imageCenter.y - dst.y};
            SDL_RenderTextureRotated(sdlRenderer, tile.texture, &src, &dst, rotation, &center, flip);
        }
    };
    auto setAlpha = [&](Uint8 a) { for (auto &t : tiles) SDL_SetTextureAlphaMod(t.texture, a); };
    auto setColor = [&](Uint8 c) { for (auto &t : tiles) SDL_SetTextureColorMod(t.texture, c, c, c); };
    auto setBlend = [&](SDL_BlendMode m) { for (auto &t : tiles) SDL_SetTextureBlendMode(t.texture, m); };

    setAlpha(static_cast<Uint8>(opacity * 255));
    setColor(255);

    if (brightness > 0) {
        const float b = brightness * 0.01f;
        drawAll();
        setBlend(SDL_BLENDMODE_ADD);
        setAlpha(Uint8(255 * b * opacity));
        drawAll();
        setBlend(SDL_BLENDMODE_BLEND);
    } else if (brightness < 0) {
        const float b = brightness * 0.01f;
        setColor(Uint8(255 * std::clamp(1.0f + b, 0.0f, 1.0f)));
        drawAll();
        setColor(255);
    } else {
        drawAll();
    }
    freeTimer = maxFreeTimer;
}

void Image_Xbox::render(ImageRenderParams &params) {
    if (!tiles.empty()) {
        renderTiled(params);
        return;
    }
    if (!texture) return;

    const float &x = params.x;
    const float &y = params.y;
    const int &brightness = params.brightness;
    const double rotation = Math::radiansToDegrees(params.rotation);
    const float &scale = params.scale;
    const float &opacity = params.opacity;
    const bool &centered = params.centered;
    SDL_FlipMode flip = params.flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    SDL_FRect renderRect;
    renderRect.w = imgData.width / imgData.scale * scale;
    renderRect.h = imgData.height / imgData.scale * scale;

    SDL_FRect subRect;
    if (params.subrect != nullptr) {
        subRect = {
            .x = static_cast<float>(params.subrect->x),
            .y = static_cast<float>(params.subrect->y),
            .w = static_cast<float>(params.subrect->w),
            .h = static_cast<float>(params.subrect->h)};
    } else {
        subRect = {
            .x = 0.0f,
            .y = 0.0f,
            .w = static_cast<float>(imgData.width),
            .h = static_cast<float>(imgData.height)};
    }

    if (centered) {
        renderRect.x = x - (renderRect.w / 2.0f);
        renderRect.y = y - (renderRect.h / 2.0f);
    } else {
        renderRect.x = x;
        renderRect.y = y;
    }

    const bool isCardinalRotation = std::fmod(std::abs(rotation), 90.0) < 0.001 || std::fmod(std::abs(rotation), 90.0) > 89.999;
    if (isCardinalRotation) {
        renderRect.x = std::round(renderRect.x);
        renderRect.y = std::round(renderRect.y);
        renderRect.w = std::round(renderRect.w);
        renderRect.h = std::round(renderRect.h);
    }

    Uint8 alpha = static_cast<Uint8>(opacity * 255);
    SDL_SetTextureAlphaMod(texture, alpha);

    const SDL_FPoint center = {renderRect.w / 2.0f, renderRect.h / 2.0f};

    if (brightness != 0) {
        float b = brightness * 0.01f;
        if (brightness > 0.0f) {
            // render the normal image first
            SDL_RenderTextureRotated(sdlRenderer, texture, &subRect, &renderRect, rotation, &center, flip);

            // render another, blended image on top
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(texture, Uint8(255 * b * opacity));
            SDL_RenderTextureRotated(sdlRenderer, texture, &subRect, &renderRect, rotation, &center, flip);

            // reset for next frame
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        } else {
            Uint8 col = Uint8(255 * std::clamp(1.0f + b, 0.0f, 1.0f));
            SDL_SetTextureColorMod(texture, col, col, col);

            SDL_RenderTextureRotated(sdlRenderer, texture, &subRect, &renderRect, rotation, &center, flip);
            // reset for next frame
            SDL_SetTextureColorMod(texture, 255, 255, 255);
        }
    } else {
        // if no brightness just render normal image
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        SDL_RenderTextureRotated(sdlRenderer, texture, &subRect, &renderRect, rotation, &center, flip);
    }
    freeTimer = maxFreeTimer;
}

// I doubt you want to mess with this...
void Image_Xbox::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    if (!texture) return;

    const float destX = xPos - (centered ? width / 2.0f : 0);
    const float destY = yPos - (centered ? height / 2.0f : 0);
    const float srcPadding = std::max(1.0f, std::min(std::min(static_cast<float>(padding), getWidth() / 2.0f), static_cast<float>(getHeight())) / 2.0f);

    const float srcCenterWidth = std::max(0.0f, imgData.width - 2 * srcPadding);
    const float srcCenterHeight = std::max(0.0f, imgData.height - 2 * srcPadding);

    const SDL_FRect srcTopLeft = {0, 0, srcPadding, srcPadding};
    const SDL_FRect srcTop = {srcPadding, 0, srcCenterWidth, srcPadding};
    const SDL_FRect srcTopRight = {imgData.width - srcPadding, 0, srcPadding, srcPadding};
    const SDL_FRect srcLeft = {0, srcPadding, srcPadding, srcCenterHeight};
    const SDL_FRect srcCenter = {srcPadding, srcPadding, srcCenterWidth, srcCenterHeight};
    const SDL_FRect srcRight = {imgData.width - srcPadding, srcPadding, srcPadding, srcCenterHeight};
    const SDL_FRect srcBottomLeft = {0, imgData.height - srcPadding, srcPadding, srcPadding};
    const SDL_FRect srcBottom = {srcPadding, imgData.height - srcPadding, srcCenterWidth, srcPadding};
    const SDL_FRect srcBottomRight = {imgData.width - srcPadding, imgData.height - srcPadding, srcPadding, srcPadding};

    const float dstCenterWidth = std::max(0.0f, static_cast<float>(width) - 2 * srcPadding);
    const float dstCenterHeight = std::max(0.0f, static_cast<float>(height) - 2 * srcPadding);

    const SDL_FRect dstTopLeft = {destX, destY, srcPadding, srcPadding};
    const SDL_FRect dstTop = {destX + srcPadding, destY, dstCenterWidth, srcPadding};
    const SDL_FRect dstTopRight = {destX + srcPadding + dstCenterWidth, destY, srcPadding, srcPadding};

    const SDL_FRect dstLeft = {destX, destY + srcPadding, srcPadding, dstCenterHeight};
    const SDL_FRect dstCenter = {destX + srcPadding, destY + srcPadding, dstCenterWidth, dstCenterHeight};
    const SDL_FRect dstRight = {destX + srcPadding + dstCenterWidth, destY + srcPadding, srcPadding, dstCenterHeight};
    const SDL_FRect dstBottomLeft = {destX, destY + srcPadding + dstCenterHeight, srcPadding, srcPadding};
    const SDL_FRect dstBottom = {destX + srcPadding, destY + srcPadding + dstCenterHeight, dstCenterWidth, srcPadding};
    const SDL_FRect dstBottomRight = {destX + srcPadding + dstCenterWidth, destY + srcPadding + dstCenterHeight, srcPadding, srcPadding};

    SDL_ScaleMode originalScaleMode;
    SDL_GetTextureScaleMode(texture, &originalScaleMode);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    SDL_RenderTexture(sdlRenderer, texture, &srcTopLeft, &dstTopLeft);
    SDL_RenderTexture(sdlRenderer, texture, &srcTop, &dstTop);
    SDL_RenderTexture(sdlRenderer, texture, &srcTopRight, &dstTopRight);
    SDL_RenderTexture(sdlRenderer, texture, &srcLeft, &dstLeft);
    SDL_RenderTexture(sdlRenderer, texture, &srcCenter, &dstCenter);
    SDL_RenderTexture(sdlRenderer, texture, &srcRight, &dstRight);
    SDL_RenderTexture(sdlRenderer, texture, &srcBottomLeft, &dstBottomLeft);
    SDL_RenderTexture(sdlRenderer, texture, &srcBottom, &dstBottom);
    SDL_RenderTexture(sdlRenderer, texture, &srcBottomRight, &dstBottomRight);

    SDL_SetTextureScaleMode(texture, originalScaleMode);
    freeTimer = maxFreeTimer;
}

void *Image_Xbox::getNativeTexture() {
    return texture;
}

nonstd::expected<void, std::string> Image_Xbox::setInitialTexture() {
    if (imgData.width > TILE_SIZE || imgData.height > TILE_SIZE) return buildTiles();
    texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, imgData.width, imgData.height);

    if (!texture) {
        Scratch::freeUnusedCostumeImages();
        texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, imgData.width, imgData.height);
    }

    if (!texture) {
        // Fall back to DXT1 (if opaque) or 16-bit to avoid Out Of Memory on projects with many assets
        bool isOpaque = true;
        bool isBinaryAlpha = true;
        const uint8_t *src = static_cast<const uint8_t *>(imgData.pixels);
        for (int y = 0; y < imgData.height; y++) {
            const uint8_t *row = src + y * imgData.pitch;
            for (int x = 0; x < imgData.width; x++) {
                uint8_t a = row[x * 4 + 3];
                if (a != 255) {
                    isOpaque = false;
                    if (a != 0) isBinaryAlpha = false;
                }
            }
        }

        if (isOpaque) {
            if (compressAndUploadImageDXT1(sdlRenderer, texture, src, imgData.pitch, imgData.width, imgData.height)) {
                return {};
            }
        }

        const SDL_PixelFormat fmt = isOpaque ? SDL_PIXELFORMAT_RGB565 : (isBinaryAlpha ? SDL_PIXELFORMAT_ARGB1555 : SDL_PIXELFORMAT_ARGB4444);
        texture = SDL_CreateTexture(sdlRenderer, fmt, SDL_TEXTUREACCESS_STATIC, imgData.width, imgData.height);
        if (!texture) {
            Scratch::freeUnusedCostumeImages();
            texture = SDL_CreateTexture(sdlRenderer, fmt, SDL_TEXTUREACCESS_STATIC, imgData.width, imgData.height);
        }

        if (texture) {
            std::vector<uint16_t> scratch16(static_cast<size_t>(imgData.width) * imgData.height);
            if (isOpaque) {
                for (int y = 0; y < imgData.height; y++) {
                    const uint8_t *row = src + y * imgData.pitch;
                    uint16_t *out = scratch16.data() + y * imgData.width;
                    for (int x = 0; x < imgData.width; x++) {
                        int dither = ditherMatrix[y & 1][x & 1];
                        int r = std::clamp(static_cast<int>(row[x * 4 + 0]) + dither, 0, 255) >> 3;
                        int g = std::clamp(static_cast<int>(row[x * 4 + 1]) + (dither >> 1), 0, 255) >> 2;
                        int b = std::clamp(static_cast<int>(row[x * 4 + 2]) + dither, 0, 255) >> 3;
                        out[x] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
                    }
                }
            } else if (isBinaryAlpha) {
                for (int y = 0; y < imgData.height; y++) {
                    const uint8_t *row = src + y * imgData.pitch;
                    uint16_t *out = scratch16.data() + y * imgData.width;
                    for (int x = 0; x < imgData.width; x++) {
                        int dither = ditherMatrix[y & 1][x & 1];
                        int a = (row[x * 4 + 3] >= 128) ? 1 : 0;
                        int r = std::clamp(static_cast<int>(row[x * 4 + 0]) + dither, 0, 255) >> 3;
                        int g = std::clamp(static_cast<int>(row[x * 4 + 1]) + dither, 0, 255) >> 3;
                        int b = std::clamp(static_cast<int>(row[x * 4 + 2]) + dither, 0, 255) >> 3;
                        out[x] = static_cast<uint16_t>((a << 15) | (r << 10) | (g << 5) | b);
                    }
                }
            } else {
                for (int y = 0; y < imgData.height; y++) {
                    const uint8_t *row = src + y * imgData.pitch;
                    uint16_t *out = scratch16.data() + y * imgData.width;
                    for (int x = 0; x < imgData.width; x++) {
                        int dither = ditherMatrix[y & 1][x & 1] * 2;
                        int a = std::clamp(static_cast<int>(row[x * 4 + 3]) + dither, 0, 255) >> 4;
                        int r = std::clamp(static_cast<int>(row[x * 4 + 0]) + dither, 0, 255) >> 4;
                        int g = std::clamp(static_cast<int>(row[x * 4 + 1]) + dither, 0, 255) >> 4;
                        int b = std::clamp(static_cast<int>(row[x * 4 + 2]) + dither, 0, 255) >> 4;
                        out[x] = static_cast<uint16_t>((a << 12) | (r << 8) | (g << 4) | b);
                    }
                }
            }

            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
            if (!SDL_UpdateTexture(texture, nullptr, scratch16.data(), imgData.width * 2)) {
                SDL_DestroyTexture(texture);
                texture = nullptr;
            } else {
                return {};
            }
        }
    }

    if (!texture) {
        const std::string err = "Failed to create texture: " + std::string(SDL_GetError());
        Log::logError(err);
        return nonstd::make_unexpected(err);
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    if (!SDL_UpdateTexture(texture, nullptr, imgData.pixels, imgData.pitch)) {
        const std::string err = "Failed to update texture: " + std::string(SDL_GetError());
        Log::logError(err);
        return nonstd::make_unexpected(err);
    }

    return {};
}

nonstd::expected<void, std::string> Image_Xbox::refreshTexture() {
    destroyTiles();
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    return setInitialTexture();
}

Image_Xbox::Image_Xbox(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
    sdlRenderer = static_cast<Renderer_Xbox *>(Render::getGlobalRenderer())->Sdlrenderer;

    const unsigned int maxTextureSizeSquare = SDL_GetNumberProperty(SDL_GetRendererProperties(sdlRenderer), SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0);
    maxTextureSize = {maxTextureSizeSquare, maxTextureSizeSquare};

    const auto initResult = init(filePath, zip, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    const auto potentialError = setInitialTexture();
    if (!potentialError.has_value()) error = potentialError.error();
}

Image_Xbox::Image_Xbox(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    sdlRenderer = static_cast<Renderer_Xbox *>(Render::getGlobalRenderer())->Sdlrenderer;

    const unsigned int maxTextureSizeSquare = SDL_GetNumberProperty(SDL_GetRendererProperties(sdlRenderer), SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0);
    maxTextureSize = {maxTextureSizeSquare, maxTextureSizeSquare};

    const auto initResult = init(filePath, fromScratchProject, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    const auto potentialError = setInitialTexture();
    if (!potentialError.has_value()) error = potentialError.error();
}

Image_Xbox::~Image_Xbox() {
    destroyTiles();
    SDL_DestroyTexture(texture);
}
