#include <string>
#include <text.hpp>
#ifdef __3DS__
#include <renderers/citro2d/text_c2d.hpp>
#elif defined(RENDERER_SDL2)
#include <renderers/sdl2/text_sdl2.hpp>
#elif defined(RENDERER_SDL3)
#include <renderers/sdl3/text_sdl3.hpp>
#elif defined(RENDERER_SDL1)
#include <renderers/sdl1/text_sdl1.hpp>
#elif defined(RENDERER_OPENGL)
#include <renderers/opengl/text_gl.hpp>
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

std::unique_ptr<TextObject> createTextObject(std::string txt, double posX, double posY, std::string fontPath) {
#ifdef RENDERER_CITRO2D
    return std::make_unique<TextObjectC2D>(txt, posX, posY, fontPath);
#elif defined(RENDERER_SDL2)
    return std::make_unique<TextObjectSDL2>(txt, posX, posY, fontPath);
#elif defined(RENDERER_SDL3)
    return std::make_unique<TextObjectSDL3>(txt, posX, posY, fontPath);
#elif defined(RENDERER_SDL1)
    return std::make_unique<TextObjectSDL1>(txt, posX, posY, fontPath);
#elif defined(RENDERER_OPENGL)
    return std::make_unique<TextObjectGL>(txt, posX, posY, fontPath);
#elif defined(RENDERER_GL2D)
    return std::make_unique<TextObjectGL2D>(txt, posX, posY, fontPath);
#elif defined(RENDERER_HEADLESS)
    return std::make_unique<TextObjectHeadless>(txt, posX, posY, fontPath);
#else
    return nullptr;
#endif
}

void TextObject::cleanupText() {
#ifdef RENDERER_CITRO2D
    TextObjectC2D::cleanupText();
#elif defined(RENDERER_SDL2)
    TextObjectSDL2::cleanupText();
#elif defined(RENDERER_SDL3)
    TextObjectSDL3::cleanupText();
#elif defined(RENDERER_SDL1)
    TextObjectSDL1::cleanupText();
#elif defined(RENDERER_OPENGL)
    TextObjectGL::cleanupText();
#elif defined(RENDERER_GL2D)
    TextObjectGL2D::cleanupText();
#endif
}
