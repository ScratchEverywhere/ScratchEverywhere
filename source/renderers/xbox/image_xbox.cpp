#include "image_xbox.hpp"
#include "nonstd/expected.hpp"
#include "renderer_xbox.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem.hpp>
#include <image.hpp>
#include <log.hpp>
#include <os.hpp>
#include <runtime.hpp>
#include <stb_image.h>
#include <string>
#include <translation.hpp>
#include <unordered_map>
#include <unzip.hpp>
#include <vector>

#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

#pragma pack(push, 1)
struct XBTHeader {
    char magic[4];       // "XBT1"
    uint32_t version;    // 1
    uint32_t width;      // original full width
    uint32_t height;     // original full height
    uint32_t isTiled;    // 1 if tiled, 0 if single texture
    uint32_t numTiles;   // number of tiles
};

struct XBTTileHeader {
    int32_t x;           // tile x offset in image
    int32_t y;           // tile y offset in image
    int32_t w;           // tile width
    int32_t h;           // tile height
    uint32_t dataSize;   // byte length of DXT1 data following
};
#pragma pack(pop)

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

static inline void unpackRGB565(uint16_t c, int &r, int &g, int &b) {
    r = (c >> 11) & 31;
    g = (c >> 5) & 63;
    b = c & 31;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
}

// Writes exactly 8 bytes (DXT1). Alpha is treated as 1-bit when !isOpaque.
// NOTE: stb_compress_dxt_block(alpha=1) emits a 16-byte DXT5 block, so it must never be used here.
static void compressDXT1BlockFast(uint8_t *dst_block, const uint8_t *block, bool isOpaque) {
    if (isOpaque) {
        stb_compress_dxt_block(dst_block, block, 0, STB_DXT_NORMAL);
        return;
    }

    uint8_t fixedBlock[64];
    uint16_t opaqueMask = 0;
    int count = 0, sr = 0, sg = 0, sb = 0;
    for (int i = 0; i < 16; i++) {
        if (block[i * 4 + 3] >= 128) {
            opaqueMask |= (1 << i);
            sr += block[i * 4 + 0];
            sg += block[i * 4 + 1];
            sb += block[i * 4 + 2];
            count++;
        }
    }

    if (count == 0) {
        // Fully transparent: c0 == c1 (3-colour mode), all indices = 3
        memset(dst_block, 0, 4);
        memset(dst_block + 4, 0xFF, 4);
        return;
    }

    const uint8_t ar = static_cast<uint8_t>(sr / count);
    const uint8_t ag = static_cast<uint8_t>(sg / count);
    const uint8_t ab = static_cast<uint8_t>(sb / count);
    for (int i = 0; i < 16; i++) {
        if (opaqueMask & (1 << i)) {
            memcpy(fixedBlock + i * 4, block + i * 4, 3);
        } else {
            fixedBlock[i * 4 + 0] = ar;
            fixedBlock[i * 4 + 1] = ag;
            fixedBlock[i * 4 + 2] = ab;
        }
        fixedBlock[i * 4 + 3] = 255;
    }

    uint8_t tmp[8];
    stb_compress_dxt_block(tmp, fixedBlock, 0, STB_DXT_NORMAL);
    const uint16_t e0 = static_cast<uint16_t>(tmp[0] | (tmp[1] << 8));
    const uint16_t e1 = static_cast<uint16_t>(tmp[2] | (tmp[3] << 8));
    // 3-colour mode requires c0 <= c1
    const uint16_t c0 = e0 < e1 ? e0 : e1;
    const uint16_t c1 = e0 < e1 ? e1 : e0;

    int pal[3][3];
    unpackRGB565(c0, pal[0][0], pal[0][1], pal[0][2]);
    unpackRGB565(c1, pal[1][0], pal[1][1], pal[1][2]);
    pal[2][0] = (pal[0][0] + pal[1][0]) / 2;
    pal[2][1] = (pal[0][1] + pal[1][1]) / 2;
    pal[2][2] = (pal[0][2] + pal[1][2]) / 2;

    uint32_t indices = 0;
    for (int i = 0; i < 16; i++) {
        uint32_t idx = 3;
        if (opaqueMask & (1 << i)) {
            int best = 0, bestD = 0x7FFFFFFF;
            for (int k = 0; k < 3; k++) {
                const int dr = block[i * 4 + 0] - pal[k][0];
                const int dg = block[i * 4 + 1] - pal[k][1];
                const int db = block[i * 4 + 2] - pal[k][2];
                const int d = dr * dr + dg * dg + db * db;
                if (d < bestD) {
                    bestD = d;
                    best = k;
                }
            }
            idx = static_cast<uint32_t>(best);
        }
        indices |= idx << (i * 2);
    }

    dst_block[0] = static_cast<uint8_t>(c0 & 0xFF);
    dst_block[1] = static_cast<uint8_t>(c0 >> 8);
    dst_block[2] = static_cast<uint8_t>(c1 & 0xFF);
    dst_block[3] = static_cast<uint8_t>(c1 >> 8);
    dst_block[4] = static_cast<uint8_t>(indices);
    dst_block[5] = static_cast<uint8_t>(indices >> 8);
    dst_block[6] = static_cast<uint8_t>(indices >> 16);
    dst_block[7] = static_cast<uint8_t>(indices >> 24);
}

