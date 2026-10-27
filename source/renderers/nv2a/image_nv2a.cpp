#include "image_nv2a.hpp"
#include "nonstd/expected.hpp"
#include "renderer_nv2a.hpp"
#include <renderers/xbox/image_xbox.hpp>
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
static inline int getMaxTextureDim() { return OS::isEnhancedPlatform() ? 1024 : 512; }
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

    const int maxDim = getMaxTextureDim();
    while (imgData.width > maxDim || imgData.height > maxDim) {
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

    const double rotationDeg = Math::radiansToDegrees(rotationRad);
    const bool isCardinalRotation = std::fmod(std::abs(rotationDeg), 90.0) < 0.001 || std::fmod(std::abs(rotationDeg), 90.0) > 89.999;
    if (isCardinalRotation) {
        renderX = std::round(renderX);
        renderY = std::round(renderY);
        renderW = std::round(renderW);
        renderH = std::round(renderH);
    }

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

            if (isCardinalRotation) {
                px0 = std::round(px0);
                py0 = std::round(py0);
                px1 = std::round(px1);
                py1 = std::round(py1);
            }

            auto p_tl = rotatePoint(px0, py0);
            auto p_tr = rotatePoint(px1, py0);
            auto p_br = rotatePoint(px1, py1);
            auto p_bl = rotatePoint(px0, py1);

            if (isCardinalRotation) {
                p_tl.first = std::round(p_tl.first);
                p_tl.second = std::round(p_tl.second);
                p_tr.first = std::round(p_tr.first);
                p_tr.second = std::round(p_tr.second);
                p_br.first = std::round(p_br.first);
                p_br.second = std::round(p_br.second);
                p_bl.first = std::round(p_bl.first);
                p_bl.second = std::round(p_bl.second);
            }

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

            nv2aRenderer->bindTexture(tile.texture, tile.texture->scaleMode);
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
        effectSlots[i].effW = 0;
        effectSlots[i].effH = 0;
        effectSlots[i].lastUsedFrame = 0;
    }
    hsvCache.clear();
    hsvCache.shrink_to_fit();
    hsvW = hsvH = 0;
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

// ---------------------------------------------------------------------------
// Baked effect textures (color + pixelate)
//
// Everything below runs only when an effect *value changes*; a steady effect is a cache hit and costs nothing.
//  * Effect textures are allocated once at a fixed pow2 capacity (<=128x128). Changing the pixelate grid size
//    just changes uScale/vScale - no destroy/create (which used to force a full GPU idle every frame).
//  * Color shift works on a cached 8-bit HSV copy of the downsampled source. Per texel it is one integer add
//    and one integer HSV->RGB (no 32768-entry float LUT rebuild, no per-texel float math).
//  * Texels are composed in cached RAM, swizzled in cached RAM, then streamed to write-combined VRAM-visible
//    memory as one sequential copy (done inside Renderer_NV2A::updateTexture).
// ---------------------------------------------------------------------------
static constexpr int EFFECT_MAX_DIM = 128;
static constexpr int PIXELATE_MESH_MAX_CELLS = 256; // grids this small are cheaper as a mesh than as a texture refill

static inline uint32_t div255(uint32_t x) {
    x += 128;
    return (x + (x >> 8)) >> 8;
}

// h: 0..1535 (6 sextants x 256), s/v: 0..255. Same clamps as the Scratch shader (v < 0.055, s < 0.09).
static inline void rgb2hsvEffect(int r, int g, int b, uint16_t &h, uint8_t &s, uint8_t &v) {
    const int mx = std::max({r, g, b});
    const int mn = std::min({r, g, b});
    const int d = mx - mn;
    int hh = 0;
    int sat = 0;
    if (mx > 0) {
        sat = (d * 255 + mx / 2) / mx;
        if (d > 0) {
            if (mx == r) {
                hh = ((g - b) * 256) / d;
                if (hh < 0) hh += 1536;
            } else if (mx == g) {
                hh = 512 + ((b - r) * 256) / d;
            } else {
                hh = 1024 + ((r - g) * 256) / d;
            }
            if (hh >= 1536) hh -= 1536;
        }
    }
    int val = mx;
    if (val * 1000 < 55 * 255) {
        hh = 0;
        sat = 255;
        val = 14;
    } else if (d * 100 < 9 * mx) {
        hh = 0;
        sat = 23;
    }
    h = static_cast<uint16_t>(hh);
    s = static_cast<uint8_t>(sat);
    v = static_cast<uint8_t>(val);
}

// Returns bytes in memory order R,G,B (little-endian: r | g<<8 | b<<16), alpha left to the caller.
static inline uint32_t hsv2rgbEffect(uint32_t h, uint32_t s, uint32_t v) {
    if (s == 0) return v | (v << 8) | (v << 16);
    const uint32_t seg = h >> 8;
    const uint32_t f = h & 255;
    const uint32_t p = div255(v * (255 - s));
    const uint32_t q = div255(v * (255 - div255(s * f)));
    const uint32_t t = div255(v * (255 - div255(s * (255 - f))));
    uint32_t r, g, b;
    switch (seg) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return r | (g << 8) | (b << 16);
}

// Same grid-size rule as the pixelate mesh path. Returns false when the block is <= 1 texel (no visible pixelation).
static bool computePixelateDims(const ImageRenderParams &params, int srcW, int srcH, float imgScale, int &effW, int &effH) {
    const float texelScale = params.scale / (imgScale > 0.0f ? imgScale : 1.0f);
    const float scaleFactor = std::max(1.0f, texelScale);
    const float amount = (std::abs(params.pixelateEffect) / 10.0f) / scaleFactor;
    if (amount <= 1.0f) return false;

    effW = std::min(std::max(2, static_cast<int>(std::round(srcW / amount))), srcW);
    effH = std::min(std::max(2, static_cast<int>(std::round(srcH / amount))), srcH);
    if (effW > EFFECT_MAX_DIM || effH > EFFECT_MAX_DIM) {
        const float g = std::min(static_cast<float>(EFFECT_MAX_DIM) / effW, static_cast<float>(EFFECT_MAX_DIM) / effH);
        effW = std::max(2, static_cast<int>(std::round(effW * g)));
        effH = std::max(2, static_cast<int>(std::round(effH * g)));
    }
    return true;
}

