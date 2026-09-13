#include "utf8.hpp"
#include <cstdio>
#include <log.hpp>
#include <os.hpp>
#include <string>
#include <text.hpp>

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(romfs);
#endif

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
#elif defined(RENDERER_OPENGL_CORE)
#include <renderers/opengl_core/text_gl_core.hpp>
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
#elif defined(RENDERER_OPENGL_CORE)
    return std::make_unique<TextObjectGLCore>(txt, posX, posY, fontPath);
#elif defined(RENDERER_HEADLESS)
    return std::make_unique<TextObjectHeadless>(txt, posX, posY, fontPath);
#else
    return nullptr;
#endif
}

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
#ifndef RENDERER_HEADLESS
    TextObjectBase::cleanupText();
#endif
}

static std::vector<unsigned char> readFontFile(const std::string &fullPath) {
#ifdef USE_CMAKERC
    auto fs = cmrc::romfs::get_filesystem();
    if (!fs.exists(fullPath)) {
        Log::logError("Failed to open font file: " + fullPath);
        return {};
    }
    auto file = fs.open(fullPath);
    return std::vector<unsigned char>(file.begin(), file.end());
#else
    FILE *f = fopen(fullPath.c_str(), "rb");
    if (!f) {
        Log::logError("Failed to open font file: " + fullPath);
        return {};
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> buffer((size_t)size);
    if (fread(buffer.data(), 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        Log::logError("Failed to read font file: " + fullPath);
        return {};
    }
    fclose(f);
    return buffer;
#endif
}

static std::vector<std::string> splitByNewlines(const std::string &text) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    lines.push_back(current);
    return lines;
}

TextObjectBase::TextObjectBase(std::string txt, double posX, double posY, std::string fontPath, float nominalSize)
    : TextObject(txt, posX, posY, fontPath), nominalPixelSize(nominalSize) {
    if (loadFont(fontPath)) {
        relayout();
    }
}

TextObjectBase::~TextObjectBase() = default;

bool TextObjectBase::loadFont(std::string fontPath) {
    if (fontPath.empty()) fontPath = "gfx/ingame/fonts/NotoSans-Medium";
    std::string fullPath = OS::getRomFSLocation() + fontPath + ".ttf";

    fontAtlas = FontManager::acquire(fullPath);
    if (!fontAtlas->isValid()) {
        auto buffer = readFontFile(fullPath);
        if (buffer.empty()) return false;
        if (!fontAtlas->loadFromMemory(std::move(buffer))) return false;
    }
    return true;
}

void TextObjectBase::relayout() {
    layoutLines.clear();
    layoutWidth = 0;
    layoutHeight = 0;
    glyphScale = 1.0f;

    if (!fontAtlas || !fontAtlas->isValid()) return;

    std::vector<std::string> rawLines = splitByNewlines(text);

    std::vector<std::vector<uint32_t>> codepointLines;
    codepointLines.reserve(rawLines.size());
    std::vector<uint32_t> allCodepoints;
    for (const auto &line : rawLines) {
        auto cps = utf8::decode(line);
        allCodepoints.insert(allCodepoints.end(), cps.begin(), cps.end());
        codepointLines.push_back(std::move(cps));
    }

    float requestedPixelSize = nominalPixelSize * scale;
    if (requestedPixelSize < 1.0f) requestedPixelSize = 1.0f;

    fontBucket = fontAtlas->pickBucket(requestedPixelSize, fontBucket);
    FontGeneration &gen = fontAtlas->ensureGeneration(fontBucket, allCodepoints);

    glyphScale = requestedPixelSize / (float)gen.pixelSize;
    const float lineHeightPx = gen.ascent - gen.descent + gen.lineGap;

    layoutLines.reserve(codepointLines.size());
    float maxWidth = 0;

    for (const auto &cps : codepointLines) {
        std::vector<GlyphQuad> glyphs;
        glyphs.reserve(cps.size());
        float penX = 0, penY = 0;

        for (uint32_t cp : cps) {
            GlyphQuad q;
            fontAtlas->getGlyphQuad(gen, cp, penX, penY, q);
            if (q.valid) glyphs.push_back(q);
        }

        if (penX > maxWidth) maxWidth = penX;
        layoutLines.push_back(std::move(glyphs));
    }

    layoutWidth = maxWidth * glyphScale;
    layoutHeight = lineHeightPx * (float)codepointLines.size() * glyphScale;
}

FontGeneration &TextObjectBase::touchGeneration() {
    static const std::vector<uint32_t> empty;
    return fontAtlas->ensureGeneration(fontBucket, empty);
}

void TextObjectBase::setText(std::string txt) {
    if (text == txt) return;
    text = txt;
    relayout();
}

void TextObjectBase::setScale(float scl) {
    if (scale == scl) return;
    scale = scl;
    relayout();
}

std::vector<float> TextObjectBase::getSize() {
    return {layoutWidth, layoutHeight};
}

std::vector<float> TextObjectBase::getStringSize(const std::string &txt) {
    const std::string oldText = text;
    text = txt;
    relayout();
    std::vector<float> size = getSize();
    text = oldText;
    relayout();
    return size;
}

void TextObjectBase::cleanupText() {
    FontManager::cleanup();
}
