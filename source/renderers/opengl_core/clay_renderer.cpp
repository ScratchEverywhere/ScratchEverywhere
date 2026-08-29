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
#include "render.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define MAX_FONTS 3
#define MAX_TEXT_SIZE 128

static char cvTextBuffer[MAX_TEXT_SIZE];
static std::vector<std::unique_ptr<TextObject>> fontList;
static uint16_t numFonts = 0;

static GLuint claySolidProgram = 0;
static GLuint clayVAO = 0;
static GLuint clayVBO = 0;

static const char *kClaySolidVert = R"glsl(
#version 410 core

layout(location = 0) in vec2 a_pos;

uniform mat4 u_projection;

void main() {
    gl_Position = u_projection * vec4(a_pos, 0.0, 1.0);
}
)glsl";

static const char *kClaySolidFrag = R"glsl(
#version 410 core

out vec4 frag_color;
uniform vec4 u_color;

void main() {
    frag_color = u_color;
}
)glsl";

static GLuint clayCompileShader(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Log::logError(std::string("[GL Core Clay] Shader compile error: ") + log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static void ensureClaySolidProgram() {
    if (claySolidProgram) return;

    GLuint vert = clayCompileShader(GL_VERTEX_SHADER, kClaySolidVert);
    GLuint frag = clayCompileShader(GL_FRAGMENT_SHADER, kClaySolidFrag);

    claySolidProgram = glCreateProgram();
    glAttachShader(claySolidProgram, vert);
    glAttachShader(claySolidProgram, frag);
    glLinkProgram(claySolidProgram);
    glDeleteShader(vert);
    glDeleteShader(frag);

    glGenVertexArrays(1, &clayVAO);
    glGenBuffers(1, &clayVBO);
    glBindVertexArray(clayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, clayVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

static void buildClayOrtho(float out[16], float l, float r, float b, float t) {
    for (int i = 0; i < 16; ++i)
        out[i] = 0.0f;
    out[0] = 2.0f / (r - l);
    out[5] = 2.0f / (t - b);
    out[10] = -1.0f;
    out[12] = -(r + l) / (r - l);
    out[13] = -(t + b) / (t - b);
    out[15] = 1.0f;
}

static void drawClayShape(const std::vector<float> &verts, GLenum mode, const Clay_Color &color, const float proj[16]) {
    if (verts.empty()) return;

    glUseProgram(claySolidProgram);
    glUniformMatrix4fv(glGetUniformLocation(claySolidProgram, "u_projection"), 1, GL_FALSE, proj);
    glUniform4f(glGetUniformLocation(claySolidProgram, "u_color"),
                color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);

    glBindVertexArray(clayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, clayVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STREAM_DRAW);
    glDrawArrays(mode, 0, (GLsizei)(verts.size() / 2));
    glBindVertexArray(0);
}

bool Clay_OpenGLCore_RegisterFont(const std::string &fontPath) {
    if (numFonts >= MAX_FONTS) return false;
    std::unique_ptr<TextObject> obj = createTextObject("", 0, 0, fontPath);
    obj->setCenterAligned(false);
    numFonts++;
    fontList.push_back(std::move(obj));
    return true;
}

Clay_Dimensions Clay_OpenGLCore_MeasureText(Clay_StringSlice slice, Clay_TextElementConfig *config, void *userData) {
    if (numFonts == 0 || config->fontId + 1 > numFonts) {
        return {.width = 0.0f, .height = 0.0f};
    }

    const auto &obj = fontList[config->fontId];

    uint32_t length = std::min<int32_t>(slice.length, MAX_TEXT_SIZE);
    memcpy(cvTextBuffer, slice.chars, length);
    cvTextBuffer[length] = '\0';

    obj->setScale((float)config->fontSize / 48.0f);
    const auto size = obj->getStringSize(cvTextBuffer);

    return {
        .width = size[0],
        .height = size[1]};
}

static void OpenGLCore_RenderFillRoundedRect(const Clay_BoundingBox &rect, const Clay_CornerRadius &cornerRadius, const Clay_Color &color, const float proj[16]) {
    const float maxRadius = std::min(rect.width, rect.height) / 2.0f;
    const float clampedRadius[4] = {
        std::min((float)cornerRadius.topLeft, maxRadius),
        std::min((float)cornerRadius.topRight, maxRadius),
        std::min((float)cornerRadius.bottomRight, maxRadius),
        std::min((float)cornerRadius.bottomLeft, maxRadius)};

    if (clampedRadius[0] == 0 && clampedRadius[1] == 0 && clampedRadius[2] == 0 && clampedRadius[3] == 0) {
        std::vector<float> verts = {
            rect.x, rect.y,
            rect.x + rect.width, rect.y,
            rect.x + rect.width, rect.y + rect.height,
            rect.x, rect.y + rect.height};
        drawClayShape(verts, GL_TRIANGLE_FAN, color, proj);
        return;
    }

    const float centerX = rect.x + rect.width / 2.0f;
    const float centerY = rect.y + rect.height / 2.0f;

    std::vector<float> verts;
    verts.push_back(centerX);
    verts.push_back(centerY);

    auto drawArc = [&](float cx, float cy, float r, float startAngle) {
        if (r > 0.0f) {
            int segments = std::max(16, (int)(r * 0.5f));
            float step = (M_PI / 2.0f) / segments;
            for (int i = 0; i <= segments; i++) {
                float a = startAngle + i * step;
                verts.push_back(cx + cosf(a) * r);
                verts.push_back(cy + sinf(a) * r);
            }
        } else {
            verts.push_back(cx);
            verts.push_back(cy);
        }
    };

    drawArc(rect.x + clampedRadius[0], rect.y + clampedRadius[0], clampedRadius[0], M_PI);
    drawArc(rect.x + rect.width - clampedRadius[1], rect.y + clampedRadius[1], clampedRadius[1], 3.0f * M_PI / 2.0f);
    drawArc(rect.x + rect.width - clampedRadius[2], rect.y + rect.height - clampedRadius[2], clampedRadius[2], 0.0f);
    drawArc(rect.x + clampedRadius[3], rect.y + rect.height - clampedRadius[3], clampedRadius[3], M_PI / 2.0f);

    verts.push_back(rect.x);
    verts.push_back(rect.y + clampedRadius[0]);

    drawClayShape(verts, GL_TRIANGLE_FAN, color, proj);
}

static void OpenGLCore_RenderCornerBorder(const Clay_BoundingBox &boundingBox, const Clay_BorderRenderData &config, int cornerIndex, Clay_Color color, const float proj[16]) {
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

        std::vector<float> verts;
        verts.reserve((segments + 1) * 4);
        for (int i = 0; i <= segments; i++) {
            float a = startAngle + i * step;
            float c = cosf(a);
            float s = sinf(a);
            verts.push_back(centerX + c * outerRadius);
            verts.push_back(centerY + s * outerRadius);
            verts.push_back(centerX + c * innerRadius);
            verts.push_back(centerY + s * innerRadius);
        }

        drawClayShape(verts, GL_TRIANGLE_STRIP, color, proj);
    }
}

void Clay_OpenGLCore_Render(Clay_Dimensions dimensions, Clay_RenderCommandArray renderCommands) {
    ensureClaySolidProgram();

    float proj[16];
    buildClayOrtho(proj, 0.0f, dimensions.width, dimensions.height, 0.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (uint32_t i = 0; i < renderCommands.length; i++) {
        Clay_RenderCommand *renderCommand = Clay_RenderCommandArray_Get(&renderCommands, i);
        Clay_BoundingBox box = renderCommand->boundingBox;

        switch (renderCommand->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            Clay_RectangleRenderData *config = &renderCommand->renderData.rectangle;

            if (config->cornerRadius.topLeft > 0 || config->cornerRadius.topRight > 0 ||
                config->cornerRadius.bottomLeft > 0 || config->cornerRadius.bottomRight > 0) {
                OpenGLCore_RenderFillRoundedRect(box, config->cornerRadius, config->backgroundColor, proj);
            } else {
                std::vector<float> verts = {
                    box.x, box.y,
                    box.x + box.width, box.y,
                    box.x + box.width, box.y + box.height,
                    box.x, box.y + box.height};
                drawClayShape(verts, GL_TRIANGLE_FAN, config->backgroundColor, proj);
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData *config = &renderCommand->renderData.border;
            Clay_Color color = config->color;

            if (box.width > 0 && box.height > 0) {
                const float maxRadius = std::min(box.width, box.height) / 2.0f;

                if (config->width.left > 0) {
                    float clampedRadiusTop = std::min((float)config->cornerRadius.topLeft, maxRadius);
                    float clampedRadiusBottom = std::min((float)config->cornerRadius.bottomLeft, maxRadius);
                    std::vector<float> verts = {
                        box.x, box.y + clampedRadiusTop,
                        box.x + config->width.left, box.y + clampedRadiusTop,
                        box.x + config->width.left, box.y + box.height - clampedRadiusBottom,
                        box.x, box.y + box.height - clampedRadiusBottom};
                    drawClayShape(verts, GL_TRIANGLE_FAN, color, proj);
                }
                if (config->width.right > 0) {
                    float clampedRadiusTop = std::min((float)config->cornerRadius.topRight, maxRadius);
                    float clampedRadiusBottom = std::min((float)config->cornerRadius.bottomRight, maxRadius);
                    std::vector<float> verts = {
                        box.x + box.width - config->width.right, box.y + clampedRadiusTop,
                        box.x + box.width, box.y + clampedRadiusTop,
                        box.x + box.width, box.y + box.height - clampedRadiusBottom,
                        box.x + box.width - config->width.right, box.y + box.height - clampedRadiusBottom};
                    drawClayShape(verts, GL_TRIANGLE_FAN, color, proj);
                }
                if (config->width.top > 0) {
                    float clampedRadiusLeft = std::min((float)config->cornerRadius.topLeft, maxRadius);
                    float clampedRadiusRight = std::min((float)config->cornerRadius.topRight, maxRadius);
                    std::vector<float> verts = {
                        box.x + clampedRadiusLeft, box.y,
                        box.x + box.width - clampedRadiusRight, box.y,
                        box.x + box.width - clampedRadiusRight, box.y + config->width.top,
                        box.x + clampedRadiusLeft, box.y + config->width.top};
                    drawClayShape(verts, GL_TRIANGLE_FAN, color, proj);
                }
                if (config->width.bottom > 0) {
                    float clampedRadiusLeft = std::min((float)config->cornerRadius.bottomLeft, maxRadius);
                    float clampedRadiusRight = std::min((float)config->cornerRadius.bottomRight, maxRadius);
                    std::vector<float> verts = {
                        box.x + clampedRadiusLeft, box.y + box.height - config->width.bottom,
                        box.x + box.width - clampedRadiusRight, box.y + box.height - config->width.bottom,
                        box.x + box.width - clampedRadiusRight, box.y + box.height,
                        box.x + clampedRadiusLeft, box.y + box.height};
                    drawClayShape(verts, GL_TRIANGLE_FAN, color, proj);
                }

                if (config->width.top > 0 && config->cornerRadius.topLeft > 0) {
                    OpenGLCore_RenderCornerBorder(box, *config, 0, color, proj);
                }
                if (config->width.top > 0 && config->cornerRadius.topRight > 0) {
                    OpenGLCore_RenderCornerBorder(box, *config, 1, color, proj);
                }
                if (config->width.bottom > 0 && config->cornerRadius.bottomRight > 0) {
                    OpenGLCore_RenderCornerBorder(box, *config, 2, color, proj);
                }
                if (config->width.bottom > 0 && config->cornerRadius.bottomLeft > 0) {
                    OpenGLCore_RenderCornerBorder(box, *config, 3, color, proj);
                }
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *config = &renderCommand->renderData.text;

            if (numFonts == 0 || config->fontId + 1 > numFonts) {
                break;
            }

            uint32_t color = ((uint32_t)config->textColor.r << 24) | ((uint32_t)config->textColor.g << 16) |
                              ((uint32_t)config->textColor.b << 8) | (uint32_t)config->textColor.a;
            const auto &obj = fontList[config->fontId];

            Clay_StringSlice string = config->stringContents;
            uint32_t length = std::min<int32_t>(string.length, MAX_TEXT_SIZE);
            memcpy(cvTextBuffer, string.chars, length);
            cvTextBuffer[length] = '\0';

            obj->setColor(color);
            obj->setScale((float)config->fontSize / 48.0f);
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
