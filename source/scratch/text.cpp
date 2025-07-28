#include "text.hpp"
#include <iostream>
#ifdef __3DS__
#include "../3ds/text_3ds.hpp"
#elif defined(SDL_BUILD)
#include "../sdl/text_sdl.hpp"
#endif

TextObject::TextObject(std::string txt, double posX, double posY, std::string fontPath) {
    x = posX;
    y = posY;
    text = txt;
}

TextObject *createTextObject(std::string txt, double posX, double posY, std::string fontPath) {
#ifdef __3DS__
    return new TextObject3DS(txt, posX, posY, fontPath);
#elif defined(SDL_BUILD)
    return new TextObjectSDL(txt, posX, posY, fontPath);
#else
    return nullptr;
#endif
}