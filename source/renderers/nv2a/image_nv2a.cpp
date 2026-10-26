#include "image_nv2a.hpp"
#include "nonstd/expected.hpp"
#include "renderer_nv2a.hpp"
#include "swizzle.h"
#include "xgu.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem.hpp>
#include <image.hpp>
#include <log.hpp>
#include <math.hpp>
#include <os.hpp>
#include <runtime.hpp>
#include <stb_image.h>
#include <string>
#include <translation.hpp>
#include <unzip.hpp>
#include <vector>

#ifdef __XBOX__
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#endif

constexpr uint32_t XBT_VERSION = 6;
static constexpr int TILE_SIZE = 256;
static constexpr int TILE_THRESHOLD = 512;
static constexpr int FORCE_DOWNSCALE_DIM = 1024;
static constexpr int CACHE_MIN_ALPHA_PIXELS = 256 * 256;

#pragma pack(push, 1)
struct XBTHeader {
    char magic[4];     // "XBT1"
    uint32_t version;  // XBT_VERSION
    uint32_t width;    // original full width
    uint32_t height;   // original full height
    uint32_t isTiled;  // 1 if tiled, 0 if single texture
    uint32_t numTiles; // number of tiles
    uint32_t flags;    // bit0: image has alpha
    uint32_t maskSize; // bytes of 1-bit alpha mask following header
    float scale;       // logical scale multiplier
};

struct XBTTileHeader {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    uint32_t dataSize;
    uint32_t codec;    // 1 = DXT1, 2 = DXT5
};
#pragma pack(pop)

enum : uint32_t {
    CODEC_DXT1 = 1,
    CODEC_DXT5 = 2
};

static size_t getAvailablePhysicalMemory() {
#ifdef __XBOX__
    MM_STATISTICS stats;
    memset(&stats, 0, sizeof(stats));
    stats.Length = sizeof(stats);
    if (NT_SUCCESS(MmQueryStatistics(&stats))) {
        return static_cast<size_t>(stats.AvailablePages) * 4096;
    }
#endif
    return 64 * 1024 * 1024;
}

#define STB_DXT_IMPLEMENTATION
#define STB_DXT_STATIC
#include <stb_dxt.h>

static inline int toPowerOfTwo(int v) {
    if (v <= 1) return 1;
    return 1 << (32 - __builtin_clz(v - 1));
}

static inline void unpackRGB565(uint16_t c, int &r, int &g, int &b) {
    r = (c >> 11) & 31;
    g = (c >> 5) & 63;
    b = c & 31;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
}

