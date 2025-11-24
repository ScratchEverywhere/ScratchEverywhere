#include <string>
#include <text.hpp>
#ifdef __3DS__
#include <renderers/citro2d/text_c2d.hpp>
#elif defined(RENDERER_SDL2)
#include <renderers/sdl2/text_sdl2.hpp>
#elif defined(RENDERER_SDL3)
#include <renderers/sdl3/text_sdl3.hpp>
#elif defined(__NDS__)
#include <renderers/gl2d/text_gl2d.hpp>
#elif defined(RENDERER_HEADLESS)
#include <renderers/headless/text_headless.hpp>
#endif

TextObject::TextObject(std::string txt, double posX, double posY, std::string fontPath) {
    x = posX;
    y = posY;
    text = txt;
}

TextObject *createTextObject(std::string txt, double posX, double posY, std::string fontPath) {
#ifdef __3DS__
    return new TextObjectC2D(txt, posX, posY, fontPath);
#elif defined(RENDERER_SDL2)
    return new TextObjectSDL2(txt, posX, posY, fontPath);
#elif defined(RENDERER_SDL3)
    return new TextObjectSDL3(txt, posX, posY, fontPath);
#elif defined(__NDS__)
    return new TextObjectGL2D(txt, posX, posY, fontPath);
#elif defined(RENDERER_HEADLESS)
    return new TextObjectHeadless(txt, posX, posY, fontPath);
#else
    return nullptr;
#endif
}

void TextObject::cleanupText() {
#ifdef __3DS__
    TextObjectC2D::cleanupText();
#elif defined(RENDERER_SDL2)
    TextObjectSDL2::cleanupText();
#elif defined(RENDERER_SDL3)
    TextObjectSDL3::cleanupText();
#elif defined(__NDS__)
    TextObjectGL2D::cleanupText();
#endif
}
