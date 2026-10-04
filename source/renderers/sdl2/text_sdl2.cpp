#include "text_sdl2.hpp"
#include <vector>

TextObjectSDL2::TextObjectSDL2(std::string txt, double posX, double posY, std::string fontPath)
    : TextObjectBase(txt, posX, posY, fontPath, 30.0f) {
}

TextObjectSDL2::~TextObjectSDL2() = default;

void TextObjectSDL2::setRenderer(void *r) {
    renderer = static_cast<SDL_Renderer *>(r);
}

void TextObjectSDL2::uploadAtlas(FontGeneration &gen) {
    if (!renderer) return;

    std::vector<uint8_t> rgba((size_t)gen.atlasWidth * (size_t)gen.atlasHeight * 4);
    for (size_t i = 0; i < gen.pixels.size(); ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = gen.pixels[i];
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
    gen.destroyBackendHandle = [](void *handle) {
        SDL_DestroyTexture((SDL_Texture *)handle);
    };
    gen.dirty = false;
}

void TextObjectSDL2::render(int xPos, int yPos) {
    if (!renderer || !fontAtlas || !fontAtlas->isValid() || layoutLines.empty()) return;

    FontGeneration &gen = touchGeneration();
    if (gen.dirty) uploadAtlas(gen);
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

    const float lineHeight = (gen.ascent - gen.descent + gen.lineGap) * glyphScale;

    std::vector<SDL_Vertex> verts;
    std::vector<int> indices;

    for (size_t li = 0; li < layoutLines.size(); ++li) {
        const auto &glyphs = layoutLines[li];
        if (glyphs.empty()) continue;

        float lineX = drawX;
        float lineY = drawY + (float)li * lineHeight + gen.ascent * glyphScale;

        for (const GlyphQuad &q : glyphs) {
            float qx0 = lineX + q.x0 * glyphScale;
            float qy0 = lineY + q.y0 * glyphScale;
            float qx1 = lineX + q.x1 * glyphScale;
            float qy1 = lineY + q.y1 * glyphScale;

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
