#include "text_gl_core.hpp"
#include "render_opengl_core.hpp"
#include <cstdint>
#include <log.hpp>
#include <vector>

static GLuint textProgram = 0;

static const char *textVertSrc = R"glsl(
#version 410 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
uniform mat4 u_projection;
void main() {
    gl_Position = u_projection * vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
}
)glsl";

static const char *textFragSrc = R"glsl(
#version 410 core
in  vec2 v_uv;
out vec4 frag_color;
uniform sampler2D u_tex;
uniform vec4      u_color;
void main() {
    float alpha = texture(u_tex, v_uv).r;
    frag_color = vec4(u_color.rgb, u_color.a * alpha);
}
)glsl";

static GLuint compileTextShader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        Log::logError(std::string("[GL Core Text] Shader error: ") + log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static void ensureTextProgram() {
    if (textProgram) return;

    GLuint v = compileTextShader(GL_VERTEX_SHADER, textVertSrc);
    GLuint f = compileTextShader(GL_FRAGMENT_SHADER, textFragSrc);
    textProgram = glCreateProgram();
    glAttachShader(textProgram, v);
    glAttachShader(textProgram, f);
    glLinkProgram(textProgram);
    glDeleteShader(v);
    glDeleteShader(f);
}

static void buildOrthoText(float out[16], float l, float r, float b, float t) {
    for (int i = 0; i < 16; ++i)
        out[i] = 0.0f;
    out[0] = 2.0f / (r - l);
    out[5] = 2.0f / (t - b);
    out[10] = -1.0f;
    out[12] = -(r + l) / (r - l);
    out[13] = -(t + b) / (t - b);
    out[15] = 1.0f;
}

static constexpr float nominalFontSize = 33.3f;

TextObjectGLCore::TextObjectGLCore(std::string txt, double posX, double posY, std::string fontPath)
    : TextObjectBase(txt, posX, posY, fontPath, nominalFontSize) {
}

void TextObjectGLCore::uploadAtlas(FontGeneration &gen) {
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
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, gen.atlasWidth, gen.atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, gen.pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gen.dirty = false;
}

void TextObjectGLCore::render(int xPos, int yPos) {
    if (!fontAtlas || !fontAtlas->isValid() || layoutLines.empty()) return;

    ensureTextProgram();

    FontGeneration &gen = touchGeneration();
    if (gen.dirty) uploadAtlas(gen);

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    float proj[16];
    buildOrthoText(proj, 0.0f, (float)vp[2], (float)vp[3], 0.0f);

    float cr = ((color >> 24) & 0xFF) / 255.0f;
    float cg = ((color >> 16) & 0xFF) / 255.0f;
    float cb = ((color >> 8) & 0xFF) / 255.0f;
    float ca = (color & 0xFF) / 255.0f;

    float drawX = (float)xPos;
    float drawY = (float)yPos;
    if (centerAligned) {
        drawX -= layoutWidth / 2.0f;
        drawY -= layoutHeight / 2.0f;
    }

    const float lineHeight = (gen.ascent - gen.descent + gen.lineGap) * glyphScale;

    glUseProgram(textProgram);
    glUniformMatrix4fv(glGetUniformLocation(textProgram, "u_projection"), 1, GL_FALSE, proj);
    glUniform1i(glGetUniformLocation(textProgram, "u_tex"), 0);
    glUniform4f(glGetUniformLocation(textProgram, "u_color"), cr, cg, cb, ca);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)(uintptr_t)gen.backendHandle);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (size_t li = 0; li < layoutLines.size(); ++li) {
        const auto &glyphs = layoutLines[li];
        if (glyphs.empty()) continue;

        float lineX = drawX;
        float lineY = drawY + (float)li * lineHeight + gen.ascent * glyphScale;

        std::vector<float> verts;
        verts.reserve(glyphs.size() * 4 * 4);
        std::vector<GLuint> indices;
        indices.reserve(glyphs.size() * 6);
        GLuint vi = 0;

        for (const GlyphQuad &q : glyphs) {
            float qx0 = lineX + q.x0 * glyphScale;
            float qy0 = lineY + q.y0 * glyphScale;
            float qx1 = lineX + q.x1 * glyphScale;
            float qy1 = lineY + q.y1 * glyphScale;

            verts.insert(verts.end(), {qx0, qy0, q.s0, q.t0,
                                       qx1, qy0, q.s1, q.t0,
                                       qx1, qy1, q.s1, q.t1,
                                       qx0, qy1, q.s0, q.t1});
            indices.insert(indices.end(), {vi, vi + 1, vi + 2, vi + 2, vi + 3, vi});
            vi += 4;
        }

        GLuint vao = 0, vbo = 0, ebo = 0;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

        glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, nullptr);

        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);
    }
}
