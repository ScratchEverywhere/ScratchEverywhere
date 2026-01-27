#include "image_gl.hpp"
#include "render.hpp"
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

    int x = params.x;
    int y = params.y;
    double rotation = Math::radiansToDegrees(params.rotation);
    float scaleX = params.scale;
    const float scaleY = params.scale;
    const float opacity = params.opacity;
    const bool centered = params.centered;
    const bool flip = params.flip;
    const int brightness = params.brightness;
    const int renderWidth = params.subrect ? params.subrect->w : getWidth();
    const int renderHeight = params.subrect ? params.subrect->h : getHeight();

    glBindTexture(GL_TEXTURE_2D, textureID);

    glColor4f(1.0f, 1.0f, 1.0f, opacity);

    glPushMatrix();
    if (flip) x += renderWidth * std::abs(scaleX);
    glTranslatef(x, y, 0.0f);
    glRotatef(rotation, 0.0f, 0.0f, 1.0f);

    if (centered) {
        glTranslatef((-renderWidth / 2) * scaleY, (-renderHeight / 2) * scaleY, 0.0f);
    }

    if (flip) scaleX = -std::abs(scaleX);

    glScalef(scaleX, scaleY, 1.0f);

    glBegin(GL_QUADS);

    if (params.subrect != nullptr) {
        const float texW = static_cast<float>(getWidth());
        const float texH = static_cast<float>(getHeight());

        const float u0 = params.subrect->x / texW;
        const float v0 = params.subrect->y / texH;
        const float u1 = (params.subrect->x + params.subrect->w) / texW;
        const float v1 = (params.subrect->y + params.subrect->h) / texH;

        glTexCoord2f(u0, v0);
        glVertex2f(0.0f, 0.0f);
        glTexCoord2f(u1, v0);
        glVertex2f(params.subrect->w, 0.0f);
        glTexCoord2f(u1, v1);
        glVertex2f(params.subrect->w, params.subrect->h);
        glTexCoord2f(u0, v1);
        glVertex2f(0.0f, params.subrect->h);
    } else {
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(width, 0.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(width, height);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(0.0f, height);
    }

    glEnd();

    glPopMatrix();

    freeTimer = maxFreeTimer;
}

void Image_GL::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    glBindTexture(GL_TEXTURE_2D, textureID);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    float destX = (float)xPos;
    float destY = (float)yPos;

    if (centered) {
        destX -= (float)width / 2.0f;
        destY -= (float)height / 2.0f;
    }

    float imgW = (float)width;
    float imgH = (float)height;
    float p = (float)padding;
    float w = (float)width;
    float h = (float)height;

    glBegin(GL_QUADS);

    auto drawSubRect = [&](float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh) {
        float u0 = sx / imgW;
        float v0 = sy / imgH;
        float u1 = (sx + sw) / imgW;
        float v1 = (sy + sh) / imgH;
        glTexCoord2f(u0, v0);
        glVertex2f(dx, dy);
        glTexCoord2f(u1, v0);
        glVertex2f(dx + dw, dy);
        glTexCoord2f(u1, v1);
        glVertex2f(dx + dw, dy + dh);
        glTexCoord2f(u0, v1);
        glVertex2f(dx, dy + dh);
    };

    // Top-left
    drawSubRect(0, 0, p, p, destX, destY, p, p);
    // Top
    drawSubRect(p, 0, imgW - p * 2, p, destX + p, destY, w - p * 2, p);
    // Top-right
    drawSubRect(imgW - p, 0, p, p, destX + w - p, destY, p, p);

    // Left
    drawSubRect(0, p, p, imgH - p * 2, destX, destY + p, p, h - p * 2);
    // Center
    drawSubRect(p, p, imgW - p * 2, imgH - p * 2, destX + p, destY + p, w - p * 2, h - p * 2);
    // Right
    drawSubRect(imgW - p, p, p, imgH - p * 2, destX + w - p, destY + p, p, h - p * 2);

    // Bottom-left
    drawSubRect(0, imgH - p, p, p, destX, destY + h - p, p, p);
    // Bottom
    drawSubRect(p, imgH - p, imgW - p * 2, p, destX + p, destY + h - p, w - p * 2, p);
    // Bottom-right
    drawSubRect(imgW - p, imgH - p, p, p, destX + w - p, destY + h - p, p, p);

    glEnd();
    freeTimer = maxFreeTimer;
}

void Image_GL::setInitialTexture() {
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWidth(), getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    free(pixels);
    pixels = nullptr;
}

Image_GL::Image_GL(std::string filePath, bool fromScratchProject) : Image(filePath, fromScratchProject) {
    setInitialTexture();
}

Image_GL::Image_GL(std::string filePath, mz_zip_archive *zip) : Image(filePath, zip) {
    setInitialTexture();
}

Image_GL::~Image_GL() {
    glDeleteTextures(1, &textureID);
}