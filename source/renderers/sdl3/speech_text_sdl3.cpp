#include "speech_text_sdl3.hpp"
#include "text.hpp"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <os.hpp>

#ifdef __PC__
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

SpeechTextObjectSDL3::SpeechTextObjectSDL3(const std::string &text, int maxWidth)
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

#ifdef __PC__
    const auto &file = cmrc::romfs::get_filesystem().open(OS::getRomFSLocation() + "gfx/menu/LibSansN.ttf");
    font = TTF_OpenFontIO(SDL_IOFromConstMem(file.begin(), file.size()), 1, 16);
#else
    font = TTF_OpenFont((OS::getRomFSLocation() + "gfx/menu/LibSansN.ttf").c_str(), 16);
#endif
    if (!font) {
        Log::logError("Failed to load speech font " + (OS::getRomFSLocation() + "gfx/menu/LibSansN.ttf") + ": " + SDL_GetError());
    }

    platformSetText(wrapText());
}

SpeechTextObjectSDL3::~SpeechTextObjectSDL3() {
    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
    }
}

float SpeechTextObjectSDL3::measureTextWidth(const std::string &text) {
    if (!font) return 0.0f;

    int width, height;
    TTF_GetStringSize(font, text.c_str(), 0, &width, &height);
    return static_cast<float>(width);
}

void SpeechTextObjectSDL3::platformSetText(const std::string &text) {
    TextObjectSDL::setText(text);
}

void SpeechTextObjectSDL3::setText(std::string txt) {
    SpeechText::setText(txt);
}
