#include "text_gl.hpp"
#include "render_opengl.hpp"
#include <cstdint>

static constexpr float nominalFontSize = 33.3f;

TextObjectGL::TextObjectGL(std::string txt, double posX, double posY, std::string fontPath)
    : TextObjectBase(txt, posX, posY, fontPath, nominalFontSize) {
}

void TextObjectGL::uploadAtlas(FontGeneration &gen) {
    GLuint texId;
    if (gen.backendHandle) {
        texId = (GLuint)(uintptr_t)gen.backendHandle;
    } else {
        glGenTextures(1, &texId);
        gen.backendHandle = (void *)(uintptr_t)texId;
        gen.destroyBackendHandle = [](void *handle) {
            GLuint id = (GLuint)(uintptr_t)handle;
            glDeleteTextures(1, &id);
        };
    }

    glBindTexture(GL_TEXTURE_2D, texId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, gen.atlasWidth, gen.atlasHeight, 0, GL_ALPHA, GL_UNSIGNED_BYTE, gen.pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gen.dirty = false;
}

void TextObjectGL::render(int xPos, int yPos) {
    if (!fontAtlas || !fontAtlas->isValid() || layoutLines.empty()) return;

    FontGeneration &gen = touchGeneration();
    if (gen.dirty) uploadAtlas(gen);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, (GLuint)(uintptr_t)gen.backendHandle);

    glColor4ub((color >> 24) & 0xFF, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);

    float drawX = (float)xPos;
    float drawY = (float)yPos;

    if (centerAligned) {
        drawX -= layoutWidth / 2.0f;
        drawY -= layoutHeight / 2.0f;
    }

    const float lineHeight = (gen.ascent - gen.descent + gen.lineGap) * glyphScale;

    for (size_t i = 0; i < layoutLines.size(); ++i) {
        const auto &glyphs = layoutLines[i];
        if (glyphs.empty()) continue;

        float lineX = drawX;
        float lineY = drawY + (i * lineHeight + gen.ascent * glyphScale);

        glBegin(GL_QUADS);
        for (const GlyphQuad &q : glyphs) {
            float qx0 = lineX + q.x0 * glyphScale;
            float qy0 = lineY + q.y0 * glyphScale;
            float qx1 = lineX + q.x1 * glyphScale;
            float qy1 = lineY + q.y1 * glyphScale;

            glTexCoord2f(q.s0, q.t0);
            glVertex2f(qx0, qy0);
            glTexCoord2f(q.s1, q.t0);
            glVertex2f(qx1, qy0);
            glTexCoord2f(q.s1, q.t1);
            glVertex2f(qx1, qy1);
            glTexCoord2f(q.s0, q.t1);
            glVertex2f(qx0, qy1);
        }
        glEnd();
    }

    glColor4ub(255, 255, 255, 255);
}
