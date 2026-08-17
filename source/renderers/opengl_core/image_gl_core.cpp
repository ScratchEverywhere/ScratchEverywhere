#include "image_gl_core.hpp"
#include "nonstd/expected.hpp"
#include "render.hpp"
#include <cmath>
#include <math.hpp>
#include <os.hpp>
#include <render.hpp>
#include <string>
#include <unzip.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern GLuint spriteProgram;
extern GLuint quadVAO;

static void buildModelMatrix(float out[16],
                             float tx, float ty,
                             float sx, float sy,
                             float rotRad,
                             float pivotX, float pivotY) {
    float c = std::cos(rotRad);
    float s = std::sin(rotRad);

    out[0] = c * sx;
    out[1] = s * sx;
    out[2] = 0.0f;
    out[3] = 0.0f;

    out[4] = -s * sy;
    out[5] = c * sy;
    out[6] = 0.0f;
    out[7] = 0.0f;

    out[8] = 0.0f;
    out[9] = 0.0f;
    out[10] = 1.0f;
    out[11] = 0.0f;

    out[12] = tx + pivotX - (c * sx * pivotX - s * sy * pivotY);
    out[13] = ty + pivotY - (s * sx * pivotX + c * sy * pivotY);
    out[14] = 0.0f;
    out[15] = 1.0f;
}

static void buildOrthoLocal(float out[16], float l, float r, float b, float t) {
    for (int i = 0; i < 16; ++i)
        out[i] = 0.0f;
    out[0] = 2.0f / (r - l);
    out[5] = 2.0f / (t - b);
    out[10] = -1.0f;
    out[12] = -(r + l) / (r - l);
    out[13] = -(t + b) / (t - b);
    out[15] = 1.0f;
}