static void compressDXT1BlockFast(uint8_t *dst_block, const uint8_t *block, bool isOpaque, bool highQuality) {
    int mode = highQuality ? STB_DXT_HIGHQUAL : STB_DXT_NORMAL;
    if (isOpaque) {
        stb_compress_dxt_block(dst_block, block, 0, mode);
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
    stb_compress_dxt_block(tmp, fixedBlock, 0, mode);
    const uint16_t e0 = static_cast<uint16_t>(tmp[0] | (tmp[1] << 8));
    const uint16_t e1 = static_cast<uint16_t>(tmp[2] | (tmp[3] << 8));
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

static inline int dxtBlockBytes(uint32_t codec) { return codec == CODEC_DXT5 ? 16 : 8; }
static inline int dxtPitch(int w, uint32_t codec) { return ((toPowerOfTwo(w) + 3) / 4) * dxtBlockBytes(codec); }

static std::vector<uint8_t> encodeRegionDXT(const uint8_t *src, int pitch, int imgW, int imgH,
                                            int tx, int ty, int tw, int th,
                                            uint32_t codec, bool isOpaque, bool highQuality = false) {
    const int blocks_x = (toPowerOfTwo(tw) + 3) / 4;
    const int blocks_y = (toPowerOfTwo(th) + 3) / 4;
    const int bytes = dxtBlockBytes(codec);
    std::vector<uint8_t> out(static_cast<size_t>(blocks_x) * blocks_y * bytes);
    const int max_sx = std::min(tx + tw - 1, imgW - 1);
    const int max_sy = std::min(ty + th - 1, imgH - 1);

    int mode = highQuality ? STB_DXT_HIGHQUAL : STB_DXT_NORMAL;

    uint8_t block[64];
    for (int by = 0; by < blocks_y; by++) {
        for (int bx = 0; bx < blocks_x; bx++) {
            for (int py = 0; py < 4; py++) {
                const int sy = std::min(ty + by * 4 + py, max_sy);
                const uint8_t *row = src + sy * pitch;
                for (int px = 0; px < 4; px++) {
                    const int sx = std::min(tx + bx * 4 + px, max_sx);
                    const uint8_t *pix = row + sx * 4;
                    uint8_t *d = block + (py * 4 + px) * 4;
                    d[0] = pix[0];
                    d[1] = pix[1];
                    d[2] = pix[2];
                    d[3] = (codec == CODEC_DXT5) ? pix[3] : (isOpaque ? 255 : (pix[3] >= 128 ? 255 : 0));
                }
            }
            uint8_t *dst = out.data() + (by * blocks_x + bx) * bytes;
            if (codec == CODEC_DXT5) stb_compress_dxt_block(dst, block, 1, mode);
            else compressDXT1BlockFast(dst, block, isOpaque, highQuality);
        }
    }
    return out;
}

static NV2ATexture *createDXTTexture(Renderer_NV2A *r, int w, int h, uint32_t codec, const uint8_t *data) {
    uint32_t fmt = (codec == CODEC_DXT5) ? XGU_TEXTURE_FORMAT_DXT5 : XGU_TEXTURE_FORMAT_DXT1;
    NV2ATexture *t = r->createTexture(w, h, fmt, false);
    if (!t) {
        Scratch::freeUnusedCostumeImages();
        t = r->createTexture(w, h, fmt, false);
    }
    if (!t) return nullptr;

    const int rowBytes = dxtPitch(w, codec);
    const int blockRows = (toPowerOfTwo(h) + 3) / 4;
    for (int row = 0; row < blockRows; ++row) {
        memcpy(t->data + static_cast<size_t>(row) * t->pitch, data + static_cast<size_t>(row) * rowBytes, rowBytes);
    }
    __asm__ __volatile__ ("sfence" ::: "memory");
    return t;
}

static bool g_cacheLoadOOM = false;

void Image_NV2A::destroyTiles() {
    if (!nv2aRenderer && Render::getGlobalRenderer()) {
        nv2aRenderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    }
    for (auto &tile : tiles) {
        if (nv2aRenderer) nv2aRenderer->destroyTexture(tile.texture);
    }
    tiles.clear();
}

nonstd::expected<void, std::string> Image_NV2A::buildTiles() {
    destroyTiles();
    if (!nv2aRenderer && Render::getGlobalRenderer()) {
        nv2aRenderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    }
    if (!nv2aRenderer) return nonstd::make_unexpected("No NV2A renderer");

    while (imgData.width > FORCE_DOWNSCALE_DIM || imgData.height > FORCE_DOWNSCALE_DIM) {
        int dw, dh;
        unsigned char *downscaled = Image::resizeRaster(static_cast<unsigned char *>(imgData.pixels),
                                                        imgData.width, imgData.height, dw, dh);
        if (!downscaled) break;
        imgData.scale *= static_cast<float>(dw) / static_cast<float>(imgData.width);
        free(imgData.pixels);
        imgData.pixels = downscaled;
        imgData.width = dw;
        imgData.height = dh;
        imgData.pitch = dw * 4;
    }

    const int tileSize = TILE_SIZE;
    const uint8_t *src = static_cast<const uint8_t *>(imgData.pixels);

    for (int ty = 0; ty < imgData.height; ty += tileSize) {
        for (int tx = 0; tx < imgData.width; tx += tileSize) {
            Tile tile;
            tile.x = tx;
            tile.y = ty;
            tile.w = std::min(tileSize, imgData.width - tx);
            tile.h = std::min(tileSize, imgData.height - ty);

            bool isOpaqueTile = true;
            bool isBinaryAlpha = true;
            for (int y = 0; y < tile.h; y++) {
                const uint8_t *row = src + (ty + y) * imgData.pitch + tx * 4;
                for (int x = 0; x < tile.w; x++) {
                    const uint8_t a = row[x * 4 + 3];
                    if (a != 255) {
                        isOpaqueTile = false;
                        if (a != 0) isBinaryAlpha = false;
                    }
                }
            }

            const uint32_t codec = (isOpaqueTile || isBinaryAlpha) ? CODEC_DXT1 : CODEC_DXT5;
            const auto data = encodeRegionDXT(src, imgData.pitch, imgData.width, imgData.height,
                                              tx, ty, tile.w, tile.h, codec, isOpaqueTile, false);
            tile.texture = createDXTTexture(nv2aRenderer, tile.w, tile.h, codec, data.data());
            if (!tile.texture) {
                Scratch::freeUnusedCostumeImages();
                tile.texture = createDXTTexture(nv2aRenderer, tile.w, tile.h, codec, data.data());
            }
            if (!tile.texture) {
                // Fallback to 16-bit without dither noise
                uint32_t fmt = isOpaqueTile ? XGU_TEXTURE_FORMAT_R5G6B5_SWIZZLED :
                               (isBinaryAlpha ? XGU_TEXTURE_FORMAT_A1R5G5B5_SWIZZLED : XGU_TEXTURE_FORMAT_A4R4G4B4_SWIZZLED);
                tile.texture = nv2aRenderer->createTexture(tile.w, tile.h, fmt, true);
                if (tile.texture) {
                    std::vector<uint16_t> scratch(static_cast<size_t>(tile.w) * tile.h);
                    for (int y = 0; y < tile.h; y++) {
                        const uint8_t *row = src + (ty + y) * imgData.pitch + tx * 4;
                        uint16_t *out = scratch.data() + y * tile.w;
                        for (int x = 0; x < tile.w; x++) {
                            if (isOpaqueTile) {
                                int r = row[x * 4 + 0] >> 3;
                                int g = row[x * 4 + 1] >> 2;
                                int b = row[x * 4 + 2] >> 3;
                                out[x] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
                            } else if (isBinaryAlpha) {
                                int a = (row[x * 4 + 3] >= 128) ? 1 : 0;
                                int r = row[x * 4 + 0] >> 3;
                                int g = row[x * 4 + 1] >> 3;
                                int b = row[x * 4 + 2] >> 3;
                                out[x] = static_cast<uint16_t>((a << 15) | (r << 10) | (g << 5) | b);
                            } else {
                                int a = row[x * 4 + 3] >> 4;
                                int r = row[x * 4 + 0] >> 4;
                                int g = row[x * 4 + 1] >> 4;
                                int b = row[x * 4 + 2] >> 4;
                                out[x] = static_cast<uint16_t>((a << 12) | (r << 8) | (g << 4) | b);
                            }
                        }
                    }
                    nv2aRenderer->updateTexture(tile.texture, scratch.data(), tile.w * 2);
                }
            }

            if (!tile.texture) {
                destroyTiles();
                return nonstd::make_unexpected("Failed to allocate texture tile");
            }

            tiles.push_back(tile);
        }
    }
    return {};
}

void Image_NV2A::renderTiled(ImageRenderParams &params) {
    if (tiles.empty() || !nv2aRenderer) return;

    const float x = params.x;
    const float y = params.y;
    const int brightness = params.brightness;
    const double rotationRad = params.rotation;
    const float scale = params.scale;
    const float opacity = params.opacity;

    if (opacity <= 0.001f) return;
    const uint8_t alpha = static_cast<uint8_t>(opacity * 255);
    if (alpha == 0) return;

    float renderW = (imgData.width / imgData.scale) * scale;
    float renderH = (imgData.height / imgData.scale) * scale;
    float renderX = params.centered ? (x - renderW / 2.0f) : x;
    float renderY = params.centered ? (y - renderH / 2.0f) : y;

    float subX = 0.0f, subY = 0.0f;
    float subW = static_cast<float>(imgData.width), subH = static_cast<float>(imgData.height);
    if (params.subrect != nullptr) {
        subX = static_cast<float>(params.subrect->x);
        subY = static_cast<float>(params.subrect->y);
        subW = static_cast<float>(params.subrect->w);
        subH = static_cast<float>(params.subrect->h);
    }
    if (subW <= 0 || subH <= 0) return;

    const float rx = renderW / subW;
    const float ry = renderH / subH;
    const float centerX = renderX + renderW / 2.0f;
    const float centerY = renderY + renderH / 2.0f;

    const float cosA = static_cast<float>(std::cos(rotationRad));
    const float sinA = static_cast<float>(std::sin(rotationRad));

    auto rotatePoint = [&](float px, float py) -> std::pair<float, float> {
        float dx = px - centerX;
        float dy = py - centerY;
        return {centerX + dx * cosA - dy * sinA, centerY + dx * sinA + dy * cosA};
    };

    auto drawPass = [&](uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca, NV2ABlendMode blendMode) {
        nv2aRenderer->setBlendMode(blendMode);
        for (const auto &tile : tiles) {
            const float ix0 = std::max(static_cast<float>(tile.x), subX);
            const float iy0 = std::max(static_cast<float>(tile.y), subY);
            const float ix1 = std::min(static_cast<float>(tile.x + tile.w), subX + subW);
            const float iy1 = std::min(static_cast<float>(tile.y + tile.h), subY + subH);
            if (ix1 <= ix0 || iy1 <= iy0) continue;

            float dx0 = (ix0 - subX) * rx;
            float dx1 = (ix1 - subX) * rx;
            float dy0 = (iy0 - subY) * ry;
            float dy1 = (iy1 - subY) * ry;

            if (params.flip) {
                float tmp = renderW - dx1;
                dx1 = renderW - dx0;
                dx0 = tmp;
            }

            float px0 = renderX + dx0, py0 = renderY + dy0;
            float px1 = renderX + dx1, py1 = renderY + dy1;

            auto p_tl = rotatePoint(px0, py0);
            auto p_tr = rotatePoint(px1, py0);
            auto p_br = rotatePoint(px1, py1);
            auto p_bl = rotatePoint(px0, py1);

            float u0 = (ix0 - tile.x) / static_cast<float>(tile.w) * tile.texture->uScale;
            float v0 = (iy0 - tile.y) / static_cast<float>(tile.h) * tile.texture->vScale;
            float u1 = (ix1 - tile.x) / static_cast<float>(tile.w) * tile.texture->uScale;
            float v1 = (iy1 - tile.y) / static_cast<float>(tile.h) * tile.texture->vScale;

            if (params.flip) std::swap(u0, u1);

            NV2AVertex quad[6] = {
                {{p_tl.first, p_tl.second}, {cr, cg, cb, ca}, {u0, v0}},
                {{p_tr.first, p_tr.second}, {cr, cg, cb, ca}, {u1, v0}},
                {{p_br.first, p_br.second}, {cr, cg, cb, ca}, {u1, v1}},
                {{p_tl.first, p_tl.second}, {cr, cg, cb, ca}, {u0, v0}},
                {{p_br.first, p_br.second}, {cr, cg, cb, ca}, {u1, v1}},
                {{p_bl.first, p_bl.second}, {cr, cg, cb, ca}, {u0, v1}}
            };

            nv2aRenderer->bindTexture(tile.texture, true);
            nv2aRenderer->drawTriangles(quad, 6);
        }
    };

    if (brightness > 0) {
        float b = brightness * 0.01f;
        drawPass(255, 255, 255, alpha, NV2ABlendMode::Blend);
        uint8_t addA = static_cast<uint8_t>(255.0f * b * opacity);
        if (addA > 0) {
            drawPass(255, 255, 255, addA, NV2ABlendMode::Add);
        }
    } else if (brightness < 0) {
        float b = brightness * 0.01f;
        uint8_t dim = static_cast<uint8_t>(255.0f * std::clamp(1.0f + b, 0.0f, 1.0f));
        drawPass(dim, dim, dim, alpha, NV2ABlendMode::Blend);
    } else {
        drawPass(255, 255, 255, alpha, NV2ABlendMode::Blend);
    }
}

static void *extractProjectAsset(const std::string &assetName, size_t &outSize);
static void freeProjectAsset(void *ptr);

static inline void rgb2hsv(float r, float g, float b, float &h, float &s, float &v) {
    float maxC = std::max({r, g, b});
    float minC = std::min({r, g, b});
    float d = maxC - minC;
    v = maxC;
    s = (maxC > 1e-6f) ? (d / maxC) : 0.0f;
    if (d < 1e-6f) {
        h = 0.0f;
    } else if (maxC == r) {
        float seg = (g - b) / d;
        h = (seg < 0.0f ? seg + 6.0f : seg) / 6.0f;
    } else if (maxC == g) {
        h = ((b - r) / d + 2.0f) / 6.0f;
    } else {
        h = ((r - g) / d + 4.0f) / 6.0f;
    }
}

static inline void hsv2rgb(float h, float s, float v, float &r, float &g, float &b) {
    if (s <= 1e-6f) {
        r = g = b = v;
        return;
    }
    h = h - std::floor(h);
    float h6 = h * 6.0f;
    int i = static_cast<int>(h6);
    float f = h6 - static_cast<float>(i);
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}

static void decodeDXT1Block(const uint8_t *block, uint8_t *dstRGBA, int dstPitch, int maxW, int maxH) {
    uint16_t c0 = static_cast<uint16_t>(block[0] | (block[1] << 8));
    uint16_t c1 = static_cast<uint16_t>(block[2] | (block[3] << 8));
    int r0, g0, b0, r1, g1, b1;
    unpackRGB565(c0, r0, g0, b0);
    unpackRGB565(c1, r1, g1, b1);
    uint8_t pal[4][4] = {
        {static_cast<uint8_t>(r0), static_cast<uint8_t>(g0), static_cast<uint8_t>(b0), 255},
        {static_cast<uint8_t>(r1), static_cast<uint8_t>(g1), static_cast<uint8_t>(b1), 255},
        {0, 0, 0, 255},
        {0, 0, 0, 255}
    };
    if (c0 > c1) {
        pal[2][0] = static_cast<uint8_t>((2 * r0 + r1) / 3);
        pal[2][1] = static_cast<uint8_t>((2 * g0 + g1) / 3);
        pal[2][2] = static_cast<uint8_t>((2 * b0 + b1) / 3);
        pal[3][0] = static_cast<uint8_t>((r0 + 2 * r1) / 3);
        pal[3][1] = static_cast<uint8_t>((g0 + 2 * r1) / 3);
        pal[3][2] = static_cast<uint8_t>((b0 + 2 * b1) / 3);
    } else {
        pal[2][0] = static_cast<uint8_t>((r0 + r1) / 2);
        pal[2][1] = static_cast<uint8_t>((g0 + g1) / 2);
        pal[2][2] = static_cast<uint8_t>((b0 + b1) / 2);
        pal[3][0] = 0; pal[3][1] = 0; pal[3][2] = 0; pal[3][3] = 0;
    }
    uint32_t indices = static_cast<uint32_t>(block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24));
    for (int y = 0; y < 4 && y < maxH; ++y) {
        uint8_t *row = dstRGBA + y * dstPitch;
        for (int x = 0; x < 4 && x < maxW; ++x) {
            int idx = (indices >> ((y * 4 + x) * 2)) & 3;
            memcpy(row + x * 4, pal[idx], 4);
        }
    }
}

static void decodeDXT5Block(const uint8_t *block, uint8_t *dstRGBA, int dstPitch, int maxW, int maxH) {
    uint8_t a0 = block[0];
    uint8_t a1 = block[1];
    uint8_t aPal[8];
    aPal[0] = a0;
    aPal[1] = a1;
    if (a0 > a1) {
        for (int i = 1; i <= 6; ++i) {
            aPal[1 + i] = static_cast<uint8_t>(((7 - i) * a0 + i * a1) / 7);
        }
    } else {
        for (int i = 1; i <= 4; ++i) {
            aPal[1 + i] = static_cast<uint8_t>(((5 - i) * a0 + i * a1) / 5);
        }
        aPal[6] = 0;
        aPal[7] = 255;
    }
    uint64_t aIndices = 0;
    for (int i = 0; i < 6; ++i) {
        aIndices |= static_cast<uint64_t>(block[2 + i]) << (i * 8);
    }

    decodeDXT1Block(block + 8, dstRGBA, dstPitch, maxW, maxH);
    for (int y = 0; y < 4 && y < maxH; ++y) {
        uint8_t *row = dstRGBA + y * dstPitch;
        for (int x = 0; x < 4 && x < maxW; ++x) {
            int shift = (y * 4 + x) * 3;
            int aIdx = static_cast<int>((aIndices >> shift) & 7);
            row[x * 4 + 3] = aPal[aIdx];
        }
    }
}

void Image_NV2A::releaseEffects() {
    if (!nv2aRenderer && Render::getGlobalRenderer()) {
        nv2aRenderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    }
    for (int i = 0; i < 2; ++i) {
        if (effectSlots[i].texture) {
            if (nv2aRenderer) {
                nv2aRenderer->destroyTexture(effectSlots[i].texture);
            }
            effectSlots[i].texture = nullptr;
        }
        effectSlots[i].color = 0.0f;
        effectSlots[i].fisheye = 0.0f;
        effectSlots[i].whirl = 0.0f;
        effectSlots[i].pixelate = 0.0f;
        effectSlots[i].mosaic = 0.0f;
        effectSlots[i].lastUsedFrame = 0;
    }
    effectSource.clear();
    effectSource.shrink_to_fit();
}

const uint8_t *Image_NV2A::getEffectSource(int &outPitch) {
    if (imgData.pixels) {
        outPitch = imgData.pitch;
        return static_cast<const uint8_t *>(imgData.pixels);
    }
    if (!effectSource.empty()) {
        outPitch = imgData.width * 4;
        return effectSource.data();
    }
    if (!cachedAssetPath.empty()) {
        size_t fileSize = 0;
        void *fileData = extractProjectAsset(cachedAssetPath, fileSize);
        if (fileData && fileSize > 0) {
            int rawW = 0, rawH = 0, channels = 0;
            unsigned char *pix = stbi_load_from_memory(static_cast<const unsigned char *>(fileData), fileSize, &rawW, &rawH, &channels, 4);
            freeProjectAsset(fileData);
            if (pix && rawW > 0 && rawH > 0) {
                int curW = rawW, curH = rawH;
                unsigned char *curPixels = pix;
                while (curW > imgData.width * 2 && curH > imgData.height * 2) {
                    int dw, dh;
                    unsigned char *downscaled = Image::resizeRaster(curPixels, curW, curH, dw, dh);
                    if (!downscaled) break;
                    if (curPixels == pix) stbi_image_free(curPixels);
                    else free(curPixels);
                    curPixels = downscaled;
                    curW = dw;
                    curH = dh;
                }
                if (curW == imgData.width && curH == imgData.height) {
                    effectSource.assign(curPixels, curPixels + curW * curH * 4);
                } else {
                    effectSource.resize(static_cast<size_t>(imgData.width) * imgData.height * 4);
                    for (int y = 0; y < imgData.height; ++y) {
                        int sy = (y * curH) / imgData.height;
                        const uint8_t *sRow = curPixels + sy * curW * 4;
                        uint8_t *dRow = effectSource.data() + y * imgData.width * 4;
                        for (int x = 0; x < imgData.width; ++x) {
                            int sx = (x * curW) / imgData.width;
                            memcpy(dRow + x * 4, sRow + sx * 4, 4);
                        }
                    }
                }
                if (curPixels == pix) stbi_image_free(curPixels);
                else free(curPixels);
            }
        }
    }
    if (effectSource.empty() && texture && texture->data && texture->isDXT) {
        effectSource.assign(static_cast<size_t>(imgData.width) * imgData.height * 4, 0);
        int blockRows = (imgData.height + 3) / 4;
        int blockCols = (imgData.width + 3) / 4;
        bool isDXT5 = (texture->format == XGU_TEXTURE_FORMAT_DXT5);
        int blockBytes = isDXT5 ? 16 : 8;
        for (int by = 0; by < blockRows; ++by) {
            for (int bx = 0; bx < blockCols; ++bx) {
                const uint8_t *block = texture->data + (by * texture->pitch) + (bx * blockBytes);
                uint8_t *dst = effectSource.data() + (by * 4 * imgData.width * 4) + (bx * 4 * 4);
                int remW = imgData.width - bx * 4;
                int remH = imgData.height - by * 4;
                if (isDXT5) decodeDXT5Block(block, dst, imgData.width * 4, remW, remH);
                else decodeDXT1Block(block, dst, imgData.width * 4, remW, remH);
            }
        }
    }
    if (effectSource.empty() && !tiles.empty()) {
        effectSource.assign(static_cast<size_t>(imgData.width) * imgData.height * 4, 0);
        for (const auto &tile : tiles) {
            if (tile.texture && tile.texture->data && tile.texture->isDXT) {
                int blockRows = (tile.h + 3) / 4;
                int blockCols = (tile.w + 3) / 4;
                bool isDXT5 = (tile.texture->format == XGU_TEXTURE_FORMAT_DXT5);
                int blockBytes = isDXT5 ? 16 : 8;
                for (int by = 0; by < blockRows; ++by) {
                    for (int bx = 0; bx < blockCols; ++bx) {
                        const uint8_t *block = tile.texture->data + (by * tile.texture->pitch) + (bx * blockBytes);
                        uint8_t *dst = effectSource.data() + ((tile.y + by * 4) * imgData.width * 4) + ((tile.x + bx * 4) * 4);
                        int remW = tile.w - bx * 4;
                        int remH = tile.h - by * 4;
                        if (isDXT5) decodeDXT5Block(block, dst, imgData.width * 4, remW, remH);
                        else decodeDXT1Block(block, dst, imgData.width * 4, remW, remH);
                    }
                }
            }
        }
    }
    if (!effectSource.empty()) {
        outPitch = imgData.width * 4;
        return effectSource.data();
    }
    return nullptr;
}

NV2ATexture *Image_NV2A::acquireEffectTexture(const ImageRenderParams &params) {
    if (imgData.width <= 0 || imgData.height <= 0) return nullptr;
    if (!nv2aRenderer && Render::getGlobalRenderer()) {
        nv2aRenderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    }
    if (!nv2aRenderer) return nullptr;

    const uint32_t currentFrame = Renderer_NV2A::getFrameCounter();

    for (int i = 0; i < 2; ++i) {
        if (effectSlots[i].texture && effectSlots[i].matches(params)) {
            effectSlots[i].lastUsedFrame = currentFrame;
            return effectSlots[i].texture;
        }
    }

    int chosenSlot = 0;
    if (effectSlots[0].lastUsedFrame != currentFrame) {
        chosenSlot = 0;
    } else if (effectSlots[1].lastUsedFrame != currentFrame) {
        chosenSlot = 1;
    } else {
        chosenSlot = (effectSlots[0].lastUsedFrame <= effectSlots[1].lastUsedFrame) ? 0 : 1;
    }

    EffectCacheSlot &slot = effectSlots[chosenSlot];

    int srcPitch = 0;
    const uint8_t *src = getEffectSource(srcPitch);
    if (!src) return texture;

    int srcW = imgData.width;
    int srcH = imgData.height;
    int effW = std::min(srcW, 512);
    int effH = std::min(srcH, 512);
    if (effW <= 0) effW = 1;
    if (effH <= 0) effH = 1;

    if (!slot.texture || slot.texture->texWidth != effW || slot.texture->texHeight != effH) {
        if (slot.texture) {
            nv2aRenderer->destroyTexture(slot.texture);
            slot.texture = nullptr;
        }
        slot.texture = nv2aRenderer->createTexture(effW, effH, XGU_TEXTURE_FORMAT_A8B8G8R8_SWIZZLED, true);
        if (!slot.texture) {
            return texture;
        }
    }

    std::vector<uint8_t> scratch(static_cast<size_t>(effW) * effH * 4);

    const float mosaic = params.mosaicEffect;
    const float pixelate = params.pixelateEffect;
    const float whirl = params.whirlEffect;
    const float fisheye = params.fisheyeEffect;
    const float color = params.colorEffect;

    const bool hasMosaic = (std::abs(mosaic) > 0.001f);
    const bool hasPixelate = (std::abs(pixelate) > 0.001f);
    const bool hasWhirl = (std::abs(whirl) > 0.001f);
    const bool hasFisheye = (std::abs(fisheye) > 0.001f);
    const bool hasColor = (std::abs(color) > 0.001f);

    const float tiles = hasMosaic ? std::clamp(std::round((std::abs(mosaic) + 10.0f) / 10.0f + 1e-3f), 1.0f, 512.0f) : 1.0f;
    const float pixelateAmount = std::abs(pixelate) / 10.0f;
    const float pixelTexelSizeX = static_cast<float>(effW) / std::max(pixelateAmount, 1e-3f);
    const float pixelTexelSizeY = static_cast<float>(effH) / std::max(pixelateAmount, 1e-3f);
    const float whirlRad = whirl * 3.141592653589793f / 180.0f;
    const float fisheyeAmount = std::max(0.0f, (fisheye + 100.0f) / 100.0f);
    const float colorShift = color / 200.0f;

    const int dstPitch = effW * 4;

    if (hasColor && !hasMosaic && !hasPixelate && !hasWhirl && !hasFisheye && effW == srcW && effH == srcH) {
        for (int y = 0; y < effH; ++y) {
            const uint8_t *sRow = src + y * srcPitch;
            uint8_t *dRow = scratch.data() + y * dstPitch;
            for (int x = 0; x < effW; ++x) {
                uint8_t a = sRow[x * 4 + 3];
                if (a == 0) {
                    *reinterpret_cast<uint32_t *>(dRow + x * 4) = 0;
                    continue;
                }
                float r = sRow[x * 4 + 0] * (1.0f / 255.0f);
                float g = sRow[x * 4 + 1] * (1.0f / 255.0f);
                float b = sRow[x * 4 + 2] * (1.0f / 255.0f);
                float h, s, v;
                rgb2hsv(r, g, b, h, s, v);
                const float minLightness = 0.11f / 2.0f;
                const float minSaturation = 0.09f;
                if (v < minLightness) { h = 0.0f; s = 1.0f; v = minLightness; }
                else if (s < minSaturation) { h = 0.0f; s = minSaturation; }
                h = h + colorShift;
                h -= std::floor(h);
                float ro, go, bo;
                hsv2rgb(h, s, v, ro, go, bo);
                dRow[x * 4 + 0] = static_cast<uint8_t>(std::clamp(ro * 255.0f + 0.5f, 0.0f, 255.0f));
                dRow[x * 4 + 1] = static_cast<uint8_t>(std::clamp(go * 255.0f + 0.5f, 0.0f, 255.0f));
                dRow[x * 4 + 2] = static_cast<uint8_t>(std::clamp(bo * 255.0f + 0.5f, 0.0f, 255.0f));
                dRow[x * 4 + 3] = a;
            }
        }
    } else {
        for (int y = 0; y < effH; ++y) {
            float vNorm = (static_cast<float>(y) + 0.5f) / static_cast<float>(effH);
            uint8_t *dRow = scratch.data() + y * dstPitch;
            for (int x = 0; x < effW; ++x) {
                float uNorm = (static_cast<float>(x) + 0.5f) / static_cast<float>(effW);
                float u = uNorm;
                float v = vNorm;

                if (hasMosaic) {
                    u = u * tiles;
                    u -= std::floor(u);
                    v = v * tiles;
                    v -= std::floor(v);
                }

                if (hasPixelate) {
                    u = (std::floor(u * pixelTexelSizeX) + 0.5f) / pixelTexelSizeX;
                    v = (std::floor(v * pixelTexelSizeY) + 0.5f) / pixelTexelSizeY;
                }

                float cx = u - 0.5f;
                float cy = v - 0.5f;

                if (hasWhirl) {
                    float dist = std::sqrt(cx * cx + cy * cy);
                    float whirlFactor = std::max(1.0f - (dist / 0.5f), 0.0f);
                    float whirlActual = whirlRad * whirlFactor * whirlFactor;
                    float s = std::sin(whirlActual);
                    float c = std::cos(whirlActual);
                    float ncx = c * cx - s * cy;
                    float ncy = s * cx + c * cy;
                    cx = ncx;
                    cy = ncy;
                }

                if (hasFisheye) {
                    float vx = cx / 0.5f;
                    float vy = cy / 0.5f;
                    float vecLen = std::sqrt(vx * vx + vy * vy);
                    if (vecLen > 0.0001f) {
                        float r = std::pow(std::min(vecLen, 1.0f), fisheyeAmount) * std::max(1.0f, vecLen);
                        float invLen = 1.0f / vecLen;
                        cx = r * (vx * invLen) * 0.5f;
                        cy = r * (vy * invLen) * 0.5f;
                    }
                }

                u = cx + 0.5f;
                v = cy + 0.5f;

                if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f) {
                    *reinterpret_cast<uint32_t *>(dRow + x * 4) = 0;
                    continue;
                }

                int sx = static_cast<int>(u * srcW);
                int sy = static_cast<int>(v * srcH);
                if (sx < 0) sx = 0; else if (sx >= srcW) sx = srcW - 1;
                if (sy < 0) sy = 0; else if (sy >= srcH) sy = srcH - 1;

                const uint8_t *sPix = src + sy * srcPitch + sx * 4;
                uint8_t a = sPix[3];
                if (a == 0) {
                    *reinterpret_cast<uint32_t *>(dRow + x * 4) = 0;
                    continue;
                }

                if (hasColor) {
                    float r = sPix[0] * (1.0f / 255.0f);
                    float g = sPix[1] * (1.0f / 255.0f);
                    float b = sPix[2] * (1.0f / 255.0f);
                    float h, s, val;
                    rgb2hsv(r, g, b, h, s, val);
                    const float minLightness = 0.11f / 2.0f;
                    const float minSaturation = 0.09f;
                    if (val < minLightness) { h = 0.0f; s = 1.0f; val = minLightness; }
                    else if (s < minSaturation) { h = 0.0f; s = minSaturation; }
                    h = h + colorShift;
                    h -= std::floor(h);
                    float ro, go, bo;
                    hsv2rgb(h, s, val, ro, go, bo);
                    dRow[x * 4 + 0] = static_cast<uint8_t>(std::clamp(ro * 255.0f + 0.5f, 0.0f, 255.0f));
                    dRow[x * 4 + 1] = static_cast<uint8_t>(std::clamp(go * 255.0f + 0.5f, 0.0f, 255.0f));
                    dRow[x * 4 + 2] = static_cast<uint8_t>(std::clamp(bo * 255.0f + 0.5f, 0.0f, 255.0f));
                    dRow[x * 4 + 3] = a;
                } else {
                    memcpy(dRow + x * 4, sPix, 4);
                }
            }
        }
    }

    nv2aRenderer->updateTexture(slot.texture, scratch.data(), dstPitch);

    slot.color = params.colorEffect;
    slot.fisheye = params.fisheyeEffect;
    slot.whirl = params.whirlEffect;
    slot.pixelate = params.pixelateEffect;
    slot.mosaic = params.mosaicEffect;
    slot.lastUsedFrame = currentFrame;

    return slot.texture;
}