static uint32_t s_effectLinear[EFFECT_MAX_DIM * EFFECT_MAX_DIM];

NV2ATexture *Image_NV2A::acquireEffectTexture(const ImageRenderParams &params) {
    if (imgData.width <= 0 || imgData.height <= 0) return nullptr;
    if (!nv2aRenderer && Render::getGlobalRenderer()) {
        nv2aRenderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    }
    if (!nv2aRenderer) return nullptr;

    const int srcW = imgData.width;
    const int srcH = imgData.height;
    const bool hasColor = (std::abs(params.colorEffect) > 0.001f);

    int effW = std::min(srcW, EFFECT_MAX_DIM);
    int effH = std::min(srcH, EFFECT_MAX_DIM);
    int pixW = 0, pixH = 0;
    const bool hasPixelGrid = (std::abs(params.pixelateEffect) > 0.001f) &&
                              computePixelateDims(params, srcW, srcH, imgData.scale, pixW, pixH);
    if (hasPixelGrid) {
        effW = pixW;
        effH = pixH;
    } else if (!hasColor) {
        return texture; // nothing to bake
    }

    const float colorKey = hasColor ? params.colorEffect : 0.0f;
    const uint32_t currentFrame = Renderer_NV2A::getFrameCounter();

    // 1) Cache hit: same color value and same grid -> reuse as-is (zero CPU work).
    for (int i = 0; i < 2; ++i) {
        EffectCacheSlot &s = effectSlots[i];
        if (s.texture && s.effW == effW && s.effH == effH && std::abs(s.color - colorKey) < 0.001f) {
            s.lastUsedFrame = currentFrame;
            return s.texture;
        }
    }

    // 2) Pick a slot. A slot already used by a draw queued THIS frame may still be read by the GPU, so it must
    //    never be rewritten in place; if both are in flight we retire the texture (freed after the GPU is idle)
    //    and allocate a fresh one.
    const bool busy0 = effectSlots[0].texture && effectSlots[0].lastUsedFrame == currentFrame;
    const bool busy1 = effectSlots[1].texture && effectSlots[1].lastUsedFrame == currentFrame;
    int chosen = 0;
    bool replace = false;
    if (!busy0) {
        chosen = 0;
    } else if (!busy1) {
        chosen = 1;
    } else {
        chosen = (effectSlots[0].lastUsedFrame <= effectSlots[1].lastUsedFrame) ? 0 : 1;
        replace = true;
    }
    EffectCacheSlot &slot = effectSlots[chosen];

    int srcPitch = 0;
    const uint8_t *src = getEffectSource(srcPitch);
    if (!src) return texture;

    // 3) Fixed-capacity texture (never reallocated when only the grid size changes).
    const int capW = toPowerOfTwo(std::min(srcW, EFFECT_MAX_DIM));
    const int capH = toPowerOfTwo(std::min(srcH, EFFECT_MAX_DIM));
    if (slot.texture && (replace || slot.texture->dataWidth != capW || slot.texture->dataHeight != capH)) {
        nv2aRenderer->destroyTexture(slot.texture); // deferred free
        slot.texture = nullptr;
        slot.effW = slot.effH = 0;
    }
    if (!slot.texture) {
        slot.texture = nv2aRenderer->createTexture(capW, capH, XGU_TEXTURE_FORMAT_A8B8G8R8_SWIZZLED, true);
        if (!slot.texture) return texture;
    }

    int sxArr[EFFECT_MAX_DIM];
    for (int x = 0; x < effW; ++x) {
        sxArr[x] = std::clamp((x * srcW + (srcW / 2)) / effW, 0, srcW - 1);
    }

    uint32_t *lin = s_effectLinear;

    if (hasColor) {
        // Cached HSV of the downsampled source; only rebuilt when the grid size changes.
        if (hsvW != effW || hsvH != effH || hsvCache.size() != static_cast<size_t>(effW) * effH) {
            hsvCache.resize(static_cast<size_t>(effW) * effH);
            for (int y = 0; y < effH; ++y) {
                const int sy = std::clamp((y * srcH + (srcH / 2)) / effH, 0, srcH - 1);
                const uint8_t *sRow = src + static_cast<size_t>(sy) * srcPitch;
                EffectHSVA *out = &hsvCache[static_cast<size_t>(y) * effW];
                for (int x = 0; x < effW; ++x) {
                    const uint8_t *p = sRow + sxArr[x] * 4;
                    EffectHSVA &e = out[x];
                    e.a = p[3];
                    if (e.a) {
                        rgb2hsvEffect(p[0], p[1], p[2], e.h, e.s, e.v);
                    } else {
                        e.h = 0;
                        e.s = 0;
                        e.v = 0;
                    }
                }
            }
            hsvW = effW;
            hsvH = effH;
        }

        const float cs = params.colorEffect / 200.0f;
        const float frac = cs - std::floor(cs);
        const uint32_t shift = static_cast<uint32_t>(std::lround(frac * 1536.0f)) % 1536u;

        for (int y = 0; y < effH; ++y) {
            uint32_t *row = lin + static_cast<size_t>(y) * capW;
            const EffectHSVA *hp = &hsvCache[static_cast<size_t>(y) * effW];
            for (int x = 0; x < effW; ++x) {
                const EffectHSVA &e = hp[x];
                if (!e.a) {
                    row[x] = 0;
                } else {
                    uint32_t h2 = e.h + shift;
                    if (h2 >= 1536) h2 -= 1536;
                    row[x] = hsv2rgbEffect(h2, e.s, e.v) | (static_cast<uint32_t>(e.a) << 24);
                }
            }
        }
    } else {
        // Pixelate only: straight nearest-neighbour sample at cell centres.
        for (int y = 0; y < effH; ++y) {
            const int sy = std::clamp((y * srcH + (srcH / 2)) / effH, 0, srcH - 1);
            const uint8_t *sRow = src + static_cast<size_t>(sy) * srcPitch;
            uint32_t *row = lin + static_cast<size_t>(y) * capW;
            for (int x = 0; x < effW; ++x) {
                memcpy(&row[x], sRow + sxArr[x] * 4, 4);
            }
        }
    }

    // Replicate the last column/row into the pow2 padding so bilinear filtering at the edge doesn't pull in
    // transparent black.
    for (int y = 0; y < effH; ++y) {
        uint32_t *row = lin + static_cast<size_t>(y) * capW;
        for (int x = effW; x < capW; ++x) row[x] = row[effW - 1];
    }
    for (int y = effH; y < capH; ++y) {
        memcpy(lin + static_cast<size_t>(y) * capW, lin + static_cast<size_t>(effH - 1) * capW, static_cast<size_t>(capW) * 4);
    }

    nv2aRenderer->updateTexture(slot.texture, lin, capW * 4);

    slot.texture->uScale = static_cast<float>(effW) / static_cast<float>(capW);
    slot.texture->vScale = static_cast<float>(effH) / static_cast<float>(capH);
    slot.color = colorKey;
    slot.effW = effW;
    slot.effH = effH;
    slot.lastUsedFrame = currentFrame;

    return slot.texture;
}