void Image_GLCore::render(ImageRenderParams &params) {
    const float x = params.x;
    const float y = params.y;
    const bool centered = params.centered;
    const bool flip = params.flip;
    const float opacity = params.opacity;
    const int brightness = params.brightness;
    const float rotation = params.rotation;

    const float colorEffect = params.colorEffect;
    const float fisheyeEffect = params.fisheyeEffect;
    const float whirlEffect = params.whirlEffect;
    const float pixelateEffect = params.pixelateEffect;
    const float mosaicEffect = params.mosaicEffect;

    const float scaleX = flip ? -std::abs(params.scale) : std::abs(params.scale);
    const float scaleY = params.scale;

    int renderWidth, renderHeight;
    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;

    if (params.subrect) {
        renderWidth = params.subrect->w;
        renderHeight = params.subrect->h;
        float tw = (float)imgData.width;
        float th = (float)imgData.height;
        u0 = params.subrect->x / tw;
        v0 = params.subrect->y / th;
        u1 = (params.subrect->x + params.subrect->w) / tw;
        v1 = (params.subrect->y + params.subrect->h) / th;
    } else {
        renderWidth = getWidth();
        renderHeight = getHeight();
    }

    float pivotX = centered ? 0.5f : 0.0f;
    float pivotY = centered ? 0.5f : 0.0f;

    float drawX = x;
    if (flip) drawX += renderWidth * std::abs(scaleX);

    float model[16];
    buildModelMatrix(model,
                     drawX, y,
                     (float)renderWidth * scaleX,
                     (float)renderHeight * scaleY,
                     -rotation,
                     pivotX, pivotY);

    float proj[16];
    {
        GLint vp[4];
        glGetIntegerv(GL_VIEWPORT, vp);
        buildOrthoLocal(proj, 0.0f, (float)vp[2], (float)vp[3], 0.0f);
    }

    glUseProgram(spriteProgram);

    glUniformMatrix4fv(glGetUniformLocation(spriteProgram, "u_projection"), 1, GL_FALSE, proj);
    glUniformMatrix4fv(glGetUniformLocation(spriteProgram, "u_model"), 1, GL_FALSE, model);

    glUniform1i(glGetUniformLocation(spriteProgram, "u_tex"), 0);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_opacity"), opacity);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_brightness"), (float)brightness);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_color"), colorEffect);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_fisheye"), fisheyeEffect);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_whirl"), whirlEffect);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_pixelate"), pixelateEffect);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_mosaic"), mosaicEffect);
    glUniform2f(glGetUniformLocation(spriteProgram, "u_tex_size"), getWidth(), getHeight());

    if (params.subrect) {
        float verts[] = {
            0.0f,
            0.0f,
            u0,
            v0,
            1.0f,
            0.0f,
            u1,
            v0,
            1.0f,
            1.0f,
            u1,
            v1,
            0.0f,
            1.0f,
            u0,
            v1,
        };
        static const GLuint idx[] = {0, 1, 2, 2, 3, 0};

        GLuint vao = 0, vbo = 0, ebo = 0;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);
    } else {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindVertexArray(quadVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    freeTimer = maxFreeTimer;
}

void Image_GLCore::renderNineslice(double xPos, double yPos,
                                   double width, double height,
                                   double padding, bool centered) {
    float destX = (float)xPos;
    float destY = (float)yPos;
    if (centered) {
        destX -= (float)width / 2.0f;
        destY -= (float)height / 2.0f;
    }

    float imgW = (float)getWidth();
    float imgH = (float)getHeight();
    float p = (float)padding;
    float w = (float)width;
    float h = (float)height;

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    float proj[16];
    buildOrthoLocal(proj, 0.0f, (float)vp[2], (float)vp[3], 0.0f);

    float model[16] = {};
    model[0] = model[5] = model[10] = model[15] = 1.0f;

    glUseProgram(spriteProgram);
    glUniformMatrix4fv(glGetUniformLocation(spriteProgram, "u_projection"), 1, GL_FALSE, proj);
    glUniformMatrix4fv(glGetUniformLocation(spriteProgram, "u_model"), 1, GL_FALSE, model);
    glUniform1i(glGetUniformLocation(spriteProgram, "u_tex"), 0);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_opacity"), 1.0f);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_brightness"), 0.0f);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_color"), 0.0f);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_fisheye"), 0.0f);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_whirl"), 0.0f);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_pixelate"), 0.0f);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_mosaic"), 0.0f);
    glUniform2f(glGetUniformLocation(spriteProgram, "u_tex_size"), imgW, imgH);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto drawSlice = [&](float sx, float sy, float sw, float sh,
                         float dx, float dy, float dw, float dh) {
        float u0 = sx / imgW, v0 = sy / imgH;
        float u1 = (sx + sw) / imgW, v1 = (sy + sh) / imgH;

        float verts[] = {
            dx,
            dy,
            u0,
            v0,
            dx + dw,
            dy,
            u1,
            v0,
            dx + dw,
            dy + dh,
            u1,
            v1,
            dx,
            dy + dh,
            u0,
            v1,
        };
        static const GLuint idx[] = {0, 1, 2, 2, 3, 0};

        GLuint vao = 0, vbo = 0, ebo = 0;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);
    };

    drawSlice(0, 0, p, p, destX, destY, p, p);
    drawSlice(p, 0, imgW - p * 2, p, destX + p, destY, w - p * 2, p);
    drawSlice(imgW - p, 0, p, p, destX + w - p, destY, p, p);

    drawSlice(0, p, p, imgH - p * 2, destX, destY + p, p, h - p * 2);
    drawSlice(p, p, imgW - p * 2, imgH - p * 2, destX + p, destY + p, w - p * 2, h - p * 2);
    drawSlice(imgW - p, p, p, imgH - p * 2, destX + w - p, destY + p, p, h - p * 2);

    drawSlice(0, imgH - p, p, p, destX, destY + h - p, p, p);
    drawSlice(p, imgH - p, imgW - p * 2, p, destX + p, destY + h - p, w - p * 2, p);
    drawSlice(imgW - p, imgH - p, p, p, destX + w - p, destY + h - p, p, p);

    freeTimer = maxFreeTimer;
}

void *Image_GLCore::getNativeTexture() {
    return reinterpret_cast<void *>(static_cast<uintptr_t>(textureID));
}

void Image_GLCore::setInitialTexture() {
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 imgData.width, imgData.height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, imgData.pixels);
}

nonstd::expected<void, std::string> Image_GLCore::refreshTexture() {
    glDeleteTextures(1, &textureID);
    setInitialTexture();
    return {};
}

Image_GLCore::Image_GLCore(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    GLint glMaxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glMaxTextureSize);
    maxTextureSize = {(unsigned)glMaxTextureSize, (unsigned)glMaxTextureSize};

    const auto result = init(filePath, fromScratchProject, bitmapHalfQuality, scale);
    if (!result.has_value()) {
        error = result.error();
        return;
    }
    setInitialTexture();
}

Image_GLCore::Image_GLCore(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality, float scale) {
    GLint glMaxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glMaxTextureSize);
    maxTextureSize = {(unsigned)glMaxTextureSize, (unsigned)glMaxTextureSize};

    const auto result = init(filePath, zip, bitmapHalfQuality, scale);
    if (!result.has_value()) {
        error = result.error();
        return;
    }
    setInitialTexture();
}

Image_GLCore::~Image_GLCore() {
    glDeleteTextures(1, &textureID);
}