void Image_NV2A::render(ImageRenderParams &params) {
    if (!nv2aRenderer && Render::getGlobalRenderer()) {
        nv2aRenderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    }
    if (!nv2aRenderer) return;

    bool hasEffects = (std::abs(params.colorEffect) > 0.001f ||
                       std::abs(params.fisheyeEffect) > 0.001f ||
                       std::abs(params.whirlEffect) > 0.001f ||
                       std::abs(params.pixelateEffect) > 0.001f ||
                       std::abs(params.mosaicEffect) > 0.001f);

    if (!hasEffects) {
        uint32_t currentFrame = Renderer_NV2A::getFrameCounter();
        if ((effectSlots[0].texture && currentFrame - effectSlots[0].lastUsedFrame > 60) ||
            (effectSlots[1].texture && currentFrame - effectSlots[1].lastUsedFrame > 60)) {
            releaseEffects();
        }
        if (!tiles.empty()) {
            renderTiled(params);
            return;
        }
    }

    NV2ATexture *activeTex = texture;
    if (hasEffects) {
        NV2ATexture *effTex = acquireEffectTexture(params);
        if (effTex) {
            activeTex = effTex;
        }
    }
    if (!activeTex) {
        if (!tiles.empty()) {
            renderTiled(params);
        }
        return;
    }

    const float x = params.x;
    const float y = params.y;
    const int brightness = params.brightness;
    const double rotationRad = params.rotation;
    const float scale = params.scale;
    const float opacity = params.opacity;

    if (opacity <= 0.001f) return;
    const uint8_t alpha = static_cast<uint8_t>(opacity * 255);
    if (alpha == 0) return;

    float renderW = (imgData.width / imgData.scale) * scale;
    float renderH = (imgData.height / imgData.scale) * scale;
    float renderX = params.centered ? (x - renderW / 2.0f) : x;
    float renderY = params.centered ? (y - renderH / 2.0f) : y;

    float subX = 0.0f, subY = 0.0f;
    float subW = static_cast<float>(imgData.width), subH = static_cast<float>(imgData.height);
    if (params.subrect != nullptr) {
        subX = static_cast<float>(params.subrect->x);
        subY = static_cast<float>(params.subrect->y);
        subW = static_cast<float>(params.subrect->w);
        subH = static_cast<float>(params.subrect->h);
    }
    if (subW <= 0 || subH <= 0) return;

    float u0 = (subX / static_cast<float>(imgData.width)) * activeTex->uScale;
    float v0 = (subY / static_cast<float>(imgData.height)) * activeTex->vScale;
    float u1 = ((subX + subW) / static_cast<float>(imgData.width)) * activeTex->uScale;
    float v1 = ((subY + subH) / static_cast<float>(imgData.height)) * activeTex->vScale;

    if (params.flip) {
        std::swap(u0, u1);
    }

    const float centerX = renderX + renderW / 2.0f;
    const float centerY = renderY + renderH / 2.0f;
    const float cosA = static_cast<float>(std::cos(rotationRad));
    const float sinA = static_cast<float>(std::sin(rotationRad));

    auto rotatePoint = [&](float px, float py) -> std::pair<float, float> {
        float dx = px - centerX;
        float dy = py - centerY;
        return {centerX + dx * cosA - dy * sinA, centerY + dx * sinA + dy * cosA};
    };

    auto p_tl = rotatePoint(renderX, renderY);
    auto p_tr = rotatePoint(renderX + renderW, renderY);
    auto p_br = rotatePoint(renderX + renderW, renderY + renderH);
    auto p_bl = rotatePoint(renderX, renderY + renderH);

    auto drawQuad = [&](uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca, NV2ABlendMode blendMode) {
        NV2AVertex verts[6] = {
            {{p_tl.first, p_tl.second}, {cr, cg, cb, ca}, {u0, v0}},
            {{p_tr.first, p_tr.second}, {cr, cg, cb, ca}, {u1, v0}},
            {{p_br.first, p_br.second}, {cr, cg, cb, ca}, {u1, v1}},
            {{p_tl.first, p_tl.second}, {cr, cg, cb, ca}, {u0, v0}},
            {{p_br.first, p_br.second}, {cr, cg, cb, ca}, {u1, v1}},
            {{p_bl.first, p_bl.second}, {cr, cg, cb, ca}, {u0, v1}}
        };
        nv2aRenderer->bindTexture(activeTex, true);
        nv2aRenderer->setBlendMode(blendMode);
        nv2aRenderer->drawTriangles(verts, 6);
    };

    if (brightness > 0) {
        float b = brightness * 0.01f;
        drawQuad(255, 255, 255, alpha, NV2ABlendMode::Blend);
        uint8_t addA = static_cast<uint8_t>(255.0f * b * opacity);
        if (addA > 0) {
            drawQuad(255, 255, 255, addA, NV2ABlendMode::Add);
        }
    } else if (brightness < 0) {
        float b = brightness * 0.01f;
        uint8_t dim = static_cast<uint8_t>(255.0f * std::clamp(1.0f + b, 0.0f, 1.0f));
        drawQuad(dim, dim, dim, alpha, NV2ABlendMode::Blend);
    } else {
        drawQuad(255, 255, 255, alpha, NV2ABlendMode::Blend);
    }
}

