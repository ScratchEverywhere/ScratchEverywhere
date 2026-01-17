#include "speech_text_sdl1.hpp"
#include <os.hpp>
#include "text.hpp"
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

SpeechTextObjectSDL::SpeechTextObjectSDL(const std::string &text, int maxWidth)
    : TextObjectSDL1(text, 0, 0), SpeechText(text, maxWidth) {
    setColor(0x00);
    setCenterAligned(false); // easier for positioning logic

    if (font && !pathFont.empty()) {
        TextObjectSDL1::fontUsageCount[pathFont]--;
        if (TextObjectSDL1::fontUsageCount[pathFont] <= 0) {
            TTF_CloseFont(TextObjectSDL1::fonts[pathFont]);
            TextObjectSDL1::fonts.erase(pathFont);
            TextObjectSDL1::fontUsageCount.erase(pathFont);
        }
        font = nullptr;
        pathFont.clear();
    }

#ifdef USE_CMAKERC
    const auto &file = cmrc::romfs::get_filesystem().open(OS::getRomFSLocation() + "gfx/menu/LibSansN.ttf");
    font = TTF_OpenFontRW(SDL_RWFromConstMem(file.begin(), file.size()), 1, 16);
#else
    font = TTF_OpenFont((OS::getRomFSLocation() + "gfx/menu/LibSansN.ttf").c_str(), 16);
#endif
    if (!font) {
        Log::logError("Failed to load speech font " + (OS::getRomFSLocation() + "gfx/menu/LibSansN.ttf") + ": " + TTF_GetError());
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
    TextObjectSDL1::setText(text);
}

void SpeechTextObjectSDL::setText(std::string txt) {
    SpeechText::setText(txt);
}