static bool compressAndUploadTileDXT1(SDL_Renderer *sdlRenderer, Image_Xbox::Tile &tile,
                                      const uint8_t *src, int srcPitch, int imgW, int imgH,
                                      int tx, int ty, bool isOpaque = true) {
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
                    block[(py * 4 + px) * 4 + 3] = isOpaque ? 255 : (pix[3] >= 128 ? 255 : 0);
                }
            }
            uint8_t *dst_block = dxt1Data.data() + (by * blocks_x + bx) * 8;
            compressDXT1BlockFast(dst_block, block, isOpaque);
        }
    }

    tile.texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_DXT1, SDL_TEXTUREACCESS_STATIC, tile.w, tile.h);
    if (!tile.texture) {
        Scratch::freeUnusedCostumeImages();
        tile.texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_DXT1, SDL_TEXTUREACCESS_STATIC, tile.w, tile.h);
    }
    if (!tile.texture) return false;

    SDL_SetTextureBlendMode(tile.texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tile.texture, SDL_SCALEMODE_LINEAR);
    if (!SDL_UpdateTexture(tile.texture, nullptr, dxt1Data.data(), pot_w * 2)) {
        SDL_DestroyTexture(tile.texture);
        tile.texture = nullptr;
        return false;
    }
    return true;
}