void Image_NV2A::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    if (!texture) return;
    if (!nv2aRenderer && Render::getGlobalRenderer()) {
        nv2aRenderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    }
    if (!nv2aRenderer) return;

    const float destX = static_cast<float>(xPos - (centered ? width / 2.0f : 0));
    const float destY = static_cast<float>(yPos - (centered ? height / 2.0f : 0));
    const float srcPadding = std::max(1.0f, std::min(std::min(static_cast<float>(padding), getWidth() / 2.0f), static_cast<float>(getHeight()) / 2.0f));

    const float srcCenterWidth = std::max(0.0f, imgData.width - 2 * srcPadding);
    const float srcCenterHeight = std::max(0.0f, imgData.height - 2 * srcPadding);

    const float dstCenterWidth = std::max(0.0f, static_cast<float>(width) - 2 * srcPadding);
    const float dstCenterHeight = std::max(0.0f, static_cast<float>(height) - 2 * srcPadding);

    float sx[4] = {0, srcPadding, srcPadding + srcCenterWidth, static_cast<float>(imgData.width)};
    float sy[4] = {0, srcPadding, srcPadding + srcCenterHeight, static_cast<float>(imgData.height)};

    float dx[4] = {destX, destX + srcPadding, destX + srcPadding + dstCenterWidth, destX + static_cast<float>(width)};
    float dy[4] = {destY, destY + srcPadding, destY + srcPadding + dstCenterHeight, destY + static_cast<float>(height)};

    std::vector<NV2AVertex> verts;
    verts.reserve(9 * 6);

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            float x0 = dx[col], x1 = dx[col + 1];
            float y0 = dy[row], y1 = dy[row + 1];
            if (x1 <= x0 || y1 <= y0) continue;

            float u0 = (sx[col] / static_cast<float>(imgData.width)) * texture->uScale;
            float v0 = (sy[row] / static_cast<float>(imgData.height)) * texture->vScale;
            float u1 = (sx[col + 1] / static_cast<float>(imgData.width)) * texture->uScale;
            float v1 = (sy[row + 1] / static_cast<float>(imgData.height)) * texture->vScale;

            NV2AVertex quad[6] = {
                {{x0, y0}, {255, 255, 255, 255}, {u0, v0}},
                {{x1, y0}, {255, 255, 255, 255}, {u1, v0}},
                {{x1, y1}, {255, 255, 255, 255}, {u1, v1}},
                {{x0, y0}, {255, 255, 255, 255}, {u0, v0}},
                {{x1, y1}, {255, 255, 255, 255}, {u1, v1}},
                {{x0, y1}, {255, 255, 255, 255}, {u0, v1}}
            };
            for (int k = 0; k < 6; ++k) verts.push_back(quad[k]);
        }
    }

    if (!verts.empty()) {
        nv2aRenderer->bindTexture(texture, false);
        nv2aRenderer->setBlendMode(NV2ABlendMode::Blend);
        nv2aRenderer->drawTriangles(verts.data(), verts.size());
    }
}

