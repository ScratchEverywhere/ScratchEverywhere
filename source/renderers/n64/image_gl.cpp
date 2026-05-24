#include "image_gl.hpp"
#include "nonstd/expected.hpp"
#include "render.hpp"
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <math.hpp>
#include <os.hpp>
#include <string>
#include <unordered_map>
#include <unzip.hpp>
#include <vector>

void Image_GL::render(ImageRenderParams &params) {
    if (textureID == 0) return;

    float x = params.x;
    float y = params.y;
    double rotation = Math::radiansToDegrees(params.rotation);
    float scaleX = params.scale;
    const float scaleY = params.scale;
    const float opacity = params.opacity;
    const bool centered = params.centered;
    const bool flip = params.flip;
    const int renderWidth = params.subrect ? params.subrect->w : getWidth();
    const int renderHeight = params.subrect ? params.subrect->h : getHeight();

    glBindTexture(GL_TEXTURE_2D, textureID);
    glColor4f(1.0f, 1.0f, 1.0f, opacity);

    glPushMatrix();
    if (flip) x += renderWidth * std::abs(scaleX);
    glTranslatef(x, y, 0.0f);
    glRotatef(rotation, 0.0f, 0.0f, 1.0f);

    if (centered) {
        glTranslatef((-renderWidth / 2.0f) * scaleY, (-renderHeight / 2.0f) * scaleY, 0.0f);
    }

    if (flip) scaleX = -std::abs(scaleX);
    glScalef(scaleX, scaleY, 1.0f);

    glBegin(GL_QUADS);
    if (params.subrect != nullptr) {
        const float texW = static_cast<float>(imgData.width);
        const float texH = static_cast<float>(imgData.height);
        const float u0 = params.subrect->x / texW;
        const float v0 = params.subrect->y / texH;
        const float u1 = (params.subrect->x + params.subrect->w) / texW;
        const float v1 = (params.subrect->y + params.subrect->h) / texH;

        glTexCoord2f(u0, v0); glVertex2f(0.0f, 0.0f);
        glTexCoord2f(u1, v0); glVertex2f(params.subrect->w, 0.0f);
        glTexCoord2f(u1, v1); glVertex2f(params.subrect->w, params.subrect->h);
        glTexCoord2f(u0, v1); glVertex2f(0.0f, params.subrect->h);
    } else {
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(imgData.width, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(imgData.width, imgData.height);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, imgData.height);
    }
    glEnd();
    glPopMatrix();

    freeTimer = maxFreeTimer;
}

void Image_GL::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    if (textureID == 0) return;
    glBindTexture(GL_TEXTURE_2D, textureID);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

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

    glBegin(GL_QUADS);
    auto drawSubRect = [&](float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh) {
        float u0 = sx / imgW;
        float v0 = sy / imgH;
        float u1 = (sx + sw) / imgW;
        float v1 = (sy + sh) / imgH;
        glTexCoord2f(u0, v0); glVertex2f(dx, dy);
        glTexCoord2f(u1, v0); glVertex2f(dx + dw, dy);
        glTexCoord2f(u1, v1); glVertex2f(dx + dw, dy + dh);
        glTexCoord2f(u0, v1); glVertex2f(dx, dy + dh);
    };

    drawSubRect(0, 0, p, p, destX, destY, p, p);
    drawSubRect(p, 0, imgW - p * 2, p, destX + p, destY, w - p * 2, p);
    drawSubRect(imgW - p, 0, p, p, destX + w - p, destY, p, p);
    drawSubRect(0, p, p, imgH - p * 2, destX, destY + p, p, h - p * 2);
    drawSubRect(p, p, imgW - p * 2, imgH - p * 2, destX + p, destY + p, w - p * 2, h - p * 2);
    drawSubRect(imgW - p, p, p, imgH - p * 2, destX + w - p, destY + p, p, h - p * 2);
    drawSubRect(0, imgH - p, p, p, destX, destY + h - p, p, p);
    drawSubRect(p, imgH - p, imgW - p * 2, p, destX + p, destY + h - p, w - p * 2, p);
    drawSubRect(imgW - p, imgH - p, p, p, destX + w - p, destY + h - p, p, p);
    glEnd();

    freeTimer = maxFreeTimer;
}

void *Image_GL::getNativeTexture() {
    return reinterpret_cast<void *>(static_cast<uintptr_t>(textureID));
}

void Image_GL::setInitialTexture() {
    if (textureID == 0) {
        glGenTextures(1, &textureID);
    }

    if (textureID == 0) return;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgData.width, imgData.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imgData.pixels);
}

nonstd::expected<void, std::string> Image_GL::refreshTexture() {
    if (textureID != 0) {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgData.width, imgData.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imgData.pixels);
    } else {
        setInitialTexture();
    }
    return {};
}

Image_GL::Image_GL(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    textureID = 0;
    GLint glMax = 256;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glMax);
    if (glMax <= 0) glMax = 256;
    maxTextureSize = {glMax, glMax};

    const auto initResult = init(filePath, fromScratchProject, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    setInitialTexture();
}

Image_GL::Image_GL(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality, float scale) {
    textureID = 0;
    GLint glMax = 256;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glMax);
    if (glMax <= 0) glMax = 256;
    maxTextureSize = {glMax, glMax};

    const auto initResult = init(filePath, zip, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    setInitialTexture();
}

Image_GL::~Image_GL() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }
}
