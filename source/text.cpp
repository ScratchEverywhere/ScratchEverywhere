#include <string>
#include <text.hpp>
#ifdef RENDERER_CITRO2D
#include <renderers/citro2d/text_c2d.hpp>
#elif defined(RENDERER_SDL2)
#include <renderers/sdl2/text_sdl2.hpp>
#elif defined(RENDERER_SDL3)
#include <renderers/sdl3/text_sdl3.hpp>
#elif defined(RENDERER_SDL1)
#include <renderers/sdl1/text_sdl1.hpp>
#elif defined(RENDERER_OPENGL)
#include <renderers/opengl/text_gl.hpp>
#elif defined(RENDERER_GL2D)
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

// stolen from SpeechText
std::string TextObject::wrap(int maxWidth) {
    const std::string text = getText();
    if (text.empty()) {
        return text;
    }

    std::string result;
    std::string currentLine;
    std::string currentWord;

    for (char c : text) {
        if (c == '\n') {
            if (!currentWord.empty()) {
                currentLine += currentWord;
                currentWord.clear();
            }
            if (!currentLine.empty()) {
                result += currentLine + "\n";
                currentLine.clear();
            } else {
                result += "\n";
            }
        } else if (c == ' ') { // add new line at space to wrap cleanly (without splitting words in half)
            if (!currentWord.empty()) {
                std::string line = currentLine.empty() ? currentWord : currentLine + " " + currentWord;

                float width = getStringSize(line)[0];

                if (width <= maxWidth) {
                    currentLine = line;
                } else {
                    if (!currentLine.empty()) {
                        result += currentLine + "\n";
                    }
                    currentLine = currentWord;
                }
                currentWord.clear();
            }
        } else {
            currentWord += c;
        }
    }

    // Handle the last word
    if (!currentWord.empty()) {
        std::string line = currentLine.empty() ? currentWord : currentLine + " " + currentWord;

        float width = getStringSize(line)[0];

        if (width <= maxWidth) {
            currentLine = line;
        } else {
            if (!currentLine.empty()) {
                result += currentLine + "\n";
            }
            currentLine = currentWord;
        }
    }

    if (!currentLine.empty()) {
        result += currentLine;
    }

    return result;
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