void *Image_NV2A::getNativeTexture() {
    if (!tiles.empty()) return tiles[0].texture;
    return texture;
}

nonstd::expected<void, std::string> Image_NV2A::setInitialTexture() {
    if (!nv2aRenderer && Render::getGlobalRenderer()) {
        nv2aRenderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    }
    if (!nv2aRenderer) return nonstd::make_unexpected("No NV2A renderer");

    if (imgData.width > TILE_THRESHOLD || imgData.height > TILE_THRESHOLD) {
        return buildTiles();
    }

    bool isOpaqueImg = true;
    bool isBinaryAlpha = true;
    const uint8_t *src = static_cast<const uint8_t *>(imgData.pixels);
    for (int y = 0; y < imgData.height; y++) {
        const uint8_t *row = src + y * imgData.pitch;
        for (int x = 0; x < imgData.width; x++) {
            uint8_t a = row[x * 4 + 3];
            if (a != 255) {
                isOpaqueImg = false;
                if (a != 0) isBinaryAlpha = false;
            }
        }
    }

    // On standard 64MB Xbox hardware, large textures (> 128x128) must use DXT compression
    // to preserve contiguous memory for fonts, tiles, and other textures.
    // Small textures (<= 128x128) or enhanced 128MB consoles with plenty of free RAM prefer 32-bit swizzled.
    const size_t freeMem = getAvailablePhysicalMemory();
    const bool prefer32Bit = (imgData.width * imgData.height <= 128 * 128) ||
                             (OS::isEnhancedPlatform() && freeMem > 20 * 1024 * 1024);

    if (prefer32Bit) {
        texture = nv2aRenderer->createTexture(imgData.width, imgData.height, XGU_TEXTURE_FORMAT_A8B8G8R8_SWIZZLED, true);
        if (texture) {
            nv2aRenderer->updateTexture(texture, imgData.pixels, imgData.pitch);
            return {};
        }
    }

    // Fast DXT compression for runtime on-the-fly
    const uint32_t codec = (isOpaqueImg || isBinaryAlpha) ? CODEC_DXT1 : CODEC_DXT5;
    const auto data = encodeRegionDXT(src, imgData.pitch, imgData.width, imgData.height,
                                      0, 0, imgData.width, imgData.height, codec, isOpaqueImg, false);
    texture = createDXTTexture(nv2aRenderer, imgData.width, imgData.height, codec, data.data());
    if (texture) return {};

    // 16-bit swizzled fallback without dither noise
    uint32_t fmt = isOpaqueImg ? XGU_TEXTURE_FORMAT_R5G6B5_SWIZZLED :
                   (isBinaryAlpha ? XGU_TEXTURE_FORMAT_A1R5G5B5_SWIZZLED : XGU_TEXTURE_FORMAT_A4R4G4B4_SWIZZLED);
    texture = nv2aRenderer->createTexture(imgData.width, imgData.height, fmt, true);
    if (texture) {
        std::vector<uint16_t> scratch(static_cast<size_t>(imgData.width) * imgData.height);
        for (int y = 0; y < imgData.height; y++) {
            const uint8_t *row = src + y * imgData.pitch;
            uint16_t *out = scratch.data() + y * imgData.width;
            for (int x = 0; x < imgData.width; x++) {
                if (isOpaqueImg) {
                    int r = row[x * 4 + 0] >> 3;
                    int g = row[x * 4 + 1] >> 2;
                    int b = row[x * 4 + 2] >> 3;
                    out[x] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
                } else if (isBinaryAlpha) {
                    int a = (row[x * 4 + 3] >= 128) ? 1 : 0;
                    int r = row[x * 4 + 0] >> 3;
                    int g = row[x * 4 + 1] >> 3;
                    int b = row[x * 4 + 2] >> 3;
                    out[x] = static_cast<uint16_t>((a << 15) | (r << 10) | (g << 5) | b);
                } else {
                    int a = row[x * 4 + 3] >> 4;
                    int r = row[x * 4 + 0] >> 4;
                    int g = row[x * 4 + 1] >> 4;
                    int b = row[x * 4 + 2] >> 4;
                    out[x] = static_cast<uint16_t>((a << 12) | (r << 8) | (g << 4) | b);
                }
            }
        }
        nv2aRenderer->updateTexture(texture, scratch.data(), imgData.width * 2);
        return {};
    }

    if (!prefer32Bit) {
        texture = nv2aRenderer->createTexture(imgData.width, imgData.height, XGU_TEXTURE_FORMAT_A8B8G8R8_SWIZZLED, true);
        if (texture) {
            nv2aRenderer->updateTexture(texture, imgData.pixels, imgData.pitch);
            return {};
        }
    }

    if (!texture && (imgData.width > TILE_SIZE || imgData.height > TILE_SIZE)) {
        Scratch::freeUnusedCostumeImages();
        return buildTiles();
    }

    return nonstd::make_unexpected("Failed to create NV2A texture");
}

