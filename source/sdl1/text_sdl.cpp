#include "text_sdl.hpp"
#include "../scratch/render.hpp"
#include "os.hpp"
#include "render.hpp"
#include "text.hpp"
#include <SDL/SDL_gfxBlitFunc.h>
#include <SDL/SDL_rotozoom.h>
#include <SDL/SDL_video.h>
#include <iostream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(__PC__) || defined(__PSP__)
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

std::unordered_map<std::string, TTF_Font *> TextObjectSDL::fonts;
std::unordered_map<std::string, size_t> TextObjectSDL::fontUsageCount;

TextObjectSDL::TextObjectSDL(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {

    // get font
    if (fontPath.empty()) {
        fontPath = "gfx/menu/LibSansN";
    }
    fontPath = OS::getRomFSLocation() + fontPath;
    fontPath = fontPath + ".ttf";

    // open font if not loaded
    if (fonts.find(fontPath) == fonts.end()) {
#if defined(__PC__) || defined(__PSP__)
        const auto &file = cmrc::romfs::get_filesystem().open(fontPath);
        TTF_Font *loadedFont = TTF_OpenFontRW(SDL_RWFromConstMem(file.begin(), file.size()), 1, 30);
#else
        TTF_Font *loadedFont = TTF_OpenFont(fontPath.c_str(), 30);
#endif
        if (!loadedFont) {
            Log::logError("Failed to load font " + fontPath + ": " + TTF_GetError());
        } else {
            fonts[fontPath] = loadedFont;
            fontUsageCount[fontPath] = 1;
            pathFont = fontPath;
            font = loadedFont;
        }
    } else {
        font = fonts[fontPath];
        pathFont = fontPath;
        fontUsageCount[fontPath]++;
    }

    // Set initial text
    setText(txt);
    setRenderer(static_cast<SDL_Surface *>(Render::getRenderer()));
}

TextObjectSDL::~TextObjectSDL() {
    if (texture) {
        SDL_FreeSurface(texture);
        texture = nullptr;
    }

    if (font && !pathFont.empty()) {
        fontUsageCount[pathFont]--;
        if (fontUsageCount[pathFont] <= 0) {
            TTF_CloseFont(fonts[pathFont]);
            fonts.erase(pathFont);
            fontUsageCount.erase(pathFont);
        }
    }
}

std::vector<std::string> TextObjectSDL::splitTextByNewlines(const std::string &text) {
    std::vector<std::string> lines;
    std::string currentLine;

    for (char c : text) {
        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
        } else {
            currentLine += c;
        }
    }
    if (!currentLine.empty() || text.back() == '\n') {
        lines.push_back(currentLine);
    }

    return lines;
}

void TextObjectSDL::updateTexture() {
    if (!font || !renderer || text.empty()) return;

    // Clean up old texture
    if (texture) {
        SDL_FreeSurface(texture);
        texture = nullptr;
    }

    // Convert color from integer to SDL_Color
    SDL_Color sdlColor = {
        (Uint8)((color >> 24) & 0xFF), // R
        (Uint8)((color >> 16) & 0xFF), // G
        (Uint8)((color >> 8) & 0xFF),  // B
        (Uint8)(color & 0xFF)          // A
    };

    // Split text into lines
    std::vector<std::string> lines = splitTextByNewlines(text);

    if (lines.empty()) {
        textWidth = 0;
        textHeight = 0;
        return;
    }

    // Get font metrics
    int lineSkip = TTF_FontLineSkip(font);

    // Calculate total dimensions needed
    int maxWidth = 0;
    int totalHeight = lineSkip * lines.size();

    // Find the widest line
    for (const auto &line : lines) {
        if (!line.empty()) {
            int w, h;
            TTF_SizeUTF8(font, line.c_str(), &w, &h);
            maxWidth = std::max(maxWidth, w);
        }
    }

    // Handle case where all lines are empty
    if (maxWidth == 0) {
        int w, h;
        TTF_SizeUTF8(font, " ", &w, &h);
        maxWidth = 1;
        totalHeight = h * lines.size();
    }

    // Create a surface to compose all lines
    SDL_Surface *compositeSurface = SDL_CreateRGBSurface(
        SDL_HWSURFACE, maxWidth, totalHeight, 32, RMASK, GMASK, BMASK, AMASK);

    // Render each line onto the composite surface
    Sint16 currentY = 0;
    for (const auto &line : lines) {
        if (!line.empty()) {
            SDL_Surface *lineSurface = TTF_RenderUTF8_Blended(font, line.c_str(), sdlColor);
            if (lineSurface) {
                SDL_Rect destRect = {0, currentY, static_cast<Uint16>(lineSurface->w), static_cast<Uint16>(lineSurface->h)};
                SDL_gfxBlitRGBA(lineSurface, nullptr, compositeSurface, &destRect);
                SDL_FreeSurface(lineSurface);
            }
        }
        currentY += lineSkip;
    }

    // Create texture from composite surface
    texture = SDL_DisplayFormatAlpha(compositeSurface);
    memorySize = compositeSurface->w * compositeSurface->h * 4;

    if (!texture) {
        std::cerr << "Failed to create text texture: " << SDL_GetError() << std::endl;
    }

    // Store dimensions
    textWidth = compositeSurface->w;
    textHeight = compositeSurface->h;

    SDL_FreeSurface(compositeSurface);
}
void TextObjectSDL::setColor(int clr) {
    if (color == clr) return;
    TextObject::setColor(clr);
    updateTexture();
}

void TextObjectSDL::setText(std::string txt) {
    if (text == txt) return;
    text = txt;
    updateTexture();
}

void TextObjectSDL::render(int xPos, int yPos) {
    if (!texture || !renderer) return;

    SDL_Surface *surface = texture;
    bool free = false;

    if (scale != 1.0f) {
        surface = zoomSurface(texture, scale, scale, SMOOTHING_OFF);
        if (!surface) {
            surface = texture;
        } else {
            free = true;
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

    if (free) {
        SDL_FreeSurface(surface);
    }
}

std::vector<float> TextObjectSDL::getSize() {
    if (!texture) {
        return {0.0f, 0.0f};
    }

    return {(float)textWidth * scale, (float)textHeight * scale};
}

void TextObjectSDL::setRenderer(void *r) {
    renderer = static_cast<SDL_Surface *>(r);
    updateTexture();
}

void TextObjectSDL::cleanupText() {
    for (auto &[fontPath, font] : fonts) {
        if (font) {
            TTF_CloseFont(font);
        }
    }

    // Clear the maps
    fonts.clear();
    fontUsageCount.clear();

    Log::log("Cleaned up all text.");
}
