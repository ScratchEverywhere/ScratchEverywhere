#include "text_sdl1.hpp"
#include "render_sdl1.hpp"
#include <SDL_rotozoom.h>
#include <cmath>

TextObjectSDL1::TextObjectSDL1(std::string txt, double posX, double posY, std::string fontPath)
    : TextObjectBase(txt, posX, posY, fontPath, 30.0f) {
}

TextObjectSDL1::~TextObjectSDL1() = default;

void TextObjectSDL1::setRenderer(void *r) {
    renderer = static_cast<SDL_Surface *>(r);
}

void TextObjectSDL1::uploadAtlas(FontGeneration &gen) {
    gen.dirty = false;
}

void TextObjectSDL1::render(int xPos, int yPos) {
    if (!renderer || !fontAtlas || !fontAtlas->isValid() || layoutLines.empty()) return;

    FontGeneration &gen = touchGeneration();
    if (gen.dirty) uploadAtlas(gen);

    const int compW = std::max(1, (int)std::ceil(layoutWidth / glyphScale));
    const int compH = std::max(1, (int)std::ceil(layoutHeight / glyphScale));

    SDL_Surface *composite = SDL_CreateRGBSurface(SDL_HWSURFACE, compW, compH, 32, RMASK, GMASK, BMASK, AMASK);
    if (!composite) return;
    SDL_FillRect(composite, nullptr, SDL_MapRGBA(composite->format, 0, 0, 0, 0));

    const Uint8 r = (color >> 24) & 0xFF;
    const Uint8 g = (color >> 16) & 0xFF;
    const Uint8 b = (color >> 8) & 0xFF;
    const Uint8 a = color & 0xFF;

    const float lineHeight = gen.ascent - gen.descent + gen.lineGap;

    SDL_LockSurface(composite);
    for (size_t li = 0; li < layoutLines.size(); ++li) {
        const auto &glyphs = layoutLines[li];
        if (glyphs.empty()) continue;

        const float lineY = (float)li * lineHeight + gen.ascent;

        for (const GlyphQuad &q : glyphs) {
            const int sx = (int)(q.s0 * gen.atlasWidth);
            const int sy = (int)(q.t0 * gen.atlasHeight);
            const int gw = (int)((q.s1 - q.s0) * gen.atlasWidth);
            const int gh = (int)((q.t1 - q.t0) * gen.atlasHeight);
            const int dx0 = (int)q.x0;
            const int dy0 = (int)(lineY + q.y0);

            for (int row = 0; row < gh; ++row) {
                const int dy = dy0 + row;
                if (dy < 0 || dy >= compH) continue;
                const int srcRow = sy + row;

                for (int col = 0; col < gw; ++col) {
                    const int dx = dx0 + col;
                    if (dx < 0 || dx >= compW) continue;

                    unsigned char alpha = gen.pixels[(size_t)srcRow * gen.atlasWidth + (sx + col)];
                    if (alpha == 0) continue;

                    Uint8 combinedA = (Uint8)(((int)alpha * a) / 255);
                    Uint32 *px = (Uint32 *)((Uint8 *)composite->pixels + dy * composite->pitch + dx * 4);
                    *px = SDL_MapRGBA(composite->format, r, g, b, combinedA);
                }
            }
        }
    }
    SDL_UnlockSurface(composite);
    SDL_SetAlpha(composite, SDL_SRCALPHA, 255);

    SDL_Surface *surface = composite;
    bool freeSurface = false;
    if (std::fabs(glyphScale - 1.0f) > 0.001f) {
        SDL_Surface *scaled = zoomSurface(composite, glyphScale, glyphScale, SMOOTHING_OFF);
        if (scaled) {
            surface = scaled;
            freeSurface = true;
        }
    }

    SDL_Rect destRect;
    destRect.w = surface->w;
    destRect.h = surface->h;
    if (centerAligned) {
        destRect.x = xPos - (destRect.w / 2);
        destRect.y = yPos - (destRect.h / 2);
    } else {
        destRect.x = xPos;
        destRect.y = yPos;
    }

    SDL_BlitSurface(surface, nullptr, renderer, &destRect);

    if (freeSurface) SDL_FreeSurface(surface);
    SDL_FreeSurface(composite);
}