static std::string getProjectCacheKey() {
    std::string key = Unzip::filePath;
    if (key.empty()) {
        key = "embedded";
    } else {
        size_t lastSlash = key.find_last_of("/\\");
        if (lastSlash != std::string::npos) key = key.substr(lastSlash + 1);
        size_t dot = key.find_last_of('.');
        if (dot != std::string::npos) key = key.substr(0, dot);
    }
    std::string clean;
    for (char c : key) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') clean += c;
    }
    if (clean.empty()) clean = "project";
    if (clean.length() > 24) clean = clean.substr(0, 24);
    return clean;
}

std::string Image_NV2A::getTextureCacheDir() {
    static std::string cachedDir, cachedProjectPath;
    if (cachedDir.empty() || cachedProjectPath != Unzip::filePath) {
        cachedProjectPath = Unzip::filePath;
        std::string ramMode = OS::isEnhancedPlatform() ? "128m" : "64m";
        cachedDir = OS::getScratchFolderLocation() + "cache\\" + getProjectCacheKey() +
                    "_v" + std::to_string(XBT_VERSION) + "_" + ramMode + "\\";
    }
    return cachedDir;
}

std::string Image_NV2A::getTextureCachePath(const std::string &assetName) {
    if (assetName.empty()) return "";
    std::string base = assetName;
    size_t lastSlash = base.find_last_of("/\\");
    if (lastSlash != std::string::npos) base = base.substr(lastSlash + 1);
    size_t lastDot = base.find_last_of('.');
    if (lastDot != std::string::npos) base = base.substr(0, lastDot);
    if (base.empty()) return "";
    if (base.length() > 36) base = base.substr(0, 36);
    return getTextureCacheDir() + base + ".xbt";
}

