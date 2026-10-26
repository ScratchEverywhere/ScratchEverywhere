#include "text_nv2a.hpp"
#include "renderer_nv2a.hpp"
#include "xgu.h"
#include <runtime.hpp>
#include <vector>

TextObjectNV2A::TextObjectNV2A(std::string txt, double posX, double posY, std::string fontPath)
    : TextObjectBase(txt, posX, posY, fontPath, 30.0f) {
    renderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
}

TextObjectNV2A::~TextObjectNV2A() = default;

void TextObjectNV2A::uploadAtlas(FontGeneration &gen) {
    if (!renderer && Render::getGlobalRenderer()) {
        renderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    }
    if (!renderer) return;

    if (gen.backendHandle) {
        NV2ATexture *oldTex = static_cast<NV2ATexture *>(gen.backendHandle);
        renderer->destroyTexture(oldTex);
        gen.backendHandle = nullptr;
    }

    NV2ATexture *tex = renderer->createTexture(gen.atlasWidth, gen.atlasHeight, XGU_TEXTURE_FORMAT_A8B8G8R8_SWIZZLED, true);
    if (!tex) {
        Scratch::freeUnusedCostumeImages();
        tex = renderer->createTexture(gen.atlasWidth, gen.atlasHeight, XGU_TEXTURE_FORMAT_A8B8G8R8_SWIZZLED, true);
    }

    if (tex) {
        std::vector<uint8_t> rgba(static_cast<size_t>(gen.atlasWidth) * gen.atlasHeight * 4);
        for (size_t i = 0; i < gen.pixels.size(); ++i) {
            rgba[i * 4 + 0] = 255;
            rgba[i * 4 + 1] = 255;
            rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = gen.pixels[i];
        }
        renderer->updateTexture(tex, rgba.data(), gen.atlasWidth * 4);
    } else {
        // Fallback to 16-bit A4R4G4B4 swizzled (half memory)
        tex = renderer->createTexture(gen.atlasWidth, gen.atlasHeight, XGU_TEXTURE_FORMAT_A4R4G4B4_SWIZZLED, true);
        if (tex) {
            std::vector<uint16_t> a4(static_cast<size_t>(gen.atlasWidth) * gen.atlasHeight);
            for (size_t i = 0; i < gen.pixels.size(); ++i) {
                uint16_t a = gen.pixels[i] >> 4;
                a4[i] = static_cast<uint16_t>((a << 12) | 0x0FFF);
            }
            renderer->updateTexture(tex, a4.data(), gen.atlasWidth * 2);
        }
    }

    gen.backendHandle = tex;
    Renderer_NV2A *r = renderer;
    gen.destroyBackendHandle = [r](void *handle) {
        if (r && handle) {
            NV2ATexture *t = static_cast<NV2ATexture *>(handle);
            r->destroyTexture(t);
        }
    };
    gen.dirty = false;
}

void TextObjectNV2A::render(int xPos, int yPos) {
    if (!renderer && Render::getGlobalRenderer()) {
        renderer = static_cast<Renderer_NV2A *>(Render::getGlobalRenderer());
    }
    if (!renderer || !fontAtlas || !fontAtlas->isValid() || layoutLines.empty()) return;

    FontGeneration &gen = touchGeneration();
    if (gen.dirty) uploadAtlas(gen);
    if (!gen.backendHandle) return;

    NV2ATexture *tex = static_cast<NV2ATexture *>(gen.backendHandle);

    uint8_t cr = static_cast<uint8_t>((color >> 24) & 0xFF);
    uint8_t cg = static_cast<uint8_t>((color >> 16) & 0xFF);
    uint8_t cb = static_cast<uint8_t>((color >> 8) & 0xFF);
    uint8_t ca = static_cast<uint8_t>(color & 0xFF);

    float drawX = static_cast<float>(xPos);
    float drawY = static_cast<float>(yPos);
    if (centerAligned) {
        drawX -= layoutWidth / 2.0f;
        drawY -= layoutHeight / 2.0f;
    }

    const float lineHeight = (gen.ascent - gen.descent + gen.lineGap) * glyphScale;

    std::vector<NV2AVertex> verts;

    for (size_t li = 0; li < layoutLines.size(); ++li) {
        const auto &glyphs = layoutLines[li];
        if (glyphs.empty()) continue;

        float lineX = drawX;
        float lineY = drawY + static_cast<float>(li) * lineHeight + gen.ascent * glyphScale;

        for (const GlyphQuad &q : glyphs) {
            float qx0 = lineX + q.x0 * glyphScale;
            float qy0 = lineY + q.y0 * glyphScale;
            float qx1 = lineX + q.x1 * glyphScale;
            float qy1 = lineY + q.y1 * glyphScale;

            // Normalized UV coordinates for the atlas
            float u0 = q.s0 * tex->uScale;
            float v0 = q.t0 * tex->vScale;
            float u1 = q.s1 * tex->uScale;
            float v1 = q.t1 * tex->vScale;

            NV2AVertex v_tl = {{qx0, qy0}, {cr, cg, cb, ca}, {u0, v0}};
            NV2AVertex v_tr = {{qx1, qy0}, {cr, cg, cb, ca}, {u1, v0}};
            NV2AVertex v_br = {{qx1, qy1}, {cr, cg, cb, ca}, {u1, v1}};
            NV2AVertex v_bl = {{qx0, qy1}, {cr, cg, cb, ca}, {u0, v1}};

            // Triangle 1: TL, TR, BR
            verts.push_back(v_tl);
            verts.push_back(v_tr);
            verts.push_back(v_br);

            // Triangle 2: TL, BR, BL
            verts.push_back(v_tl);
            verts.push_back(v_br);
            verts.push_back(v_bl);
        }
    }

    if (!verts.empty()) {
        renderer->bindTexture(tex, false);
        renderer->setBlendMode(NV2ABlendMode::Blend);
        renderer->drawTriangles(verts.data(), verts.size());
    }
}
