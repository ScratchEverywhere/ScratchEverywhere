#include "speech_text_sdl.hpp"
#include "os.hpp"
#include "text.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

SpeechTextObjectSDL::SpeechTextObjectSDL(const std::string &text, int maxWidth)
    : TextObjectSDL(text, 0, 0), SpeechText(text, maxWidth) {
    setColor(0x00);
    setCenterAligned(false); // easier for positioning logic

    if (font && !pathFont.empty()) {
        TextObjectSDL::fontUsageCount[pathFont]--;
        if (TextObjectSDL::fontUsageCount[pathFont] <= 0) {
            TTF_CloseFont(TextObjectSDL::fonts[pathFont]);
            TextObjectSDL::fonts.erase(pathFont);
            TextObjectSDL::fontUsageCount.erase(pathFont);
        }
        font = nullptr;
        pathFont.clear();
    }

    font = TTF_OpenFont((OS::getRomFSLocation() + "gfx/menu/Arialn.ttf").c_str(), 16);
    if (!font) {
        Log::logError("Failed to load speech font " + (OS::getRomFSLocation() + "gfx/menu/Arialn.ttf") + ": " + TTF_GetError());
    }

    platformSetText(wrapText());
}

SpeechTextObjectSDL::~SpeechTextObjectSDL() {
    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
    }
}

float SpeechTextObjectSDL::measureTextWidth(const std::string &text) {
    if (!font) return 0.0f;

    int width, height;
    TTF_SizeUTF8(font, text.c_str(), &width, &height);
    return static_cast<float>(width);
}

void SpeechTextObjectSDL::platformSetText(const std::string &text) {
    TextObjectSDL::setText(text);
}

void SpeechTextObjectSDL::setText(std::string txt) {
    SpeechText::setText(txt);
}