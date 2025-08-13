#include "text_sdl.hpp"
#include "../scratch/render.hpp"

std::unordered_map<std::string, TTF_Font *> TextObjectSDL::fonts;

TextObjectSDL::TextObjectSDL(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {

    // get font
    if (fontPath.empty()) {
        fontPath = "gfx/Arialn.ttf";
    }
#if defined(__WIIU__) || defined(__OGC__)
    fontPath = "romfs:/" + fontPath;
#endif

    // open font if not loaded
    if (fonts.find(fontPath) == fonts.end()) {
        TTF_Font *loadedFont = TTF_OpenFont(fontPath.c_str(), 30);
        if (!loadedFont) {
            std::cerr << "Failed to load font " << fontPath << ": " << TTF_GetError() << std::endl;
        } else {
            fonts[fontPath] = loadedFont;
        }
        font = loadedFont;
    } else {
        font = fonts[fontPath];
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

    // Create surface from text
    SDL_Surface *textSurface = TTF_RenderText_Blended(font, text.c_str(), sdlColor);
    if (!textSurface) {
        std::cerr << "Failed to create text surface: " << TTF_GetError() << std::endl;
        return;
    }

    // Create texture from surface
    MemoryTracker::deallocateVRAM(memorySize);
    texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    memorySize = textSurface->w * textSurface->h * 4;
    MemoryTracker::allocateVRAM(memorySize);
    if (!texture) {
        std::cerr << "Failed to create text texture: " << SDL_GetError() << std::endl;
    }

    // Store dimensions
    textWidth = textSurface->w;
    textHeight = textSurface->h;

    SDL_FreeSurface(textSurface);
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