bool Image_NV2A::loadFromCache(const std::string &cachePath) {
    g_cacheLoadOOM = false;
    FILE *f = fopen(cachePath.c_str(), "rb");
    if (!f) return false;

    XBTHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1 ||
        memcmp(header.magic, "XBT1", 4) != 0 || header.version != XBT_VERSION ||
        header.numTiles == 0) {
        fclose(f);
        return false;
    }

    if (!nv2aRenderer && Render::getGlobalRenderer()) {
        nv2aRenderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    }
    if (!nv2aRenderer) {
        fclose(f);
        return false;
    }

    auto bail = [&]() {
        destroyTiles();
        if (texture) nv2aRenderer->destroyTexture(texture);
        alphaMask.clear();
        alphaMaskPitch = 0;
        fclose(f);
        return false;
    };

    imgData.width = header.width;
    imgData.height = header.height;
    imgData.scale = header.scale > 0.0f ? header.scale : 1.0f;
    imgData.pitch = header.width * 4;
    imgData.pixels = nullptr;
    isOpaque = !(header.flags & 1);

    alphaMask.clear();
    alphaMaskPitch = 0;
    if (header.maskSize > 0) {
        const uint32_t pitchBytes = (header.width + 7) / 8;
        if (header.maskSize != pitchBytes * header.height) return bail();
        alphaMask.resize(header.maskSize);
        if (fread(alphaMask.data(), 1, header.maskSize, f) != header.maskSize) return bail();
        alphaMaskPitch = static_cast<int>(pitchBytes);
    }

    destroyTiles();
    for (uint32_t i = 0; i < header.numTiles; i++) {
        XBTTileHeader th;
        if (fread(&th, sizeof(th), 1, f) != 1 ||
            (th.codec != CODEC_DXT1 && th.codec != CODEC_DXT5) ||
            th.dataSize > 4 * 1024 * 1024) {
            return bail();
        }
        std::vector<uint8_t> data(th.dataSize);
        if (fread(data.data(), 1, th.dataSize, f) != th.dataSize) return bail();

        NV2ATexture *t = createDXTTexture(nv2aRenderer, th.w, th.h, th.codec, data.data());
        if (!t) return bail();

        if (!header.isTiled) {
            texture = t;
            fclose(f);
            return true;
        }
        Tile tile;
        tile.x = th.x;
        tile.y = th.y;
        tile.w = th.w;
        tile.h = th.h;
        tile.texture = t;
        tiles.push_back(tile);
    }
    fclose(f);
    return true;
}

static void *extractProjectAsset(const std::string &assetName, size_t &outSize) {
    if (Unzip::loader) {
        void *data = Unzip::loader->getAsset(assetName, &outSize);
        if (data) return data;
    }
    std::string fullPath;
    if (Unzip::UnpackedInSD) fullPath = Unzip::filePath + assetName;
    else fullPath = OS::getScratchFolderLocation() + "project/" + assetName;

    FILE *f = fopen(fullPath.c_str(), "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        return nullptr;
    }
    void *buf = malloc(sz);
    if (!buf) {
        fclose(f);
        return nullptr;
    }
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

static bool writeCompressedTextureCache(const std::string &cachePath, int width, int height,
                                        const uint8_t *src, int pitch, bool isOpaque, uint32_t codec, float logicalScale) {
    const std::string tempPath = cachePath + ".tmp";
    FILE *f = fopen(tempPath.c_str(), "wb");
    if (!f) return false;
    auto fail = [&]() {
        fclose(f);
        remove(tempPath.c_str());
        return false;
    };

    const bool isTiled = (width > TILE_THRESHOLD || height > TILE_THRESHOLD);
    const int stepX = isTiled ? TILE_SIZE : width;
    const int stepY = isTiled ? TILE_SIZE : height;

    uint32_t numTiles = 0;
    for (int ty = 0; ty < height; ty += stepY)
        for (int tx = 0; tx < width; tx += stepX)
            numTiles++;

    XBTHeader header;
    memcpy(header.magic, "XBT1", 4);
    header.version = XBT_VERSION;
    header.width = width;
    header.height = height;
    header.isTiled = isTiled ? 1 : 0;
    header.numTiles = numTiles;
    header.flags = isOpaque ? 0 : 1;
    header.scale = logicalScale;

    std::vector<uint8_t> alphaMask;
    if (!isOpaque) {
        const int maskPitch = (width + 7) / 8;
        alphaMask.assign(static_cast<size_t>(maskPitch) * height, 0);
        for (int y = 0; y < height; y++) {
            const uint8_t *row = src + static_cast<size_t>(y) * pitch;
            uint8_t *maskRow = alphaMask.data() + static_cast<size_t>(y) * maskPitch;
            for (int x = 0; x < width; x++) {
                if (row[x * 4 + 3] != 0) maskRow[x >> 3] |= static_cast<uint8_t>(1u << (x & 7));
            }
        }
    }
    header.maskSize = static_cast<uint32_t>(alphaMask.size());

    if (fwrite(&header, sizeof(header), 1, f) != 1) return fail();
    if (!alphaMask.empty() && fwrite(alphaMask.data(), 1, alphaMask.size(), f) != alphaMask.size()) return fail();

    for (int ty = 0; ty < height; ty += stepY) {
        for (int tx = 0; tx < width; tx += stepX) {
            const int tw = std::min(stepX, width - tx);
            const int th = std::min(stepY, height - ty);
            const auto data = encodeRegionDXT(src, pitch, width, height, tx, ty, tw, th, codec, isOpaque, true);

            XBTTileHeader tileHeader;
            tileHeader.x = tx;
            tileHeader.y = ty;
            tileHeader.w = tw;
            tileHeader.h = th;
            tileHeader.dataSize = static_cast<uint32_t>(data.size());
            tileHeader.codec = codec;
            if (fwrite(&tileHeader, sizeof(tileHeader), 1, f) != 1 ||
                fwrite(data.data(), 1, data.size(), f) != data.size()) return fail();
        }
    }

    fclose(f);
    remove(cachePath.c_str());
    return rename(tempPath.c_str(), cachePath.c_str()) == 0;
}

// Describes how a cache was built. Stored in .complete: a cache built under different settings is rebuilt.
static std::string textureCacheSignature() {
    return "nv2a_xbt=" + std::to_string(XBT_VERSION) +
           " ram=" + (OS::isEnhancedPlatform() ? "128" : "64") +
           " tile=" + std::to_string(TILE_SIZE) + "/" + std::to_string(TILE_THRESHOLD) +
           " maxdim=" + std::to_string(FORCE_DOWNSCALE_DIM);
}

static std::string readTextFile(const std::string &path) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return "";
    char buf[256];
    const size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    std::string text(buf, n);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
        text.pop_back();
    return text;
}

