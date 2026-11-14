#include "text_sdl2.hpp"
#include <algorithm>
#include <cmath>
#include <renderer.hpp>
#include <renderer_sdl2.hpp>
#include <unordered_map>
#include <vector>

#ifdef __VFLASH__
namespace {
struct ColoredAtlasEntry {
    uint64_t uploadVersion = 0;
    SDL_Texture *tex = nullptr;
};
struct ColoredAtlasKey {
    const FontGeneration *gen;
    uint32_t rgb;
    bool operator==(const ColoredAtlasKey &o) const {
        return gen == o.gen && rgb == o.rgb;
    }
};
struct ColoredAtlasKeyHash {
    size_t operator()(const ColoredAtlasKey &k) const {
        return std::hash<const void *>()(k.gen) ^ (std::hash<uint32_t>()(k.rgb) << 1);
    }
};
static std::unordered_map<ColoredAtlasKey, ColoredAtlasEntry, ColoredAtlasKeyHash> vflashColoredAtlases;
static std::unordered_map<const FontGeneration *, uint64_t> vflashUploadVersions;

static void destroyVflashColoredAtlasesForGen(const FontGeneration *gen) {
    for (auto it = vflashColoredAtlases.begin(); it != vflashColoredAtlases.end();) {
        if (it->first.gen == gen) {
            if (it->second.tex) {
                SDL_DestroyTexture(it->second.tex);
            }
            it = vflashColoredAtlases.erase(it);
        } else {
            ++it;
        }
    }
    vflashUploadVersions.erase(gen);
}

static SDL_Texture *createColoredAtlasTexture(SDL_Renderer *renderer, const FontGeneration &gen, uint8_t r, uint8_t g, uint8_t b) {
    std::vector<uint8_t> rgba((size_t)gen.atlasWidth * (size_t)gen.atlasHeight * 4);
    for (size_t i = 0; i < gen.pixels.size(); ++i) {
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
        rgba[i * 4 + 3] = gen.pixels[i] >= 96 ? 255 : 0;
    }
    SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, gen.atlasWidth, gen.atlasHeight);
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(tex, nullptr, rgba.data(), gen.atlasWidth * 4);
    }
    return tex;
}
} // namespace
#endif

TextObjectSDL2::TextObjectSDL2(std::string txt, double posX, double posY, std::string fontPath)
    : TextObjectBase(txt, posX, posY, fontPath, 30.0f) {
    renderer = static_cast<Renderer_SDL2 *>(Render::getGlobalRenderer())->Sdlrenderer;
}

TextObjectSDL2::~TextObjectSDL2() = default;

void TextObjectSDL2::uploadAtlas(FontGeneration &gen) {
    if (!renderer) return;

    std::vector<uint8_t> rgba((size_t)gen.atlasWidth * (size_t)gen.atlasHeight * 4);
    for (size_t i = 0; i < gen.pixels.size(); ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
#ifdef __VFLASH__
        rgba[i * 4 + 3] = gen.pixels[i] >= 96 ? 255 : 0;
#else
        rgba[i * 4 + 3] = gen.pixels[i];
#endif
    }

    if (gen.backendHandle) {
        SDL_DestroyTexture((SDL_Texture *)gen.backendHandle);
        gen.backendHandle = nullptr;
    }

    SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, gen.atlasWidth, gen.atlasHeight);
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(tex, nullptr, rgba.data(), gen.atlasWidth * 4);
    }

    gen.backendHandle = tex;
#ifdef __VFLASH__
    if (!tex) {
        destroyVflashColoredAtlasesForGen(&gen);
        gen.destroyBackendHandle = nullptr;
    } else {
        const FontGeneration *genPtr = &gen;
        gen.destroyBackendHandle = [genPtr](void *handle) {
            if (handle) SDL_DestroyTexture((SDL_Texture *)handle);
            destroyVflashColoredAtlasesForGen(genPtr);
        };
    }
#else
    gen.destroyBackendHandle = [](void *handle) {
        SDL_DestroyTexture((SDL_Texture *)handle);
    };
#endif
    gen.dirty = false;
}

void TextObjectSDL2::render(int xPos, int yPos) {
    if (!renderer || !fontAtlas || !fontAtlas->isValid() || layoutLines.empty()) return;

    FontGeneration &gen = touchGeneration();
    if (gen.dirty) {
        uploadAtlas(gen);
#ifdef __VFLASH__
        ++vflashUploadVersions[&gen];
#endif
    }
    if (!gen.backendHandle) return;

    SDL_Texture *tex = (SDL_Texture *)gen.backendHandle;

    SDL_Color col;
    col.r = (color >> 24) & 0xFF;
    col.g = (color >> 16) & 0xFF;
    col.b = (color >> 8) & 0xFF;
    col.a = color & 0xFF;

#ifdef __VFLASH__
    uint32_t rgb24 = ((uint32_t)col.r << 16) | ((uint32_t)col.g << 8) | (uint32_t)col.b;
    if (rgb24 != 0xFFFFFFu) {
        uint64_t ver = vflashUploadVersions[&gen];
        ColoredAtlasKey key{&gen, rgb24};
        auto &entry = vflashColoredAtlases[key];
        if (!entry.tex || entry.uploadVersion != ver) {
            if (entry.tex) SDL_DestroyTexture(entry.tex);
            entry.tex = createColoredAtlasTexture(renderer, gen, col.r, col.g, col.b);
            entry.uploadVersion = ver;
        }
        if (entry.tex) {
            tex = entry.tex;
            col.r = 255;
            col.g = 255;
            col.b = 255;
        }
    }
#endif

    float drawX = (float)xPos;
    float drawY = (float)yPos;
    if (centerAligned) {
        drawX -= layoutWidth / 2.0f;
        drawY -= layoutHeight / 2.0f;
    }
#ifdef __VFLASH__
    drawX = std::round(drawX);
    drawY = std::round(drawY);
    const float renderScale = std::max(1.0f, std::round(glyphScale));
#else
    const float renderScale = glyphScale;
#endif
    const float lineHeight = (gen.ascent - gen.descent + gen.lineGap) * renderScale;

    std::vector<SDL_Vertex> verts;
    std::vector<int> indices;

    for (size_t li = 0; li < layoutLines.size(); ++li) {
        const auto &glyphs = layoutLines[li];
        if (glyphs.empty()) continue;

        float lineX = drawX;
#ifdef __VFLASH__
        float lineY = std::round(drawY + (float)li * lineHeight + gen.ascent * renderScale);
#else
        float lineY = drawY + (float)li * lineHeight + gen.ascent * renderScale;
#endif

        for (const GlyphQuad &q : glyphs) {
            float qx0 = lineX + q.x0 * renderScale;
            float qy0 = lineY + q.y0 * renderScale;
            float qx1 = lineX + q.x1 * renderScale;
            float qy1 = lineY + q.y1 * renderScale;

            int base = (int)verts.size();
            verts.push_back({{qx0, qy0}, col, {q.s0, q.t0}});
            verts.push_back({{qx1, qy0}, col, {q.s1, q.t0}});
            verts.push_back({{qx1, qy1}, col, {q.s1, q.t1}});
            verts.push_back({{qx0, qy1}, col, {q.s0, q.t1}});

            indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
        }
    }

    if (!verts.empty()) {
        SDL_RenderGeometry(renderer, tex, verts.data(), (int)verts.size(), indices.data(), (int)indices.size());
    }
}
