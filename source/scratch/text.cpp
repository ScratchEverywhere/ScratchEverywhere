#include "text.hpp"
#include <string>
#ifdef __3DS__
#include "../3ds/text_3ds.hpp"
#elif defined(RENDERER_SDL2)
#include "../sdl2/text_sdl.hpp"
#elif defined(RENDERER_SDL3)
#include "../sdl3/text_sdl.hpp"
#elif defined(__NDS__)
#include "../nds/text_nds.hpp"
#elif defined(RENDERER_HEADLESS)
#include "../headless/text_headless.hpp"
#endif

TextObject::TextObject(std::string txt, double posX, double posY, std::string fontPath) {
    x = posX;
    y = posY;
    text = txt;
}

TextObject *createTextObject(std::string txt, double posX, double posY, std::string fontPath) {
#ifdef __3DS__
    return new TextObject3DS(txt, posX, posY, fontPath);
#elif defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
    return new TextObjectSDL(txt, posX, posY, fontPath);
#elif defined(__NDS__)
    return new TextObjectNDS(txt, posX, posY, fontPath);
#elif defined(RENDERER_HEADLESS)
    return new HeadlessText(txt, posX, posY, fontPath);
#else
    return nullptr;
#endif
}

void TextObject::cleanupText() {
#ifdef __3DS__
    TextObject3DS::cleanupText();
#elif defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
    TextObjectSDL::cleanupText();
#elif defined(__NDS__)
    TextObjectNDS::cleanupText();
#else

#endif
}
