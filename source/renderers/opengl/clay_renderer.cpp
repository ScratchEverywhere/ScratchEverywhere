#include <algorithm>
#include <log.hpp>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <text.hpp>

#include <clay.h>
#include <math.hpp>

#include "clay_renderer.hpp"
#include "image.hpp"

#include <GL/gl.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define MAX_FONTS 3
#define MAX_TEXT_SIZE 128

static char cvTextBuffer[MAX_TEXT_SIZE];
static std::vector<std::unique_ptr<TextObject>> fontList;
static uint16_t numFonts = 0;

static inline uint32_t CLAY_COLOR_TO_RGBA(const Clay_Color &color) {
    return ((uint32_t)color.r << 24) | ((uint32_t)color.g << 16) | ((uint32_t)color.b << 8) | (uint32_t)color.a;
}

bool Clay_OpenGL_RegisterFont(const std::string &fontPath) {
    if (numFonts >= MAX_FONTS) return false;
    std::unique_ptr<TextObject> obj = createTextObject("", 0, 0, fontPath);
    obj->setCenterAligned(false);
    numFonts++;
    fontList.push_back(std::move(obj));
    return true;
}

Clay_Dimensions Clay_OpenGL_MeasureText(Clay_StringSlice slice, Clay_TextElementConfig *config, void *userData) {
    if (numFonts == 0 || config->fontId + 1 > numFonts) {
        return {.width = 0.0f, .height = 0.0f};
    }

    const auto &obj = fontList[config->fontId];

    uint32_t length = std::min<int32_t>(slice.length, MAX_TEXT_SIZE);
    memcpy(cvTextBuffer, slice.chars, length);
    cvTextBuffer[length] = '\0';

    obj->setScale((float)config->fontSize / 16.0f);
    const auto size = obj->getStringSize(cvTextBuffer);

    return {
        .width = size[0],
        .height = size[1]};
}

static void OpenGL_RenderFillRoundedRect(const Clay_BoundingBox &rect, const Clay_CornerRadius &cornerRadius, const Clay_Color &color) {
    glColor4ub((GLubyte)color.r, (GLubyte)color.g, (GLubyte)color.b, (GLubyte)color.a);

    const float maxRadius = std::min(rect.width, rect.height) / 2.0f;
    const float clampedRadius[4] = {
        std::min((float)cornerRadius.topLeft, maxRadius),
        std::min((float)cornerRadius.topRight, maxRadius),
        std::min((float)cornerRadius.bottomRight, maxRadius),
        std::min((float)cornerRadius.bottomLeft, maxRadius)};

    if (clampedRadius[0] == 0 && clampedRadius[1] == 0 && clampedRadius[2] == 0 && clampedRadius[3] == 0) {
        glBegin(GL_QUADS);
        glVertex2f(rect.x, rect.y);
        glVertex2f(rect.x + rect.width, rect.y);
        glVertex2f(rect.x + rect.width, rect.y + rect.height);
        glVertex2f(rect.x, rect.y + rect.height);
        glEnd();
        return;
    }

    const float centerX = rect.x + rect.width / 2.0f;
    const float centerY = rect.y + rect.height / 2.0f;

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(centerX, centerY);

    auto drawArc = [&](float cx, float cy, float r, float startAngle) {
        if (r > 0.0f) {
            int segments = std::max(16, (int)(r * 0.5f));
            float step = (M_PI / 2.0f) / segments;
            for (int i = 0; i <= segments; i++) {
                float a = startAngle + i * step;
                glVertex2f(cx + cosf(a) * r, cy + sinf(a) * r);
            }
        } else {
            glVertex2f(cx, cy);
        }
    };

    drawArc(rect.x + clampedRadius[0], rect.y + clampedRadius[0], clampedRadius[0], M_PI);
    drawArc(rect.x + rect.width - clampedRadius[1], rect.y + clampedRadius[1], clampedRadius[1], 3.0f * M_PI / 2.0f);
    drawArc(rect.x + rect.width - clampedRadius[2], rect.y + rect.height - clampedRadius[2], clampedRadius[2], 0.0f);
    drawArc(rect.x + clampedRadius[3], rect.y + rect.height - clampedRadius[3], clampedRadius[3], M_PI / 2.0f);

    glVertex2f(rect.x, rect.y + clampedRadius[0]);

    glEnd();
}

