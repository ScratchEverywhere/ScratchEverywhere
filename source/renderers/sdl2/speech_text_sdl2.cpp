#include "speech_text_sdl2.hpp"
#include "text.hpp"
#include <SDL2/SDL.h>
#include <SDL_ttf.h>
#include <os.hpp>

SpeechTextObjectSDL2::SpeechTextObjectSDL2(const std::string &text, int maxWidth)
    : TextObjectSDL2(text, 0, 0), SpeechText(text, maxWidth) {
    setColor(0x00);
    setCenterAligned(false); // easier for positioning logic

    if (font && !pathFont.empty()) {
        TextObjectSDL2::fontUsageCount[pathFont]--;
        if (TextObjectSDL2::fontUsageCount[pathFont] <= 0) {
            TTF_CloseFont(TextObjectSDL2::fonts[pathFont]);
            TextObjectSDL2::fonts.erase(pathFont);
            TextObjectSDL2::fontUsageCount.erase(pathFont);
        }
        font = nullptr;
        pathFont.clear();
    }

    font = TTF_OpenFont((OS::getRomFSLocation() + "gfx/menu/LibSansN.ttf").c_str(), 16);
    if (!font) {
        Log::logError("Failed to load speech font " + (OS::getRomFSLocation() + "gfx/menu/LibSansN.ttf") + ": " + TTF_GetError());
    }

    platformSetText(wrapText());
}

SpeechTextObjectSDL2::~SpeechTextObjectSDL2() {
    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
    }
}

float SpeechTextObjectSDL2::measureTextWidth(const std::string &text) {
    if (!font) return 0.0f;

    int width, height;
    TTF_SizeUTF8(font, text.c_str(), &width, &height);
    return static_cast<float>(width);
}

void SpeechTextObjectSDL2::platformSetText(const std::string &text) {
    TextObjectSDL2::setText(text);
}

void SpeechTextObjectSDL2::setText(std::string txt) {
    SpeechText::setText(txt);
}