static bool writeTextFile(const std::string &path, const std::string &text) {
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
    fclose(f);
    return ok;
}

static void clearTextureCacheDir(const std::string &dir) {
    auto files = FileSystem::listDirectory(dir);
    if (!files.has_value()) return;
    for (const std::string &name : files.value()) {
        if (name == "." || name == "..") continue;
        remove((dir + name).c_str());
    }
}

void Image_NV2A::precacheProjectTextures() {
    std::string cacheDir = getTextureCacheDir();
    FileSystem::createDirectory(OS::getScratchFolderLocation() + "cache\\");
    FileSystem::createDirectory(cacheDir);

    const std::string completeFile = cacheDir + ".complete";
    const std::string signature = textureCacheSignature();
    if (FileSystem::fileExists(completeFile)) {
        const std::string builtFor = readTextFile(completeFile);
        if (builtFor == signature) return;

        Log::log("Texture cache was built for different settings; rebuilding. Built: [" + builtFor +
                 "] Current: [" + signature + "]");
        clearTextureCacheDir(cacheDir);
    }

    bool cachingStateSet = false;
    auto ensureCachingState = [&]() {
        if (!cachingStateSet) {
            Unzip::setLoadingState(TranslationManager::getTranslation("ui.loading.caching"));
            cachingStateSet = true;
        }
    };

    bool allSucceeded = true;

    auto precacheCostume = [&](const Costume &costume, bool isStage) {
        if (costume.isSVG) return;
        const std::string &name = costume.fullName;
        if (name.empty()) return;

        std::string cachePath = getTextureCachePath(name);
        if (cachePath.empty() || FileSystem::fileExists(cachePath)) return;

        size_t fileSize = 0;
        void *fileData = extractProjectAsset(name, fileSize);
        if (!fileData || fileSize == 0) {
            allSucceeded = false;
            return;
        }

        int rawW = 0, rawH = 0, channels = 0;
        unsigned char *pixels = stbi_load_from_memory(static_cast<const unsigned char *>(fileData), fileSize, &rawW, &rawH, &channels, 4);
        freeProjectAsset(fileData);
        if (!pixels || rawW <= 0 || rawH <= 0) {
            if (pixels) stbi_image_free(pixels);
            allSucceeded = false;
            return;
        }

        int curW = rawW, curH = rawH;
        unsigned char *curPixels = pixels;
        float logicalScale = 1.0f;

        if (curW * curH <= 128 * 128) {
            if (curPixels == pixels) stbi_image_free(curPixels);
            else free(curPixels);
            return;
        }

        ensureCachingState();

        // Downscale Retina (resolution == 2) assets on 64MB consoles if large (> 256x256)
        if (!OS::isEnhancedPlatform() && costume.bitmapResolution == 2) {
            if (curW * curH > 256 * 256) {
                int dw, dh;
                unsigned char *downscaled = Image::resizeRaster(curPixels, curW, curH, dw, dh);
                if (downscaled) {
                    logicalScale /= 2.0f;
                    if (curPixels == pixels) stbi_image_free(curPixels);
                    else free(curPixels);
                    curPixels = downscaled;
                    curW = dw;
                    curH = dh;
                }
            }
        }

        while (curW > FORCE_DOWNSCALE_DIM || curH > FORCE_DOWNSCALE_DIM) {
            int dw, dh;
            unsigned char *downscaled = Image::resizeRaster(curPixels, curW, curH, dw, dh);
            if (!downscaled) break;
            logicalScale *= static_cast<float>(dw) / static_cast<float>(curW);
            if (curPixels == pixels) stbi_image_free(curPixels);
            else free(curPixels);
            curPixels = downscaled;
            curW = dw;
            curH = dh;
        }

        bool isOpaque = true, isBinary = true;
        const int pitch = curW * 4;
        for (int y = 0; y < curH && isBinary; y++) {
            const uint8_t *row = curPixels + y * pitch;
            for (int x = 0; x < curW; x++) {
                if (row[x * 4 + 3] != 255) {
                    isOpaque = false;
                    if (row[x * 4 + 3] != 0) {
                        isBinary = false;
                        break;
                    }
                }
            }
        }

        uint32_t codec = isBinary ? CODEC_DXT1 : CODEC_DXT5;
        if (!writeCompressedTextureCache(cachePath, curW, curH, curPixels, pitch, isOpaque, codec, logicalScale)) {
            allSucceeded = false;
        }

        if (curPixels == pixels) stbi_image_free(curPixels);
        else free(curPixels);
    };

    if (Scratch::stageSprite) {
        for (const auto &costume : Scratch::stageSprite->costumes) precacheCostume(costume, true);
    }
    for (auto *sprite : Scratch::sprites) {
        if (sprite == Scratch::stageSprite) continue;
        for (const auto &costume : sprite->costumes) precacheCostume(costume, false);
    }

    if (allSucceeded) {
        writeTextFile(completeFile, signature);
    }
}

nonstd::expected<void, std::string> Image_NV2A::refreshTexture() {
    releaseEffects();
    destroyTiles();
    if (texture && nv2aRenderer) nv2aRenderer->destroyTexture(texture);
    if (!cachedAssetPath.empty()) {
        std::string cachePath = getTextureCachePath(cachedAssetPath);
        if (!cachePath.empty() && FileSystem::fileExists(cachePath)) {
            if (loadFromCache(cachePath)) return {};
        }
    }
    return setInitialTexture();
}

Image_NV2A::Image_NV2A(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
    nv2aRenderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    maxTextureSize = {1024, 1024};
    cachedAssetPath = filePath;

    std::string cachePath = getTextureCachePath(filePath);
    if (!cachePath.empty() && FileSystem::fileExists(cachePath)) {
        if (loadFromCache(cachePath)) return;
    }

    const auto initResult = init(filePath, zip, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }
    const auto potentialError = setInitialTexture();
    if (!potentialError.has_value()) error = potentialError.error();
}

Image_NV2A::Image_NV2A(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    nv2aRenderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    maxTextureSize = {1024, 1024};

    if (fromScratchProject) {
        cachedAssetPath = filePath;
        std::string cachePath = getTextureCachePath(filePath);
        if (!cachePath.empty() && FileSystem::fileExists(cachePath)) {
            if (loadFromCache(cachePath)) return;
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

Image_NV2A::~Image_NV2A() {
    releaseEffects();
    destroyTiles();
    if (texture && nv2aRenderer) {
        nv2aRenderer->destroyTexture(texture);
        texture = nullptr;
    }
}
