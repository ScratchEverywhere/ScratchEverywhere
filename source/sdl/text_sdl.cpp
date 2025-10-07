#include "text_sdl.hpp"
#include "../scratch/render.hpp"
#include "os.hpp"
#include "text.hpp"
#include <iostream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

std::unordered_map<std::string, TTF_Font *> TextObjectSDL::fonts;
std::unordered_map<std::string, size_t> TextObjectSDL::fontUsageCount;

TextObjectSDL::TextObjectSDL(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {

    // get font
    if (fontPath.empty()) {
        fontPath = "gfx/menu/Arialn";
    }
    fontPath = OS::getRomFSLocation() + fontPath;
    fontPath = fontPath + ".ttf";

    // open font if not loaded
    if (fonts.find(fontPath) == fonts.end()) {
        TTF_Font *loadedFont = TTF_OpenFont(fontPath.c_str(), 30);
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
    setRenderer(static_cast<SDL_Renderer *>(Render::getRenderer()));
}

TextObjectSDL::~TextObjectSDL() {
    if (texture) {
        MemoryTracker::deallocateVRAM(memorySize);
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
    SDL_Surface *compositeSurface = SDL_CreateRGBSurfaceWithFormat(
        0, maxWidth, totalHeight, 32, SDL_PIXELFORMAT_RGBA8888);

    if (!compositeSurface) {
        std::cerr << "Failed to create composite surface: " << SDL_GetError() << std::endl;
        return;
    }

    // Make the surface transparent
    SDL_SetSurfaceBlendMode(compositeSurface, SDL_BLENDMODE_BLEND);
    SDL_FillRect(compositeSurface, nullptr, SDL_MapRGBA(compositeSurface->format, 0, 0, 0, 0));

    // Render each line onto the composite surface
    int currentY = 0;
    for (const auto &line : lines) {
        if (!line.empty()) {
            SDL_Surface *lineSurface = TTF_RenderUTF8_Blended(font, line.c_str(), sdlColor);
            if (lineSurface) {
                SDL_Rect destRect = {0, currentY, lineSurface->w, lineSurface->h};
                SDL_BlitSurface(lineSurface, nullptr, compositeSurface, &destRect);
                SDL_FreeSurface(lineSurface);
            }
        }
        currentY += lineSkip;
    }

    // Create texture from composite surface
    MemoryTracker::deallocateVRAM(memorySize);
    texture = SDL_CreateTextureFromSurface(renderer, compositeSurface);
    memorySize = compositeSurface->w * compositeSurface->h * 4;
    MemoryTracker::allocateVRAM(memorySize);

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

    SDL_Rect destRect;
    destRect.w = (int)(textWidth * scale);
    destRect.h = (int)(textHeight * scale);

    if (centerAligned) {
        destRect.x = xPos - (destRect.w / 2);
        destRect.y = yPos - (destRect.h / 2);
    } else {
        destRect.x = xPos;
        destRect.y = yPos;
    }

    SDL_RenderCopy(renderer, texture, nullptr, &destRect);
}

std::vector<float> TextObjectSDL::getSize() {
    if (!texture) {
        return {0.0f, 0.0f};
    }

    return {textWidth * scale, textHeight * scale};
}

void TextObjectSDL::setRenderer(void *r) {
    renderer = static_cast<SDL_Renderer *>(r);
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
