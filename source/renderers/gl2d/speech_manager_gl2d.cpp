#include "speech_manager_gl2d.hpp"
#include <image_gl2d.hpp>
#include <nds.h>

SpeechManagerGL2D::SpeechManagerGL2D() = default;

SpeechManagerGL2D::~SpeechManagerGL2D() {
    cleanup();
}

void SpeechManagerGL2D::renderIndicator(Sprite *sprite, int indicatorX, int indicatorY, int indicatorSize, bool flip) {
    if (!indicatorImage) return;

    auto styleIt = speechStyles.find(sprite);
    if (styleIt == speechStyles.end()) return;
    std::string style = styleIt->second;

    Image_GL2D *image = reinterpret_cast<Image_GL2D *>(indicatorImage.get());
    glBindTexture(0, image->textureID);

    int halfWidth = image->getWidth() / 2;
    int fullHeight = image->getHeight();

    int srcX = (style == "think") ? halfWidth : 0;
    int srcY = 0;
    int srcW = halfWidth;
    int srcH = fullHeight;

    int u0 = srcX;
    int v0 = srcY;
    int u1 = srcX + srcW;
    int v1 = srcY + srcH;

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    const int depth = 100;
    glColor3b(255, 255, 255);

    glBegin(GL_QUADS);

    glTexCoord2t16(inttot16(u0), inttot16(v1));
    glVertex3v16(indicatorX, indicatorY + indicatorSize, depth);

    glTexCoord2t16(inttot16(u1), inttot16(v1));
    glVertex3v16(indicatorX + indicatorSize, indicatorY + indicatorSize, depth);

    glTexCoord2t16(inttot16(u1), inttot16(v0));
    glVertex3v16(indicatorX + indicatorSize, indicatorY, depth);

    glTexCoord2t16(inttot16(u0), inttot16(v0));
    glVertex3v16(indicatorX, indicatorY, depth);

    glEnd();
    glColor3b(255, 255, 255);
}