static void OpenGL_RenderCornerBorder(const Clay_BoundingBox &boundingBox, const Clay_BorderRenderData &config, int cornerIndex, Clay_Color color) {
    glColor4ub((GLubyte)color.r, (GLubyte)color.g, (GLubyte)color.b, (GLubyte)color.a);

    float centerX, centerY, outerRadius, startAngle, borderWidth;
    const float maxRadius = std::min(boundingBox.width, boundingBox.height) / 2.0f;

    switch (cornerIndex) {
    case 0:
        startAngle = M_PI;
        outerRadius = std::min((float)config.cornerRadius.topLeft, maxRadius);
        centerX = boundingBox.x + outerRadius;
        centerY = boundingBox.y + outerRadius;
        borderWidth = config.width.top;
        break;
    case 1:
        startAngle = 3.0f * M_PI / 2.0f;
        outerRadius = std::min((float)config.cornerRadius.topRight, maxRadius);
        centerX = boundingBox.x + boundingBox.width - outerRadius;
        centerY = boundingBox.y + outerRadius;
        borderWidth = config.width.top;
        break;
    case 2:
        startAngle = 0.0f;
        outerRadius = std::min((float)config.cornerRadius.bottomRight, maxRadius);
        centerX = boundingBox.x + boundingBox.width - outerRadius;
        centerY = boundingBox.y + boundingBox.height - outerRadius;
        borderWidth = config.width.bottom;
        break;
    case 3:
        startAngle = M_PI / 2.0f;
        outerRadius = std::min((float)config.cornerRadius.bottomLeft, maxRadius);
        centerX = boundingBox.x + outerRadius;
        centerY = boundingBox.y + boundingBox.height - outerRadius;
        borderWidth = config.width.bottom;
        break;
    default:
        return;
    }

    float innerRadius = std::max(0.0f, outerRadius - borderWidth);

    if (outerRadius > 0.0f) {
        int segments = std::max(16, (int)(outerRadius * 0.5f));
        float step = (M_PI / 2.0f) / segments;

        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= segments; i++) {
            float a = startAngle + i * step;
            float c = cosf(a);
            float s = sinf(a);
            glVertex2f(centerX + c * outerRadius, centerY + s * outerRadius); // Outer edge
            glVertex2f(centerX + c * innerRadius, centerY + s * innerRadius); // Inner edge
        }
        glEnd();
    }
}

