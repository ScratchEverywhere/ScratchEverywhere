#include "text_sdl3.hpp"
#include <iostream>
#include <os.hpp>
#include <ostream>
#include <render.hpp>
#include <string>
#include <text.hpp>
#include <unordered_map>
#include <vector>

#ifdef __PC__
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

std::unordered_map<std::string, TTF_Font *> TextObjectSDL3::fonts;
std::unordered_map<std::string, size_t> TextObjectSDL3::fontUsageCount;

TextObjectSDL3::TextObjectSDL3(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {

    // get font
    if (fontPath.empty()) {
        fontPath = "gfx/menu/LibSansN";
    }
    fontPath = OS::getRomFSLocation() + fontPath;
    fontPath = fontPath + ".ttf";

    // open font if not loaded
    if (fonts.find(fontPath) == fonts.end()) {
#ifdef __PC__
        const auto &file = cmrc::romfs::get_filesystem().open(fontPath);
        TTF_Font *loadedFont = TTF_OpenFontIO(SDL_IOFromConstMem(file.begin(), file.size()), 1, 30);
#else
        TTF_Font *loadedFont = TTF_OpenFont(fontPath.c_str(), 30);
#endif
        if (!loadedFont) {
            Log::logError("Failed to load font " + fontPath + ": " + SDL_GetError());
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
    setRenderer(static_cast<SDL_Renderer *>(Render::getRenderer()));
}

TextObjectSDL3::~TextObjectSDL3() {
    if (texture) {
        SDL_DestroyTexture(texture);
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

std::vector<std::string> TextObjectSDL3::splitTextByNewlines(const std::string &text) {
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

void TextObjectSDL3::updateTexture() {
    if (!font || !renderer || text.empty()) return;

    // Clean up old texture
    if (texture) {
        SDL_DestroyTexture(texture);
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
    int lineSkip = TTF_GetFontLineSkip(font);

    // Calculate total dimensions needed
    int maxWidth = 0;
    int totalHeight = lineSkip * lines.size();

    // Find the widest line
    for (const auto &line : lines) {
        if (!line.empty()) {
            int w, h;
            TTF_GetStringSize(font, line.c_str(), 0, &w, &h);
            maxWidth = std::max(maxWidth, w);
        }
    }

    // Handle case where all lines are empty
    if (maxWidth == 0) {
        int w, h;
        TTF_GetStringSize(font, " ", 1, &w, &h);
        maxWidth = 1;
        totalHeight = h * lines.size();
    }

    // Create a surface to compose all lines
    SDL_Surface *compositeSurface = SDL_CreateSurface(maxWidth, totalHeight, SDL_PIXELFORMAT_RGBA8888);

    if (!compositeSurface) {
        Log::logError("Failed to create composite surface: " + std::string(SDL_GetError()));
        return;
    }

    // Make the surface transparent
    SDL_SetSurfaceBlendMode(compositeSurface, SDL_BLENDMODE_BLEND);
    SDL_FillSurfaceRect(compositeSurface, nullptr, SDL_MapSurfaceRGBA(compositeSurface, sdlColor.r, sdlColor.g, sdlColor.b, 0)); // workaround for the black edges on white text

    // Render each line onto the composite surface
    int currentY = 0;
    for (const auto &line : lines) {
        if (!line.empty()) {
            SDL_Surface *lineSurface = TTF_RenderText_Blended(font, line.c_str(), 0, sdlColor);
            if (lineSurface) {
                SDL_Rect destRect = {0, currentY, lineSurface->w, lineSurface->h};
                SDL_BlitSurface(lineSurface, nullptr, compositeSurface, &destRect);
                SDL_DestroySurface(lineSurface);
            }
        }
        currentY += lineSkip;
    }

    // Create texture from composite surface
    texture = SDL_CreateTextureFromSurface(renderer, compositeSurface);
    memorySize = compositeSurface->w * compositeSurface->h * 4;

    if (!texture) Log::logError("Failed to create text texture: " + std::string(SDL_GetError()));

    // Store dimensions
    textWidth = compositeSurface->w;
    textHeight = compositeSurface->h;

    SDL_DestroySurface(compositeSurface);
}

void TextObjectSDL3::setColor(int clr) {
    if (color == clr) return;
    TextObject::setColor(clr);
    updateTexture();
}

void TextObjectSDL3::setText(std::string txt) {
    if (text == txt) return;
    text = txt;
    updateTexture();
}

void TextObjectSDL3::render(int xPos, int yPos) {
    if (!texture || !renderer) return;

    SDL_FRect destRect;
    destRect.w = textWidth * scale;
    destRect.h = textHeight * scale;

    if (centerAligned) {
        destRect.x = xPos - (destRect.w / 2);
        destRect.y = yPos - (destRect.h / 2);
    } else {
        destRect.x = xPos;
        destRect.y = yPos;
    }

    SDL_RenderTexture(renderer, texture, nullptr, &destRect);
}

std::vector<float> TextObjectSDL3::getSize() {
    if (!texture) {
        return {0.0f, 0.0f};
    }

    return {textWidth * scale, textHeight * scale};
}

void TextObjectSDL3::setRenderer(void *r) {
    renderer = static_cast<SDL_Renderer *>(r);
    updateTexture();
}

void TextObjectSDL3::cleanupText() {
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
