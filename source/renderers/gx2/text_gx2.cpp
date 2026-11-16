#include "text_gx2.hpp"
#include "gx2_gfx.hpp"

#include <cstdint>
#include <log.hpp>

static constexpr float nominalFontSize = 33.3f;

TextObjectGX2::TextObjectGX2(std::string txt, double posX, double posY, std::string fontPath)
    : TextObjectBase(txt, posX, posY, fontPath, nominalFontSize) {
}

void TextObjectGX2::uploadAtlas(FontGeneration &gen) {
    GX2Gfx::Texture *texture = static_cast<GX2Gfx::Texture *>(gen.backendHandle);
    if (!texture) {
        texture = new GX2Gfx::Texture();
        gen.backendHandle = texture;
        gen.destroyBackendHandle = [](void *handle) {
            GX2Gfx::Texture *t = static_cast<GX2Gfx::Texture *>(handle);
            GX2Gfx::destroyTexture(*t);
            delete t;
        };
    }

    if (!GX2Gfx::replaceTexture(*texture, gen.atlasWidth, gen.atlasHeight, GX2_SURFACE_FORMAT_UNORM_R8,
                                gen.pixels.data(), gen.atlasWidth)) {
        Log::logError("[GX2 Text] Failed to upload the font atlas");
        return;
    }

    gen.dirty = false;
}

void TextObjectGX2::render(int xPos, int yPos) {
    if (!fontAtlas || !fontAtlas->isValid() || layoutLines.empty()) return;

    FontGeneration &gen = touchGeneration();
    if (gen.dirty) uploadAtlas(gen);

    const GX2Gfx::Texture *atlas = static_cast<GX2Gfx::Texture *>(gen.backendHandle);
    if (!atlas || !atlas->valid) return;

    size_t glyphCount = 0;
    for (const auto &line : layoutLines)
        glyphCount += line.size();
    if (glyphCount == 0) return;

    int viewportWidth, viewportHeight;
    GX2Gfx::getViewportSize(viewportWidth, viewportHeight);
    float proj[16];
    GX2Gfx::buildOrtho(proj, 0.0f, static_cast<float>(viewportWidth), static_cast<float>(viewportHeight), 0.0f);

    const float color4[4] = {
        ((color >> 24) & 0xFF) / 255.0f,
        ((color >> 16) & 0xFF) / 255.0f,
        ((color >> 8) & 0xFF) / 255.0f,
        (color & 0xFF) / 255.0f};

    float drawX = static_cast<float>(xPos);
    float drawY = static_cast<float>(yPos);
    if (centerAligned) {
        drawX -= layoutWidth / 2.0f;
        drawY -= layoutHeight / 2.0f;
    }

    const float lineHeight = (gen.ascent - gen.descent + gen.lineGap) * glyphScale;

    const size_t bytes = glyphCount * 4 * 4 * sizeof(float);
    float *verts = static_cast<float *>(GX2Gfx::streamAllocate(bytes));
    if (!verts) return;

    float *out = verts;
    for (size_t li = 0; li < layoutLines.size(); ++li) {
        const float lineX = drawX;
        const float lineY = drawY + static_cast<float>(li) * lineHeight + gen.ascent * glyphScale;

        for (const GlyphQuad &q : layoutLines[li]) {
            const float qx0 = lineX + q.x0 * glyphScale;
            const float qy0 = lineY + q.y0 * glyphScale;
            const float qx1 = lineX + q.x1 * glyphScale;
            const float qy1 = lineY + q.y1 * glyphScale;

            const float quad[16] = {
                qx0, qy0, q.s0, q.t0,
                qx1, qy0, q.s1, q.t0,
                qx1, qy1, q.s1, q.t1,
                qx0, qy1, q.s0, q.t1};
            for (int i = 0; i < 16; ++i)
                out[i] = quad[i];
            out += 16;
        }
    }

    const GX2Gfx::TextProgram &program = GX2Gfx::textProgram();
    GX2Gfx::bindProgram(program);
    GX2Gfx::setBlend(GX2Gfx::Blend::Alpha);
    GX2Gfx::setVertexUniform(program.uProjection, proj, 16);
    GX2Gfx::setPixelUniform(program.uColor, color4, 4);
    GX2Gfx::bindTexture(*atlas, program.samplerLocation, GX2Gfx::Filter::Linear);

    GX2Gfx::drawStream(verts, bytes, 4 * sizeof(float), GX2_PRIMITIVE_MODE_QUADS, static_cast<uint32_t>(glyphCount * 4));
}
