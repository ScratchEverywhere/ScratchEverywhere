#include "text_sdl.hpp"
TextObjectSDL::TextObjectSDL(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {

    // Load font
    if (fontPath.empty()) {
#ifdef __WIIU__
        font = TTF_OpenFont("romfs:/gfx/Arialn.ttf", 24);
#else
        font = TTF_OpenFont("gfx/Arialn.ttf", 24);
#endif
        if (!font) {
            std::cerr << "Failed to load default font: " << TTF_GetError() << std::endl;
        }
    } else {
        font = TTF_OpenFont(fontPath.c_str(), 24);
        if (!font) {
            std::cerr << "Failed to load font " << fontPath << ": " << TTF_GetError() << std::endl;
        }
    }

    // Set initial text
    setText(txt);
}

TextObjectSDL::~TextObjectSDL() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
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
    texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!texture) {
        std::cerr << "Failed to create text texture: " << SDL_GetError() << std::endl;
    }

    // Store dimensions
    textWidth = textSurface->w;
    textHeight = textSurface->h;

    SDL_FreeSurface(textSurface);
}

void TextObjectSDL::setColor(int clr) {
    TextObject::setColor(clr);
    updateTexture();
}

void TextObjectSDL::setText(std::string txt) {
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