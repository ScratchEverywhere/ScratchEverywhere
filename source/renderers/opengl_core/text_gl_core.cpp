#include "text_gl_core.hpp"
#include "render_opengl_core.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <log.hpp>
#include <math.hpp>
#include <os.hpp>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(romfs);
#endif

static GLuint textProgram = 0;
static GLuint textVAO = 0;

static const char *kTextVert = R"glsl(
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

static const char *kTextFrag = R"glsl(
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

    GLuint v = compileTextShader(GL_VERTEX_SHADER, kTextVert);
    GLuint f = compileTextShader(GL_FRAGMENT_SHADER, kTextFrag);
    textProgram = glCreateProgram();
    glAttachShader(textProgram, v);
    glAttachShader(textProgram, f);
    glLinkProgram(textProgram);
    glDeleteShader(v);
    glDeleteShader(f);

    glGenVertexArrays(1, &textVAO);
}

std::unordered_map<std::string, FontDataCore *> TextObjectGLCore::fonts;

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

static std::vector<std::string> splitLines(const std::string &text) {
    std::vector<std::string> lines;
    std::string cur;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else cur += c;
    }
    lines.push_back(cur);
    return lines;
}

TextObjectGLCore::TextObjectGLCore(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {

    if (fontPath.empty()) fontPath = "gfx/ingame/fonts/NotoSans-Medium";
    std::string fullPath = OS::getRomFSLocation() + fontPath + ".ttf";

    if (loadFont(fullPath)) {
        setText(txt);
    }
}

TextObjectGLCore::~TextObjectGLCore() {
    if (!font) return;
    font->usageCount--;
    if (font->usageCount == 0) {
        glDeleteTextures(1, &font->textureID);
        free(font->charData);
        fonts.erase(font->fontName);
        delete font;
    }
    font = nullptr;
}

bool TextObjectGLCore::loadFont(std::string fontPath) {
    auto it = fonts.find(fontPath);
    if (it != fonts.end()) {
        font = it->second;
        font->usageCount++;
        setDimensions();
        return true;
    }

#ifdef USE_CMAKERC
    const auto &file = cmrc::romfs::get_filesystem().open(fontPath);
    auto *fontBuffer = (unsigned char *)malloc(file.size() + 1);
    std::copy(file.begin(), file.end(), fontBuffer);
    size_t size = file.size();
#else
    FILE *fontFile = fopen(fontPath.c_str(), "rb");
    if (!fontFile) {
        Log::logError("[GL Core Text] Failed to open font: " + fontPath);
        return false;
    }
    fseek(fontFile, 0, SEEK_END);
    size_t size = (size_t)ftell(fontFile);
    fseek(fontFile, 0, SEEK_SET);
    auto *fontBuffer = (unsigned char *)malloc(size);
    fread(fontBuffer, size, 1, fontFile);
    fclose(fontFile);
#endif

    if (!fontBuffer) return false;

    font = new FontDataCore();
    font->fontName = fontPath;
    font->atlasWidth = 512;
    font->atlasHeight = 512;
    font->fontSize = 33.3f;
    font->firstChar = 32;
    font->numChars = 96;
    font->usageCount = 1;
    font->charData = (stbtt_bakedchar *)malloc(sizeof(stbtt_bakedchar) * 96);

    auto *bitmap = (unsigned char *)malloc(font->atlasWidth * font->atlasHeight);
    if (!bitmap) {
        free(fontBuffer);
        free(font->charData);
        delete font;
        font = nullptr;
        return false;
    }

    stbtt_BakeFontBitmap(fontBuffer, 0, font->fontSize,
                         bitmap, font->atlasWidth, font->atlasHeight,
                         font->firstChar, font->numChars, font->charData);

    stbtt_fontinfo info;
    if (stbtt_InitFont(&info, fontBuffer, 0)) {
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        float sc = stbtt_ScaleForPixelHeight(&info, font->fontSize);
        font->ascent = (float)ascent * sc;
        font->descent = (float)descent * sc;
        font->lineGap = (float)lineGap * sc;
    } else {
        font->ascent = font->fontSize * 0.8f;
        font->descent = -font->fontSize * 0.2f;
        font->lineGap = 0.0f;
    }
    free(fontBuffer);

    ensureTextProgram();
    glGenTextures(1, &font->textureID);
    glBindTexture(GL_TEXTURE_2D, font->textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 font->atlasWidth, font->atlasHeight,
                 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    free(bitmap);
    fonts[fontPath] = font;
    return true;
}

void TextObjectGLCore::setDimensions() {
    if (!font) {
        width = height = minY = 0;
        return;
    }

    auto lines = splitLines(text);
    float lineHeight = font->ascent - font->descent + font->lineGap;
    float maxWidth = 0;

    for (const auto &line : lines) {
        float x = 0, y = 0;
        for (unsigned char c : line) {
            if (c < font->firstChar || c >= font->firstChar + font->numChars) c = 'x';
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font->charData, font->atlasWidth, font->atlasHeight,
                               c - font->firstChar, &x, &y, &q, 1);
        }
        if (x > maxWidth) maxWidth = x;
    }

    width = maxWidth;
    height = lineHeight * (float)lines.size();
    minY = font->ascent;
}

void TextObjectGLCore::setText(std::string txt) {
    text = txt;
    setDimensions();
}

void TextObjectGLCore::render(int xPos, int yPos) {
    if (!font) return;

    ensureTextProgram();

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
        drawX -= (width * scale) / 2.0f;
        drawY -= (height * scale) / 2.0f;
    }

    auto lines = splitLines(text);
    float lineHeight = font->ascent - font->descent + font->lineGap;

    glUseProgram(textProgram);
    glUniformMatrix4fv(glGetUniformLocation(textProgram, "u_projection"), 1, GL_FALSE, proj);
    glUniform1i(glGetUniformLocation(textProgram, "u_tex"), 0);
    glUniform4f(glGetUniformLocation(textProgram, "u_color"), cr, cg, cb, ca);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->textureID);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (size_t li = 0; li < lines.size(); ++li) {
        float x = 0.0f, y = 0.0f;
        float lineX = drawX;
        float lineY = drawY + ((float)li * lineHeight + font->ascent) * scale;

        std::vector<float> verts;
        verts.reserve(lines[li].size() * 4 * 4);
        std::vector<GLuint> indices;
        indices.reserve(lines[li].size() * 6);
        GLuint vi = 0;

        for (unsigned char c : lines[li]) {
            if (c < font->firstChar || c >= font->firstChar + font->numChars) c = 'x';

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font->charData, font->atlasWidth, font->atlasHeight,
                               c - font->firstChar, &x, &y, &q, 1);

            float qx0 = lineX + q.x0 * scale;
            float qy0 = lineY + q.y0 * scale;
            float qx1 = lineX + q.x1 * scale;
            float qy1 = lineY + q.y1 * scale;

            verts.insert(verts.end(), {qx0, qy0, q.s0, q.t0,
                                       qx1, qy0, q.s1, q.t0,
                                       qx1, qy1, q.s1, q.t1,
                                       qx0, qy1, q.s0, q.t1});
            indices.insert(indices.end(), {vi, vi + 1, vi + 2, vi + 2, vi + 3, vi});
            vi += 4;
        }

        if (verts.empty()) continue;

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

std::vector<float> TextObjectGLCore::getSize() {
    return {width * scale, height * scale};
}

std::vector<float> TextObjectGLCore::getStringSize(const std::string &txt) {
    const std::string old = getText();
    setText(txt);
    auto sz = getSize();
    setText(old);
    return sz;
}

void TextObjectGLCore::cleanupText() {
    for (auto &[id, data] : fonts) {
        glDeleteTextures(1, &data->textureID);
        free(data->charData);
        delete data;
    }
    fonts.clear();

    if (textProgram) {
        glDeleteProgram(textProgram);
        textProgram = 0;
    }
    if (textVAO) {
        glDeleteVertexArrays(1, &textVAO);
        textVAO = 0;
    }
}