void Clay_OpenGL_Render(Clay_Dimensions dimensions, Clay_RenderCommandArray renderCommands) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (uint32_t i = 0; i < renderCommands.length; i++) {
        Clay_RenderCommand *renderCommand = Clay_RenderCommandArray_Get(&renderCommands, i);
        Clay_BoundingBox box = renderCommand->boundingBox;

        switch (renderCommand->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            glDisable(GL_TEXTURE_2D);

            Clay_RectangleRenderData *config = &renderCommand->renderData.rectangle;

            if (config->cornerRadius.topLeft > 0 || config->cornerRadius.topRight > 0 ||
                config->cornerRadius.bottomLeft > 0 || config->cornerRadius.bottomRight > 0) {
                OpenGL_RenderFillRoundedRect(box, config->cornerRadius, config->backgroundColor);
            } else {
                Clay_Color color = config->backgroundColor;
                glColor4ub((GLubyte)color.r, (GLubyte)color.g, (GLubyte)color.b, (GLubyte)color.a);
                glBegin(GL_QUADS);
                glVertex2f(box.x, box.y);
                glVertex2f(box.x + box.width, box.y);
                glVertex2f(box.x + box.width, box.y + box.height);
                glVertex2f(box.x, box.y + box.height);
                glEnd();
            }

            glEnable(GL_TEXTURE_2D);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            glDisable(GL_TEXTURE_2D);

            Clay_BorderRenderData *config = &renderCommand->renderData.border;
            Clay_Color color = config->color;

            glColor4ub((GLubyte)color.r, (GLubyte)color.g, (GLubyte)color.b, (GLubyte)color.a);

            if (box.width > 0 && box.height > 0) {
                const float maxRadius = std::min(box.width, box.height) / 2.0f;

                glBegin(GL_QUADS);
                if (config->width.left > 0) {
                    float clampedRadiusTop = std::min((float)config->cornerRadius.topLeft, maxRadius);
                    float clampedRadiusBottom = std::min((float)config->cornerRadius.bottomLeft, maxRadius);
                    glVertex2f(box.x, box.y + clampedRadiusTop);
                    glVertex2f(box.x + config->width.left, box.y + clampedRadiusTop);
                    glVertex2f(box.x + config->width.left, box.y + box.height - clampedRadiusBottom);
                    glVertex2f(box.x, box.y + box.height - clampedRadiusBottom);
                }
                if (config->width.right > 0) {
                    float clampedRadiusTop = std::min((float)config->cornerRadius.topRight, maxRadius);
                    float clampedRadiusBottom = std::min((float)config->cornerRadius.bottomRight, maxRadius);
                    glVertex2f(box.x + box.width - config->width.right, box.y + clampedRadiusTop);
                    glVertex2f(box.x + box.width, box.y + clampedRadiusTop);
                    glVertex2f(box.x + box.width, box.y + box.height - clampedRadiusBottom);
                    glVertex2f(box.x + box.width - config->width.right, box.y + box.height - clampedRadiusBottom);
                }
                if (config->width.top > 0) {
                    float clampedRadiusLeft = std::min((float)config->cornerRadius.topLeft, maxRadius);
                    float clampedRadiusRight = std::min((float)config->cornerRadius.topRight, maxRadius);
                    glVertex2f(box.x + clampedRadiusLeft, box.y);
                    glVertex2f(box.x + box.width - clampedRadiusRight, box.y);
                    glVertex2f(box.x + box.width - clampedRadiusRight, box.y + config->width.top);
                    glVertex2f(box.x + clampedRadiusLeft, box.y + config->width.top);
                }
                if (config->width.bottom > 0) {
                    float clampedRadiusLeft = std::min((float)config->cornerRadius.bottomLeft, maxRadius);
                    float clampedRadiusRight = std::min((float)config->cornerRadius.bottomRight, maxRadius);
                    glVertex2f(box.x + clampedRadiusLeft, box.y + box.height - config->width.bottom);
                    glVertex2f(box.x + box.width - clampedRadiusRight, box.y + box.height - config->width.bottom);
                    glVertex2f(box.x + box.width - clampedRadiusRight, box.y + box.height);
                    glVertex2f(box.x + clampedRadiusLeft, box.y + box.height);
                }
                glEnd();

                if (config->width.top > 0 && config->cornerRadius.topLeft > 0) {
                    OpenGL_RenderCornerBorder(box, *config, 0, color);
                }
                if (config->width.top > 0 && config->cornerRadius.topRight > 0) {
                    OpenGL_RenderCornerBorder(box, *config, 1, color);
                }
                if (config->width.bottom > 0 && config->cornerRadius.bottomRight > 0) {
                    OpenGL_RenderCornerBorder(box, *config, 2, color);
                }
                if (config->width.bottom > 0 && config->cornerRadius.bottomLeft > 0) {
                    OpenGL_RenderCornerBorder(box, *config, 3, color);
                }
            }

            glEnable(GL_TEXTURE_2D);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *config = &renderCommand->renderData.text;

            if (numFonts == 0 || config->fontId + 1 > numFonts) {
                break;
            }

            uint32_t color = CLAY_COLOR_TO_RGBA(config->textColor);
            const auto &obj = fontList[config->fontId];

            Clay_StringSlice string = config->stringContents;
            uint32_t length = std::min<int32_t>(string.length, MAX_TEXT_SIZE);
            memcpy(cvTextBuffer, string.chars, length);
            cvTextBuffer[length] = '\0';

            obj->setColor(color);
            obj->setScale((float)config->fontSize / 16.0f);
            obj->setText(cvTextBuffer);

            obj->render(box.x, box.y);

            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            Clay_ImageRenderData *config = &renderCommand->renderData.image;

            if (config->imageData != NULL) {
                auto &image = *(Image *)config->imageData;
                const float scale = box.width / static_cast<double>(image.getWidth());

                ImageRenderParams params;
                params.centered = false;
                params.x = box.x;
                params.y = box.y;
                params.scale = scale;

                image.render(params);
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
            glEnable(GL_SCISSOR_TEST);

            GLint scissorX = (GLint)box.x;
            GLint scissorY = (GLint)(dimensions.height - (box.y + box.height));
            GLsizei scissorWidth = (GLsizei)box.width;
            GLsizei scissorHeight = (GLsizei)box.height;

            glScissor(scissorX, scissorY, scissorWidth, scissorHeight);

            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
            glDisable(GL_SCISSOR_TEST);
            break;
        }
        default: {
            Log::logWarning("Clay: Unhandled render command: " + std::to_string(renderCommand->commandType));
        }
        }
    }
}