static inline void setNV2AVertexColor(NV2AVertex &v, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    v.color[0] = r;
    v.color[1] = g;
    v.color[2] = b;
    v.color[3] = a;
}

void Image_NV2A::renderMosaic(ImageRenderParams &params, NV2ATexture *activeTex) {
    if (!activeTex || !nv2aRenderer) return;

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

    const double rotationDeg = Math::radiansToDegrees(rotationRad);
    const bool isCardinalRotation = std::fmod(std::abs(rotationDeg), 90.0) < 0.001 || std::fmod(std::abs(rotationDeg), 90.0) > 89.999;
    if (isCardinalRotation) {
        renderX = std::round(renderX);
        renderY = std::round(renderY);
        renderW = std::round(renderW);
        renderH = std::round(renderH);
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

    int N = std::clamp(static_cast<int>(std::round((std::abs(params.mosaicEffect) + 10.0f) / 10.0f + 1e-3f)), 1, 16);

    // If texture is a full POT texture without subrect, use GPU texture WRAP (1 quad, 6 verts!)
    bool isFullTexture = (params.subrect == nullptr ||
                          (params.subrect->x == 0 && params.subrect->y == 0 &&
                           params.subrect->w == imgData.width && params.subrect->h == imgData.height));
    if (isFullTexture && activeTex->uScale == 1.0f && activeTex->vScale == 1.0f) {
        auto p_tl = rotatePoint(renderX, renderY);
        auto p_tr = rotatePoint(renderX + renderW, renderY);
        auto p_br = rotatePoint(renderX + renderW, renderY + renderH);
        auto p_bl = rotatePoint(renderX, renderY + renderH);

        if (isCardinalRotation) {
            p_tl.first = std::round(p_tl.first);
            p_tl.second = std::round(p_tl.second);
            p_tr.first = std::round(p_tr.first);
            p_tr.second = std::round(p_tr.second);
            p_br.first = std::round(p_br.first);
            p_br.second = std::round(p_br.second);
            p_bl.first = std::round(p_bl.first);
            p_bl.second = std::round(p_bl.second);
        }

        float u0 = params.flip ? static_cast<float>(N) : 0.0f;
        float u1 = params.flip ? 0.0f : static_cast<float>(N);
        float v0 = 0.0f;
        float v1 = static_cast<float>(N);

        NV2AVertex quadVerts[6] = {
            {{p_tl.first, p_tl.second}, {255, 255, 255, alpha}, {u0, v0}},
            {{p_tr.first, p_tr.second}, {255, 255, 255, alpha}, {u1, v0}},
            {{p_br.first, p_br.second}, {255, 255, 255, alpha}, {u1, v1}},
            {{p_tl.first, p_tl.second}, {255, 255, 255, alpha}, {u0, v0}},
            {{p_br.first, p_br.second}, {255, 255, 255, alpha}, {u1, v1}},
            {{p_bl.first, p_bl.second}, {255, 255, 255, alpha}, {u0, v1}}
        };

        const NV2AScaleMode mosaicScaleMode = (std::abs(params.pixelateEffect) > 0.001f) ? NV2AScaleMode::Nearest : activeTex->scaleMode;
        nv2aRenderer->bindTexture(activeTex, mosaicScaleMode, /*wrap=*/true);

        if (brightness > 0) {
            float b = brightness * 0.01f;
            nv2aRenderer->setBlendMode(NV2ABlendMode::Blend);
            nv2aRenderer->drawTriangles(quadVerts, 6);
            uint8_t addA = static_cast<uint8_t>(255.0f * b * opacity);
            if (addA > 0) {
                for (int i = 0; i < 6; ++i) {
                    setNV2AVertexColor(quadVerts[i], 255, 255, 255, addA);
                }
                nv2aRenderer->setBlendMode(NV2ABlendMode::Add);
                nv2aRenderer->drawTriangles(quadVerts, 6);
            }
        } else if (brightness < 0) {
            float b = brightness * 0.01f;
            uint8_t dim = static_cast<uint8_t>(255.0f * std::clamp(1.0f + b, 0.0f, 1.0f));
            for (int i = 0; i < 6; ++i) {
                setNV2AVertexColor(quadVerts[i], dim, dim, dim, alpha);
            }
            nv2aRenderer->setBlendMode(NV2ABlendMode::Blend);
            nv2aRenderer->drawTriangles(quadVerts, 6);
        } else {
            nv2aRenderer->setBlendMode(NV2ABlendMode::Blend);
            nv2aRenderer->drawTriangles(quadVerts, 6);
        }
        return;
    }

    // Fallback for subrect or NPOT texture: grid of quads with step vector affine additions
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

    float stepW = renderW / static_cast<float>(N);
    float stepH = renderH / static_cast<float>(N);
    float stepUx = stepW * cosA;
    float stepUy = stepW * sinA;
    float stepVx = -stepH * sinA;
    float stepVy = stepH * cosA;

    auto p00 = rotatePoint(renderX, renderY);

    static NV2AVertex s_mosaicVerts[16 * 16 * 6];
    size_t vertCount = 0;

    uint8_t cr = 255, cg = 255, cb = 255;
    if (brightness < 0) {
        float b = brightness * 0.01f;
        uint8_t dim = static_cast<uint8_t>(255.0f * std::clamp(1.0f + b, 0.0f, 1.0f));
        cr = cg = cb = dim;
    }

    for (int row = 0; row < N; ++row) {
        float rY_x = p00.first + row * stepVx;
        float rY_y = p00.second + row * stepVy;
        float rY1_x = rY_x + stepVx;
        float rY1_y = rY_y + stepVy;

        for (int col = 0; col < N; ++col) {
            float p_tl_x = rY_x + col * stepUx;
            float p_tl_y = rY_y + col * stepUy;
            float p_tr_x = p_tl_x + stepUx;
            float p_tr_y = p_tl_y + stepUy;
            float p_bl_x = rY1_x + col * stepUx;
            float p_bl_y = rY1_y + col * stepUy;
            float p_br_x = p_bl_x + stepUx;
            float p_br_y = p_bl_y + stepUy;

            s_mosaicVerts[vertCount++] = {{p_tl_x, p_tl_y}, {cr, cg, cb, alpha}, {u0, v0}};
            s_mosaicVerts[vertCount++] = {{p_tr_x, p_tr_y}, {cr, cg, cb, alpha}, {u1, v0}};
            s_mosaicVerts[vertCount++] = {{p_br_x, p_br_y}, {cr, cg, cb, alpha}, {u1, v1}};

            s_mosaicVerts[vertCount++] = {{p_tl_x, p_tl_y}, {cr, cg, cb, alpha}, {u0, v0}};
            s_mosaicVerts[vertCount++] = {{p_br_x, p_br_y}, {cr, cg, cb, alpha}, {u1, v1}};
            s_mosaicVerts[vertCount++] = {{p_bl_x, p_bl_y}, {cr, cg, cb, alpha}, {u0, v1}};
        }
    }

    const NV2AScaleMode mosaicScaleMode = (std::abs(params.pixelateEffect) > 0.001f) ? NV2AScaleMode::Nearest : activeTex->scaleMode;
    nv2aRenderer->bindTexture(activeTex, mosaicScaleMode, /*wrap=*/false);
    nv2aRenderer->setBlendMode(NV2ABlendMode::Blend);
    nv2aRenderer->drawTriangles(s_mosaicVerts, vertCount);

    if (brightness > 0) {
        float b = brightness * 0.01f;
        uint8_t addA = static_cast<uint8_t>(255.0f * b * opacity);
        if (addA > 0) {
            for (size_t i = 0; i < vertCount; ++i) {
                setNV2AVertexColor(s_mosaicVerts[i], 255, 255, 255, addA);
            }
            nv2aRenderer->setBlendMode(NV2ABlendMode::Add);
            nv2aRenderer->drawTriangles(s_mosaicVerts, vertCount);
        }
    }
}

void Image_NV2A::renderPixelate(ImageRenderParams &params, NV2ATexture *activeTex) {
    if (!activeTex || !nv2aRenderer) return;

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

    const double rotationDeg = Math::radiansToDegrees(rotationRad);
    const bool isCardinalRotation = std::fmod(std::abs(rotationDeg), 90.0) < 0.001 || std::fmod(std::abs(rotationDeg), 90.0) > 89.999;
    if (isCardinalRotation) {
        renderX = std::round(renderX);
        renderY = std::round(renderY);
        renderW = std::round(renderW);
        renderH = std::round(renderH);
    }

    float subX = 0.0f, subY = 0.0f;
    float subW = static_cast<float>(imgData.width), subH = static_cast<float>(imgData.height);
    if (params.subrect != nullptr) {
        subX = static_cast<float>(params.subrect->x);
        subY = static_cast<float>(params.subrect->y);
        subW = static_cast<float>(params.subrect->w);
        subH = static_cast<float>(params.subrect->h);
    }
    if (subW <= 0 || subH <= 0) return;

    float baseU0 = (subX / static_cast<float>(imgData.width)) * activeTex->uScale;
    float baseV0 = (subY / static_cast<float>(imgData.height)) * activeTex->vScale;
    float baseU1 = ((subX + subW) / static_cast<float>(imgData.width)) * activeTex->uScale;
    float baseV1 = ((subY + subH) / static_cast<float>(imgData.height)) * activeTex->vScale;

    if (params.flip) {
        std::swap(baseU0, baseU1);
    }

    const float pixelate = params.pixelateEffect;
    const float absPixelate = std::abs(pixelate);
    const float texelScale = scale / (imgData.scale > 0.0f ? imgData.scale : 1.0f);
    const float scaleFactor = std::max(1.0f, texelScale);
    const float pixelateAmount = absPixelate / 10.0f;
    // Scale down block size by scaleFactor so scaled-up sprites don't balloon into massive blocks
    const float blockSizeTexels = pixelateAmount / scaleFactor;
    const float blockSizeScreen = blockSizeTexels * texelScale;

    const float centerX = renderX + renderW / 2.0f;
    const float centerY = renderY + renderH / 2.0f;
    float cosA = 1.0f;
    float sinA = 0.0f;
    if (rotationRad != 0.0) {
        cosA = static_cast<float>(std::cos(rotationRad));
        sinA = static_cast<float>(std::sin(rotationRad));
    }

    auto rotatePoint = [&](float px, float py) -> std::pair<float, float> {
        if (rotationRad == 0.0) return {px, py};
        float dx = px - centerX;
        float dy = py - centerY;
        return {centerX + dx * cosA - dy * sinA, centerY + dx * sinA + dy * cosA};
    };

    auto drawSingleQuad = [&]() {
        auto p_tl = rotatePoint(renderX, renderY);
        auto p_tr = rotatePoint(renderX + renderW, renderY);
        auto p_br = rotatePoint(renderX + renderW, renderY + renderH);
        auto p_bl = rotatePoint(renderX, renderY + renderH);

        if (isCardinalRotation) {
            p_tl.first = std::round(p_tl.first);
            p_tl.second = std::round(p_tl.second);
            p_tr.first = std::round(p_tr.first);
            p_tr.second = std::round(p_tr.second);
            p_br.first = std::round(p_br.first);
            p_br.second = std::round(p_br.second);
            p_bl.first = std::round(p_bl.first);
            p_bl.second = std::round(p_bl.second);
        }

        uint8_t cr = 255, cg = 255, cb = 255;
        if (brightness < 0) {
            float b = brightness * 0.01f;
            uint8_t dim = static_cast<uint8_t>(255.0f * std::clamp(1.0f + b, 0.0f, 1.0f));
            cr = cg = cb = dim;
        }

        NV2AVertex quadVerts[6] = {
            {{p_tl.first, p_tl.second}, {cr, cg, cb, alpha}, {baseU0, baseV0}},
            {{p_tr.first, p_tr.second}, {cr, cg, cb, alpha}, {baseU1, baseV0}},
            {{p_br.first, p_br.second}, {cr, cg, cb, alpha}, {baseU1, baseV1}},
            {{p_tl.first, p_tl.second}, {cr, cg, cb, alpha}, {baseU0, baseV0}},
            {{p_br.first, p_br.second}, {cr, cg, cb, alpha}, {baseU1, baseV1}},
            {{p_bl.first, p_bl.second}, {cr, cg, cb, alpha}, {baseU0, baseV1}}
        };

        nv2aRenderer->bindTexture(activeTex, NV2AScaleMode::Nearest, /*wrap=*/false);
        nv2aRenderer->setBlendMode(NV2ABlendMode::Blend);
        nv2aRenderer->drawTriangles(quadVerts, 6);

        if (brightness > 0) {
            float b = brightness * 0.01f;
            uint8_t addA = static_cast<uint8_t>(255.0f * b * opacity);
            if (addA > 0) {
                for (int i = 0; i < 6; ++i) {
                    setNV2AVertexColor(quadVerts[i], 255, 255, 255, addA);
                }
                nv2aRenderer->setBlendMode(NV2ABlendMode::Add);
                nv2aRenderer->drawTriangles(quadVerts, 6);
            }
        }
    };

    // In Scratch 3 WebGL shader:
    // vec2 pixelTexelSize = u_tex_size / max(pixelateAmount, 1.0);
    // When pixelate <= 10.0, max(pixelateAmount, 1.0) is 1.0, meaning block size is <= 1 texel.
    // If block size is <= 1 texel or <= 1.25 screen pixels, the sprite is rendered at native
    // resolution using hardware nearest-neighbor filtering on the GPU (1 quad, 6 vertices).
    const bool isFinePixelation = (absPixelate <= 10.0f || blockSizeTexels <= 1.0f || blockSizeScreen <= 1.25f);
    if (isFinePixelation) {
        drawSingleQuad();
        return;
    }

    constexpr int MAX_PIXELATE_GRID = 48;
    int effW = std::max(2, static_cast<int>(std::round(subW / blockSizeTexels)));
    int effH = std::max(2, static_cast<int>(std::round(subH / blockSizeTexels)));

    if (effW >= static_cast<int>(subW) && effH >= static_cast<int>(subH)) {
        drawSingleQuad();
        return;
    }

    if (effW > MAX_PIXELATE_GRID || effH > MAX_PIXELATE_GRID) {
        float gridScale = std::min(static_cast<float>(MAX_PIXELATE_GRID) / effW,
                                   static_cast<float>(MAX_PIXELATE_GRID) / effH);
        effW = std::max(2, static_cast<int>(std::round(effW * gridScale)));
        effH = std::max(2, static_cast<int>(std::round(effH * gridScale)));
    }

    float stepW = renderW / static_cast<float>(effW);
    float stepH = renderH / static_cast<float>(effH);
    float stepUx = stepW * cosA;
    float stepUy = stepW * sinA;
    float stepVx = -stepH * sinA;
    float stepVy = stepH * cosA;

    auto p00 = rotatePoint(renderX, renderY);

    static NV2AVertex s_pixelateVerts[MAX_PIXELATE_GRID * MAX_PIXELATE_GRID * 6];
    size_t vertCount = 0;

    uint8_t cr = 255, cg = 255, cb = 255;
    if (brightness < 0) {
        float b = brightness * 0.01f;
        uint8_t dim = static_cast<uint8_t>(255.0f * std::clamp(1.0f + b, 0.0f, 1.0f));
        cr = cg = cb = dim;
    }

    const float invEffW = 1.0f / static_cast<float>(effW);
    const float invEffH = 1.0f / static_cast<float>(effH);
    const float uSpan = baseU1 - baseU0;
    const float vSpan = baseV1 - baseV0;

    for (int row = 0; row < effH; ++row) {
        float rY_x = p00.first + row * stepVx;
        float rY_y = p00.second + row * stepVy;
        float rY1_x = rY_x + stepVx;
        float rY1_y = rY_y + stepVy;

        float vCenter = baseV0 + (static_cast<float>(row) + 0.5f) * invEffH * vSpan;

        for (int col = 0; col < effW; ++col) {
            float p_tl_x = rY_x + col * stepUx;
            float p_tl_y = rY_y + col * stepUy;
            float p_tr_x = p_tl_x + stepUx;
            float p_tr_y = p_tl_y + stepUy;
            float p_bl_x = rY1_x + col * stepUx;
            float p_bl_y = rY1_y + col * stepUy;
            float p_br_x = p_bl_x + stepUx;
            float p_br_y = p_bl_y + stepUy;

            float uCenter = baseU0 + (static_cast<float>(col) + 0.5f) * invEffW * uSpan;

            s_pixelateVerts[vertCount++] = {{p_tl_x, p_tl_y}, {cr, cg, cb, alpha}, {uCenter, vCenter}};
            s_pixelateVerts[vertCount++] = {{p_tr_x, p_tr_y}, {cr, cg, cb, alpha}, {uCenter, vCenter}};
            s_pixelateVerts[vertCount++] = {{p_br_x, p_br_y}, {cr, cg, cb, alpha}, {uCenter, vCenter}};

            s_pixelateVerts[vertCount++] = {{p_tl_x, p_tl_y}, {cr, cg, cb, alpha}, {uCenter, vCenter}};
            s_pixelateVerts[vertCount++] = {{p_br_x, p_br_y}, {cr, cg, cb, alpha}, {uCenter, vCenter}};
            s_pixelateVerts[vertCount++] = {{p_bl_x, p_bl_y}, {cr, cg, cb, alpha}, {uCenter, vCenter}};
        }
    }

    nv2aRenderer->bindTexture(activeTex, NV2AScaleMode::Nearest, /*wrap=*/false);
    nv2aRenderer->setBlendMode(NV2ABlendMode::Blend);
    nv2aRenderer->drawTriangles(s_pixelateVerts, vertCount);

    if (brightness > 0) {
        float b = brightness * 0.01f;
        uint8_t addA = static_cast<uint8_t>(255.0f * b * opacity);
        if (addA > 0) {
            for (size_t i = 0; i < vertCount; ++i) {
                setNV2AVertexColor(s_pixelateVerts[i], 255, 255, 255, addA);
            }
            nv2aRenderer->setBlendMode(NV2ABlendMode::Add);
            nv2aRenderer->drawTriangles(s_pixelateVerts, vertCount);
        }
    }
}

void Image_NV2A::renderDeformedMesh(ImageRenderParams &params, NV2ATexture *activeTex,
                                    bool hasWhirl, bool hasFisheye, bool hasMosaic) {
    if (!activeTex || !nv2aRenderer) return;

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

    const double rotationDeg = Math::radiansToDegrees(rotationRad);
    const bool isCardinalRotation = std::fmod(std::abs(rotationDeg), 90.0) < 0.001 || std::fmod(std::abs(rotationDeg), 90.0) > 89.999;
    if (isCardinalRotation) {
        renderX = std::round(renderX);
        renderY = std::round(renderY);
        renderW = std::round(renderW);
        renderH = std::round(renderH);
    }

    float subX = 0.0f, subY = 0.0f;
    float subW = static_cast<float>(imgData.width), subH = static_cast<float>(imgData.height);
    if (params.subrect != nullptr) {
        subX = static_cast<float>(params.subrect->x);
        subY = static_cast<float>(params.subrect->y);
        subW = static_cast<float>(params.subrect->w);
        subH = static_cast<float>(params.subrect->h);
    }
    if (subW <= 0 || subH <= 0) return;

    float baseU0 = (subX / static_cast<float>(imgData.width)) * activeTex->uScale;
    float baseV0 = (subY / static_cast<float>(imgData.height)) * activeTex->vScale;
    float baseU1 = ((subX + subW) / static_cast<float>(imgData.width)) * activeTex->uScale;
    float baseV1 = ((subY + subH) / static_cast<float>(imgData.height)) * activeTex->vScale;

    if (params.flip) {
        std::swap(baseU0, baseU1);
    }

    const float centerX = renderX + renderW / 2.0f;
    const float centerY = renderY + renderH / 2.0f;
    float cosA = 1.0f;
    float sinA = 0.0f;
    if (rotationRad != 0.0) {
        cosA = static_cast<float>(std::cos(rotationRad));
        sinA = static_cast<float>(std::sin(rotationRad));
    }

    auto rotatePoint = [&](float px, float py) -> std::pair<float, float> {
        if (rotationRad == 0.0) return {px, py};
        float dx = px - centerX;
        float dy = py - centerY;
        return {centerX + dx * cosA - dy * sinA, centerY + dx * sinA + dy * cosA};
    };

    const float mosaic = params.mosaicEffect;
    const float whirl = params.whirlEffect;
    const float fisheye = params.fisheyeEffect;

    const float tiles = hasMosaic ? std::clamp(std::round((std::abs(mosaic) + 10.0f) / 10.0f + 1e-3f), 1.0f, 16.0f) : 1.0f;
    const float whirlRad = whirl * 3.141592653589793f / 180.0f;
    const float fisheyeAmount = std::max(0.0f, (fisheye + 100.0f) / 100.0f);
    const bool doFisheye = hasFisheye && (std::abs(fisheyeAmount - 1.0f) > 0.001f);

    constexpr int M = 8;
    constexpr float invM = 1.0f / static_cast<float>(M);
    struct GridPt {
        std::pair<float, float> pos;
        float u, v;
    };
    GridPt grid[M + 1][M + 1];

    // Cache normalized UV grid for whirl and fisheye to avoid 81 sqrt/sin/cos/pow calls per frame
    static float s_cachedWhirl = -99999.0f;
    static float s_cachedFisheye = -99999.0f;
    static float s_cachedMosaic = -99999.0f;
    static float s_cachedU[M + 1][M + 1];
    static float s_cachedV[M + 1][M + 1];

    if (whirl != s_cachedWhirl || fisheye != s_cachedFisheye || mosaic != s_cachedMosaic) {
        for (int j = 0; j <= M; ++j) {
            float gy = static_cast<float>(j) * invM;
            for (int i = 0; i <= M; ++i) {
                float gx = static_cast<float>(i) * invM;
                float u = gx;
                float v = gy;

                if (hasMosaic) {
                    u = u * tiles;
                    u -= std::floor(u);
                    v = v * tiles;
                    v -= std::floor(v);
                }

                float cx = u - 0.5f;
                float cy = v - 0.5f;
                float distSq = cx * cx + cy * cy;

                if (distSq < 0.25f) {
                    if (hasWhirl) {
                        float dist = std::sqrt(distSq);
                        float whirlFactor = 1.0f - (dist * 2.0f);
                        float whirlActual = whirlRad * whirlFactor * whirlFactor;
                        float s = std::sin(whirlActual);
                        float c = std::cos(whirlActual);
                        float ncx = c * cx - s * cy;
                        float ncy = s * cx + c * cy;
                        cx = ncx;
                        cy = ncy;
                    }

                    if (doFisheye) {
                        float vx = cx * 2.0f;
                        float vy = cy * 2.0f;
                        float vecLenSq = vx * vx + vy * vy;
                        if (vecLenSq > 0.000001f) {
                            float vecLen = std::sqrt(vecLenSq);
                            float r = std::pow(std::min(vecLen, 1.0f), fisheyeAmount) * std::max(1.0f, vecLen);
                            float invLen = 1.0f / vecLen;
                            cx = r * (vx * invLen) * 0.5f;
                            cy = r * (vy * invLen) * 0.5f;
                        }
                    }
                }

                s_cachedU[j][i] = std::clamp(cx + 0.5f, 0.0f, 1.0f);
                s_cachedV[j][i] = std::clamp(cy + 0.5f, 0.0f, 1.0f);
            }
        }
        s_cachedWhirl = whirl;
        s_cachedFisheye = fisheye;
        s_cachedMosaic = mosaic;
    }

    float stepUx = (renderW * invM) * cosA;
    float stepUy = (renderW * invM) * sinA;
    float stepVx = -(renderH * invM) * sinA;
    float stepVy = (renderH * invM) * cosA;
    auto p00 = rotatePoint(renderX, renderY);

    for (int j = 0; j <= M; ++j) {
        float rY_x = p00.first + j * stepVx;
        float rY_y = p00.second + j * stepVy;

        for (int i = 0; i <= M; ++i) {
            grid[j][i].pos = {rY_x + i * stepUx, rY_y + i * stepUy};
            grid[j][i].u = baseU0 + s_cachedU[j][i] * (baseU1 - baseU0);
            grid[j][i].v = baseV0 + s_cachedV[j][i] * (baseV1 - baseV0);
        }
    }

    static NV2AVertex s_deformedVerts[M * M * 6];
    size_t vertCount = 0;

    uint8_t cr = 255, cg = 255, cb = 255;
    if (brightness < 0) {
        float b = brightness * 0.01f;
        uint8_t dim = static_cast<uint8_t>(255.0f * std::clamp(1.0f + b, 0.0f, 1.0f));
        cr = cg = cb = dim;
    }

    for (int j = 0; j < M; ++j) {
        for (int i = 0; i < M; ++i) {
            const auto &p00_pt = grid[j][i];
            const auto &p10_pt = grid[j][i + 1];
            const auto &p11_pt = grid[j + 1][i + 1];
            const auto &p01_pt = grid[j + 1][i];

            s_deformedVerts[vertCount++] = {{p00_pt.pos.first, p00_pt.pos.second}, {cr, cg, cb, alpha}, {p00_pt.u, p00_pt.v}};
            s_deformedVerts[vertCount++] = {{p10_pt.pos.first, p10_pt.pos.second}, {cr, cg, cb, alpha}, {p10_pt.u, p10_pt.v}};
            s_deformedVerts[vertCount++] = {{p11_pt.pos.first, p11_pt.pos.second}, {cr, cg, cb, alpha}, {p11_pt.u, p11_pt.v}};

            s_deformedVerts[vertCount++] = {{p00_pt.pos.first, p00_pt.pos.second}, {cr, cg, cb, alpha}, {p00_pt.u, p00_pt.v}};
            s_deformedVerts[vertCount++] = {{p11_pt.pos.first, p11_pt.pos.second}, {cr, cg, cb, alpha}, {p11_pt.u, p11_pt.v}};
            s_deformedVerts[vertCount++] = {{p01_pt.pos.first, p01_pt.pos.second}, {cr, cg, cb, alpha}, {p01_pt.u, p01_pt.v}};
        }
    }

    const NV2AScaleMode meshScaleMode = (std::abs(params.pixelateEffect) > 0.001f) ? NV2AScaleMode::Nearest : activeTex->scaleMode;
    nv2aRenderer->bindTexture(activeTex, meshScaleMode, /*wrap=*/false);
    nv2aRenderer->setBlendMode(NV2ABlendMode::Blend);
    nv2aRenderer->drawTriangles(s_deformedVerts, vertCount);

    if (brightness > 0) {
        float b = brightness * 0.01f;
        uint8_t addA = static_cast<uint8_t>(255.0f * b * opacity);
        if (addA > 0) {
            for (size_t k = 0; k < vertCount; ++k) {
                setNV2AVertexColor(s_deformedVerts[k], 255, 255, 255, addA);
            }
            nv2aRenderer->setBlendMode(NV2ABlendMode::Add);
            nv2aRenderer->drawTriangles(s_deformedVerts, vertCount);
        }
    }
}

void Image_NV2A::render(ImageRenderParams &params) {
    if (!nv2aRenderer && Render::getGlobalRenderer()) {
        nv2aRenderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    }
    if (!nv2aRenderer) return;

    bool hasWhirl = (std::abs(params.whirlEffect) > 0.001f);
    bool hasFisheye = (std::abs(params.fisheyeEffect) > 0.001f);
    bool hasMosaic = (std::abs(params.mosaicEffect) > 0.001f);
    bool hasPixelate = (std::abs(params.pixelateEffect) > 0.001f);
    bool hasColor = (std::abs(params.colorEffect) > 0.001f);
    bool hasEffects = (hasWhirl || hasFisheye || hasMosaic || hasPixelate || hasColor);

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

    const bool hasMeshEffects = (hasWhirl || hasFisheye || hasMosaic);
    NV2ATexture *activeTex = texture;
    bool needsPixelateTexture = false;
    bool pixelateBaked = false; // pixelate resolved by a (cached) low-res texture + 1 nearest-filtered quad
    if (hasPixelate) {
        int gw = 0, gh = 0;
        if (computePixelateDims(params, imgData.width, imgData.height, imgData.scale, gw, gh)) {
            const bool smallGrid = (gw * gh) <= PIXELATE_MESH_MAX_CELLS;
            if (hasMeshEffects || hasColor || (params.subrect == nullptr && !smallGrid)) {
                needsPixelateTexture = true;
                pixelateBaked = !hasMeshEffects;
            }
        }
    }
    if (hasColor || needsPixelateTexture) {
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

    if (hasWhirl || hasFisheye) {
        renderDeformedMesh(params, activeTex, hasWhirl, hasFisheye, hasMosaic);
        return;
    }

    if (hasMosaic) {
        renderMosaic(params, activeTex);
        return;
    }

    if (hasPixelate && !pixelateBaked) {
        renderPixelate(params, activeTex);
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

    const double rotationDeg = Math::radiansToDegrees(rotationRad);
    const bool isCardinalRotation = std::fmod(std::abs(rotationDeg), 90.0) < 0.001 || std::fmod(std::abs(rotationDeg), 90.0) > 89.999;
    if (isCardinalRotation) {
        renderX = std::round(renderX);
        renderY = std::round(renderY);
        renderW = std::round(renderW);
        renderH = std::round(renderH);
    }

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

    if (isCardinalRotation) {
        p_tl.first = std::round(p_tl.first);
        p_tl.second = std::round(p_tl.second);
        p_tr.first = std::round(p_tr.first);
        p_tr.second = std::round(p_tr.second);
        p_br.first = std::round(p_br.first);
        p_br.second = std::round(p_br.second);
        p_bl.first = std::round(p_bl.first);
        p_bl.second = std::round(p_bl.second);
    }

    auto drawQuad = [&](uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca, NV2ABlendMode blendMode) {
        NV2AVertex verts[6] = {
            {{p_tl.first, p_tl.second}, {cr, cg, cb, ca}, {u0, v0}},
            {{p_tr.first, p_tr.second}, {cr, cg, cb, ca}, {u1, v0}},
            {{p_br.first, p_br.second}, {cr, cg, cb, ca}, {u1, v1}},
            {{p_tl.first, p_tl.second}, {cr, cg, cb, ca}, {u0, v0}},
            {{p_br.first, p_br.second}, {cr, cg, cb, ca}, {u1, v1}},
            {{p_bl.first, p_bl.second}, {cr, cg, cb, ca}, {u0, v1}}
        };
        const NV2AScaleMode scaleMode = hasPixelate ? NV2AScaleMode::Nearest : activeTex->scaleMode;
        nv2aRenderer->bindTexture(activeTex, scaleMode);
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

    float destX = static_cast<float>(xPos - (centered ? width / 2.0f : 0));
    float destY = static_cast<float>(yPos - (centered ? height / 2.0f : 0));
    destX = std::round(destX);
    destY = std::round(destY);
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
        nv2aRenderer->bindTexture(texture, texture->scaleMode);
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

    const int maxDim = getMaxTextureDim();
    while (imgData.width > maxDim || imgData.height > maxDim) {
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
                    "_nv2a_v" + std::to_string(XBT_VERSION) + "_" + ramMode + "\\";
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
        (memcmp(header.magic, "XBTN", 4) != 0 &&
         memcmp(header.magic, "XBTX", 4) != 0 &&
         memcmp(header.magic, "XBT1", 4) != 0) ||
        (header.version != XBT_VERSION && header.version != 5) ||
        header.numTiles == 0) {
        fclose(f);
        remove(cachePath.c_str());
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
        remove(cachePath.c_str());
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
    memcpy(header.magic, "XBTN", 4);
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
           " maxdim=" + std::to_string(getMaxTextureDim());
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
        if (!cachePath.empty() && FileSystem::fileExists(cachePath)) {
            FILE *vf = fopen(cachePath.c_str(), "rb");
            if (vf) {
                XBTHeader vhdr;
                bool ok = (fread(&vhdr, sizeof(vhdr), 1, vf) == 1 &&
                           (memcmp(vhdr.magic, "XBTN", 4) == 0 ||
                            memcmp(vhdr.magic, "XBTX", 4) == 0 ||
                            memcmp(vhdr.magic, "XBT1", 4) == 0) &&
                           (vhdr.version == XBT_VERSION || vhdr.version == 5) &&
                           vhdr.numTiles > 0);
                fclose(vf);
                if (ok) return;
                remove(cachePath.c_str());
            }
        }
        std::string altCachePath = Image_Xbox::getTextureCachePath(name);
        if (!altCachePath.empty() && FileSystem::fileExists(altCachePath)) {
            FILE *vf = fopen(altCachePath.c_str(), "rb");
            if (vf) {
                XBTHeader vhdr;
                bool ok = (fread(&vhdr, sizeof(vhdr), 1, vf) == 1 &&
                           (memcmp(vhdr.magic, "XBTN", 4) == 0 ||
                            memcmp(vhdr.magic, "XBTX", 4) == 0 ||
                            memcmp(vhdr.magic, "XBT1", 4) == 0) &&
                           (vhdr.version == XBT_VERSION || vhdr.version == 5) &&
                           vhdr.numTiles > 0);
                fclose(vf);
                if (ok) return;
            }
        }

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

        const int maxDim = getMaxTextureDim();
        while (curW > maxDim || curH > maxDim) {
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
        std::string altCachePath = Image_Xbox::getTextureCachePath(cachedAssetPath);
        if (!altCachePath.empty() && FileSystem::fileExists(altCachePath)) {
            if (loadFromCache(altCachePath)) return {};
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
    std::string altCachePath = Image_Xbox::getTextureCachePath(filePath);
    if (!altCachePath.empty() && FileSystem::fileExists(altCachePath)) {
        if (loadFromCache(altCachePath)) return;
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
        std::string altCachePath = Image_Xbox::getTextureCachePath(filePath);
        if (!altCachePath.empty() && FileSystem::fileExists(altCachePath)) {
            if (loadFromCache(altCachePath)) return;
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
