#include "text_sdl2.hpp"
#include <algorithm>
#include <cmath>
#include <renderer.hpp>
#include <renderer_sdl2.hpp>
#include <vector>

#if defined(__VFLASH__) && defined(SE_VFLASH_TELEMETRY)
#include <vflash/clock.h>
extern uint32_t g_se_vflash_upload_ticks;
extern uint32_t g_se_vflash_upload_count;
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

    SDL_Texture *tex = (SDL_Texture *)gen.backendHandle;
    int texW = 0;
    int texH = 0;
    if (tex && (SDL_QueryTexture(tex, nullptr, nullptr, &texW, &texH) != 0 ||
                texW != gen.atlasWidth || texH != gen.atlasHeight)) {
        SDL_DestroyTexture(tex);
        tex = nullptr;
        gen.backendHandle = nullptr;
    }

    if (!tex) {
        tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, gen.atlasWidth, gen.atlasHeight);
        if (tex) {
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        }
        gen.backendHandle = tex;
    }

    if (tex) {
#if defined(__VFLASH__) && defined(SE_VFLASH_TELEMETRY)
        uint32_t ut0 = vflash_clock_ticks();
#endif
        SDL_UpdateTexture(tex, nullptr, rgba.data(), gen.atlasWidth * 4);
#if defined(__VFLASH__) && defined(SE_VFLASH_TELEMETRY)
        g_se_vflash_upload_ticks += (uint16_t)(vflash_clock_ticks() - ut0);
        g_se_vflash_upload_count++;
#endif
    }

    gen.destroyBackendHandle = [](void *handle) {
        if (handle) SDL_DestroyTexture((SDL_Texture *)handle);
    };
    gen.dirty = false;
}

void TextObjectSDL2::render(int xPos, int yPos) {
    if (!renderer || !fontAtlas || !fontAtlas->isValid() || layoutLines.empty()) return;

    FontGeneration &gen = touchGeneration();
    if (gen.dirty) {
        uploadAtlas(gen);
    }
    if (!gen.backendHandle) return;

    SDL_Texture *tex = (SDL_Texture *)gen.backendHandle;

    SDL_Color col;
    col.r = (color >> 24) & 0xFF;
    col.g = (color >> 16) & 0xFF;
    col.b = (color >> 8) & 0xFF;
    col.a = color & 0xFF;

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