static bool compressAndUploadImageDXT1(SDL_Renderer *sdlRenderer, SDL_Texture *&texture,
                                       const uint8_t *src, int srcPitch, int imgW, int imgH, bool isOpaque = true) {
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
                    block[(py * 4 + px) * 4 + 3] = isOpaque ? 255 : (pix[3] >= 128 ? 255 : 0);
                }
            }
            uint8_t *dst_block = dxt1Data.data() + (by * blocks_x + bx) * 8;
            compressDXT1BlockFast(dst_block, block, isOpaque);
        }
    }

    texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_DXT1, SDL_TEXTUREACCESS_STATIC, imgW, imgH);
    if (!texture) {
        Scratch::freeUnusedCostumeImages();
        texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_DXT1, SDL_TEXTUREACCESS_STATIC, imgW, imgH);
    }
    if (!texture) return false;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
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

            // DXT1 compressed path for opaque and binary alpha (cutout) tiles (128KB per 512x512 tile).
            // Pixels stay resident in imgData, so alpha-based collision keeps working.
            if (isOpaque || isBinaryAlpha) {
                if (compressAndUploadTileDXT1(sdlRenderer, tile, src, imgData.pitch, imgData.width, imgData.height, tx, ty, isOpaque)) {
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
    if (tiles.empty()) return;
    const float &x = params.x;
    const float &y = params.y;
    const int &brightness = params.brightness;
    const double rotation = Math::radiansToDegrees(params.rotation);
    const float &scale = params.scale;
    const float &opacity = params.opacity;
    const SDL_FlipMode flip = params.flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    if (opacity <= 0.001f) return;
    const Uint8 alpha = static_cast<Uint8>(opacity * 255);
    if (alpha == 0) return;

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

    const SDL_BlendMode normalBlendMode = (params.blend || alpha < 255) ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE;
    setAlpha(alpha);
    setColor(255);
    setBlend(normalBlendMode);

    if (brightness > 0) {
        const float b = brightness * 0.01f;
        drawAll();
        setBlend(SDL_BLENDMODE_ADD);
        setAlpha(Uint8(255 * b * opacity));
        drawAll();
        setBlend(normalBlendMode);
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

    if (opacity <= 0.001f) return;
    const Uint8 alpha = static_cast<Uint8>(opacity * 255);
    if (alpha == 0) return;

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

    SDL_SetTextureAlphaMod(texture, alpha);
    const SDL_BlendMode normalBlendMode = (params.blend || alpha < 255) ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE;
    SDL_SetTextureBlendMode(texture, normalBlendMode);

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
            SDL_SetTextureBlendMode(texture, normalBlendMode);
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
    if (!tiles.empty()) return tiles[0].texture;
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

        if (isOpaque || isBinaryAlpha) {
            if (compressAndUploadImageDXT1(sdlRenderer, texture, src, imgData.pitch, imgData.width, imgData.height, isOpaque)) {
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

static std::string getProjectCacheKey() {
    std::string key = Unzip::filePath;
    if (key.empty()) {
        key = "embedded";
    } else {
        size_t lastSlash = key.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            key = key.substr(lastSlash + 1);
        }
        size_t dot = key.find_last_of('.');
        if (dot != std::string::npos) {
            key = key.substr(0, dot);
        }
    }
    std::string clean;
    for (char c : key) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            clean += c;
        }
    }
    if (clean.empty()) clean = "project";
    if (clean.length() > 24) clean = clean.substr(0, 24);
    return clean;
}

std::string Image_Xbox::getTextureCacheDir() {
    return OS::getScratchFolderLocation() + "cache\\" + getProjectCacheKey() + "_v2\\";
}

std::string Image_Xbox::getTextureCachePath(const std::string &assetName) {
    if (assetName.empty()) return "";
    std::string base = assetName;
    size_t lastSlash = base.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        base = base.substr(lastSlash + 1);
    }
    size_t lastDot = base.find_last_of('.');
    if (lastDot != std::string::npos) {
        base = base.substr(0, lastDot);
    }
    if (base.empty()) return "";
    if (base.length() > 36) base = base.substr(0, 36);
    return getTextureCacheDir() + base + ".xbt";
}

static bool writeCompressedTextureCache(const std::string &cachePath, int width, int height, const uint8_t *src, int pitch) {
    std::string tempPath = cachePath + ".tmp";
    FILE *f = fopen(tempPath.c_str(), "wb");
    if (!f) return false;

    const bool isTiled = (width > TILE_SIZE || height > TILE_SIZE);
    XBTHeader header;
    memcpy(header.magic, "XBT1", 4);
    header.version = 1;
    header.width = width;
    header.height = height;
    header.isTiled = isTiled ? 1 : 0;

    if (!isTiled) {
        header.numTiles = 1;
        if (fwrite(&header, sizeof(header), 1, f) != 1) {
            fclose(f);
            remove(tempPath.c_str());
            return false;
        }

        const int pot_w = toPowerOfTwo(width);
        const int pot_h = toPowerOfTwo(height);
        const int blocks_x = (pot_w + 3) / 4;
        const int blocks_y = (pot_h + 3) / 4;
        const uint32_t dataSize = static_cast<uint32_t>(blocks_x) * blocks_y * 8;

        std::vector<uint8_t> dxt1Data(dataSize);
        uint8_t block[64];
        for (int by = 0; by < blocks_y; by++) {
            for (int bx = 0; bx < blocks_x; bx++) {
                for (int py = 0; py < 4; py++) {
                    int sy = by * 4 + py;
                    if (sy >= height) sy = height - 1;
                    const uint8_t *row = src + sy * pitch;
                    for (int px = 0; px < 4; px++) {
                        int sx = bx * 4 + px;
                        if (sx >= width) sx = width - 1;
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

        XBTTileHeader tileHeader;
        tileHeader.x = 0;
        tileHeader.y = 0;
        tileHeader.w = width;
        tileHeader.h = height;
        tileHeader.dataSize = dataSize;

        if (fwrite(&tileHeader, sizeof(tileHeader), 1, f) != 1 ||
            fwrite(dxt1Data.data(), 1, dataSize, f) != dataSize) {
            fclose(f);
            remove(tempPath.c_str());
            return false;
        }
    } else {
        uint32_t numTiles = 0;
        for (int ty = 0; ty < height; ty += TILE_SIZE) {
            for (int tx = 0; tx < width; tx += TILE_SIZE) {
                numTiles++;
            }
        }
        header.numTiles = numTiles;
        if (fwrite(&header, sizeof(header), 1, f) != 1) {
            fclose(f);
            remove(tempPath.c_str());
            return false;
        }

        for (int ty = 0; ty < height; ty += TILE_SIZE) {
            for (int tx = 0; tx < width; tx += TILE_SIZE) {
                const int tw = std::min(TILE_SIZE, width - tx);
                const int th = std::min(TILE_SIZE, height - ty);
                const int pot_w = toPowerOfTwo(tw);
                const int pot_h = toPowerOfTwo(th);
                const int blocks_x = (pot_w + 3) / 4;
                const int blocks_y = (pot_h + 3) / 4;
                const uint32_t dataSize = static_cast<uint32_t>(blocks_x) * blocks_y * 8;

                std::vector<uint8_t> dxt1Data(dataSize);
                const int max_sy = std::min(ty + th - 1, height - 1);
                const int max_sx = std::min(tx + tw - 1, width - 1);

                uint8_t block[64];
                for (int by = 0; by < blocks_y; by++) {
                    for (int bx = 0; bx < blocks_x; bx++) {
                        for (int py = 0; py < 4; py++) {
                            int sy = ty + by * 4 + py;
                            if (sy > max_sy) sy = max_sy;
                            const uint8_t *row = src + sy * pitch;
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

                XBTTileHeader tileHeader;
                tileHeader.x = tx;
                tileHeader.y = ty;
                tileHeader.w = tw;
                tileHeader.h = th;
                tileHeader.dataSize = dataSize;

                if (fwrite(&tileHeader, sizeof(tileHeader), 1, f) != 1 ||
                    fwrite(dxt1Data.data(), 1, dataSize, f) != dataSize) {
                    fclose(f);
                    remove(tempPath.c_str());
                    return false;
                }
            }
        }
    }

    fclose(f);
    remove(cachePath.c_str());
    rename(tempPath.c_str(), cachePath.c_str());
    return true;
}

bool Image_Xbox::loadFromCache(const std::string &cachePath) {
    FILE *f = fopen(cachePath.c_str(), "rb");
    if (!f) return false;

    XBTHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return false;
    }
    if (memcmp(header.magic, "XBT1", 4) != 0 || header.version != 1) {
        fclose(f);
        return false;
    }

    imgData.width = header.width;
    imgData.height = header.height;
    imgData.scale = 1.0f;
    imgData.pitch = header.width * 4;
    imgData.pixels = nullptr;
    isOpaque = true;

    if (header.isTiled == 0) {
        XBTTileHeader tileHeader;
        if (fread(&tileHeader, sizeof(tileHeader), 1, f) != 1) {
            fclose(f);
            return false;
        }
        std::vector<uint8_t> dxt1Data(tileHeader.dataSize);
        if (fread(dxt1Data.data(), 1, tileHeader.dataSize, f) != tileHeader.dataSize) {
            fclose(f);
            return false;
        }
        fclose(f);

        texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_DXT1, SDL_TEXTUREACCESS_STATIC, tileHeader.w, tileHeader.h);
        if (!texture) {
            Scratch::freeUnusedCostumeImages();
            texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_DXT1, SDL_TEXTUREACCESS_STATIC, tileHeader.w, tileHeader.h);
        }
        if (!texture) return false;

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
        const int pot_w = toPowerOfTwo(tileHeader.w);
        if (!SDL_UpdateTexture(texture, nullptr, dxt1Data.data(), pot_w * 2)) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
            return false;
        }
        return true;
    } else {
        destroyTiles();
        for (uint32_t i = 0; i < header.numTiles; i++) {
            XBTTileHeader tileHeader;
            if (fread(&tileHeader, sizeof(tileHeader), 1, f) != 1) {
                destroyTiles();
                fclose(f);
                return false;
            }
            std::vector<uint8_t> dxt1Data(tileHeader.dataSize);
            if (fread(dxt1Data.data(), 1, tileHeader.dataSize, f) != tileHeader.dataSize) {
                destroyTiles();
                fclose(f);
                return false;
            }
            Tile tile;
            tile.x = tileHeader.x;
            tile.y = tileHeader.y;
            tile.w = tileHeader.w;
            tile.h = tileHeader.h;
            tile.texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_DXT1, SDL_TEXTUREACCESS_STATIC, tile.w, tile.h);
            if (!tile.texture) {
                Scratch::freeUnusedCostumeImages();
                tile.texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_DXT1, SDL_TEXTUREACCESS_STATIC, tile.w, tile.h);
            }
            if (!tile.texture) {
                destroyTiles();
                fclose(f);
                return false;
            }
            SDL_SetTextureBlendMode(tile.texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(tile.texture, SDL_SCALEMODE_LINEAR);
            const int pot_w = toPowerOfTwo(tile.w);
            if (!SDL_UpdateTexture(tile.texture, nullptr, dxt1Data.data(), pot_w * 2)) {
                SDL_DestroyTexture(tile.texture);
                tile.texture = nullptr;
                destroyTiles();
                fclose(f);
                return false;
            }
            tiles.push_back(tile);
        }
        fclose(f);
        return true;
    }
}

static void *extractProjectAsset(const std::string &assetName, size_t &outSize) {
    if (Unzip::loader) {
        void *data = Unzip::loader->getAsset(assetName, &outSize);
        if (data) return data;
    }
    std::string fullPath;
    if (Unzip::UnpackedInSD) fullPath = Unzip::filePath + assetName;
    else fullPath = OS::getRomFSLocation() + "project/" + assetName;

    FILE *f = fopen(fullPath.c_str(), "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return nullptr; }
    void *buf = malloc(sz);
    if (!buf) { fclose(f); return nullptr; }
    if (fread(buf, 1, sz, f) != static_cast<size_t>(sz)) {
        free(buf);
        fclose(f);
        return nullptr;
    }
    fclose(f);
    outSize = static_cast<size_t>(sz);
    return buf;
}

static void freeProjectAsset(void *ptr) {
    if (!ptr) return;
    if (Unzip::zipArchive) {
        Unzip::zipArchive->freeHeap(ptr);
        return;
    }
    free(ptr);
}

void Image_Xbox::precacheProjectTextures() {
    std::string cacheDir = getTextureCacheDir();
    FileSystem::createDirectory(OS::getScratchFolderLocation() + "cache\\");
    FileSystem::createDirectory(cacheDir);

    const std::string completeFile = cacheDir + ".complete";
    if (FileSystem::fileExists(completeFile)) {
        return;
    }

    bool cachingStateSet = false;
    auto ensureCachingState = [&]() {
        if (!cachingStateSet) {
            Unzip::setLoadingState(TranslationManager::getTranslation("ui.loading.caching"));
            cachingStateSet = true;
        }
    };

    bool allSucceeded = true;

    auto precacheCostume = [&](const Costume &costume) {
        if (costume.isSVG) return;
        const std::string &name = costume.fullName;
        if (name.empty()) return;

        std::string cachePath = getTextureCachePath(name);
        if (cachePath.empty() || FileSystem::fileExists(cachePath)) {
            return;
        }

        size_t fileSize = 0;
        void *fileData = extractProjectAsset(name, fileSize);
        if (!fileData || fileSize == 0) return;

        int rawW = 0, rawH = 0, channels = 0;
        unsigned char *pixels = stbi_load_from_memory(static_cast<const unsigned char *>(fileData), fileSize, &rawW, &rawH, &channels, 4);
        freeProjectAsset(fileData);

        if (!pixels || rawW <= 0 || rawH <= 0) {
            if (pixels) stbi_image_free(pixels);
            return;
        }

        int curW = rawW;
        int curH = rawH;
        unsigned char *curPixels = pixels;

        // Downscale if bitmapHalfQuality and resolution == 2 (matching Image::loadRasterFromMemory)
        if (Scratch::bitmapHalfQuality && costume.bitmapResolution == 2) {
            int dw, dh;
            unsigned char *downscaled = Image::resizeRaster(curPixels, curW, curH, dw, dh);
            if (downscaled) {
                if (curPixels == pixels) stbi_image_free(curPixels);
                else free(curPixels);
                curPixels = downscaled;
                curW = dw;
                curH = dh;
            }
        }

        // Downscale if > 2048 in either dimension
        while (curW > 2048 || curH > 2048) {
            int dw, dh;
            unsigned char *downscaled = Image::resizeRaster(curPixels, curW, curH, dw, dh);
            if (!downscaled) break;
            if (curPixels == pixels) stbi_image_free(curPixels);
            else free(curPixels);
            curPixels = downscaled;
            curW = dw;
            curH = dh;
        }

        // Verify opacity - only opaque textures qualify for DXT1
        bool isOpaque = true;
        const int pitch = curW * 4;
        for (int y = 0; y < curH; y++) {
            const uint8_t *row = curPixels + y * pitch;
            for (int x = 0; x < curW; x++) {
                if (row[x * 4 + 3] != 255) {
                    isOpaque = false;
                    break;
                }
            }
            if (!isOpaque) break;
        }

        if (isOpaque) {
            ensureCachingState();
            if (!writeCompressedTextureCache(cachePath, curW, curH, curPixels, pitch)) {
                allSucceeded = false;
            }
        }

        if (curPixels == pixels) stbi_image_free(curPixels);
        else free(curPixels);
    };

    // 1. Stage backdrops (critical for memory and background rendering)
    if (Scratch::stageSprite) {
        for (const auto &costume : Scratch::stageSprite->costumes) {
            precacheCostume(costume);
        }
    }

    // 2. Sprite costumes
    for (auto *sprite : Scratch::sprites) {
        if (sprite == Scratch::stageSprite) continue;
        for (const auto &costume : sprite->costumes) {
            precacheCostume(costume);
        }
    }

    if (allSucceeded) {
        FILE *f = fopen(completeFile.c_str(), "wb");
        if (f) fclose(f);
    }
}

nonstd::expected<void, std::string> Image_Xbox::refreshTexture() {
    destroyTiles();
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (!cachedAssetPath.empty()) {
        std::string cachePath = getTextureCachePath(cachedAssetPath);
        if (!cachePath.empty() && FileSystem::fileExists(cachePath)) {
            if (loadFromCache(cachePath)) {
                return {};
            }
        }
    }
    return setInitialTexture();
}

Image_Xbox::Image_Xbox(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
    sdlRenderer = static_cast<Renderer_Xbox *>(Render::getGlobalRenderer())->Sdlrenderer;

    const unsigned int maxTextureSizeSquare = SDL_GetNumberProperty(SDL_GetRendererProperties(sdlRenderer), SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0);
    maxTextureSize = {maxTextureSizeSquare, maxTextureSizeSquare};

    cachedAssetPath = filePath;
    std::string cachePath = getTextureCachePath(filePath);
    if (!cachePath.empty() && FileSystem::fileExists(cachePath)) {
        if (loadFromCache(cachePath)) {
            return;
        }
    }

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

    if (fromScratchProject) {
        cachedAssetPath = filePath;
        std::string cachePath = getTextureCachePath(filePath);
        if (!cachePath.empty() && FileSystem::fileExists(cachePath)) {
            if (loadFromCache(cachePath)) {
                return;
            }
        }
    }

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
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}
