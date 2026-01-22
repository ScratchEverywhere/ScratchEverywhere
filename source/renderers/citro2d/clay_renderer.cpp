// Modified version of Clay3DS to work with modern Clay

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

#include <clay.h>
#include <math.hpp>

#include "clay_renderer.hpp"

// Maximum number of glyphs drawable with each single text draw call.
#define MAX_TEXT_SIZE 4096
// Maximum number of extra fonts that can be loaded at the same time.
#define MAX_FONTS 8

#define CLAY_COLOR_TO_C2D(cc) C2D_Color32((uint8_t)cc.r, (uint8_t)cc.g, (uint8_t)cc.b, (uint8_t)cc.a)
#define CALC_FONT_SCALE(size) ((float)(size) / 30.f)

static void Clay_Citro2D_FillQuad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, uint32_t color) {
    C2D_DrawTriangle(x1, y1, color, x2, y2, color, x3, y3, color, 0.f);
    C2D_DrawTriangle(x1, y1, color, x3, y3, color, x4, y4, color, 0.f);
}

static void Clay_Citro2D_DrawArc(float cx, float cy, float radius, float angs, float ange, float thickness, uint32_t color) {
    uint32_t segments = 4;
    float innerRadius = radius - thickness / 2.f;
    float outerRadius = radius + thickness / 2.f;

    float step = Math::degreesToRadians((ange - angs) / segments);
    float cosStep = cos(step);
    float sinStep = sin(step);

    float angle = Math::degreesToRadians(angs);
    float cosAngle1 = cos(angle);
    float sinAngle1 = sin(angle);

    float xInner1 = cx + innerRadius * cosAngle1;
    float yInner1 = cy + innerRadius * sinAngle1;
    float xOuter1 = cx + outerRadius * cosAngle1;
    float yOuter1 = cy + outerRadius * sinAngle1;

    for (uint32_t i = 1; i <= segments; ++i) {
        float cosAngle2 = cosAngle1 * cosStep - sinAngle1 * sinStep;
        float sinAngle2 = sinAngle1 * cosStep + cosAngle1 * sinStep;
        float xInner2 = cx + innerRadius * cosAngle2;
        float yInner2 = cy + innerRadius * sinAngle2;
        float xOuter2 = cx + outerRadius * cosAngle2;
        float yOuter2 = cy + outerRadius * sinAngle2;

        Clay_Citro2D_FillQuad(xInner1, yInner1, xInner2, yInner2, xOuter2, yOuter2, xOuter1, yOuter1, color);

        cosAngle1 = cosAngle2;
        sinAngle1 = sinAngle2;
        xInner1 = xInner2;
        yInner1 = yInner2;
        xOuter1 = xOuter2;
        yOuter1 = yOuter2;
    }
}

static void Clay_Citro2D_FillArc(float cx, float cy, float radius, float angs, float ange, uint32_t color) {
    uint32_t segments = 4;

    float step = Math::degreesToRadians((ange - angs) / segments);
    float cosStep = cos(step);
    float sinStep = sin(step);

    float angle = Math::degreesToRadians(angs);
    float cosAngle1 = cos(angle);
    float sinAngle1 = sin(angle);

    float x1 = cx + radius * cosAngle1;
    float y1 = cy + radius * sinAngle1;

    for (uint32_t i = 1; i <= segments; ++i) {
        float cosAngle2 = cosAngle1 * cosStep - sinAngle1 * sinStep;
        float sinAngle2 = sinAngle1 * cosStep + cosAngle1 * sinStep;
        float x2 = cx + radius * cosAngle2;
        float y2 = cy + radius * sinAngle2;

        C2D_DrawTriangle(cx, cy, color, x1, y1, color, x2, y2, color, 0.f);

        cosAngle1 = cosAngle2;
        sinAngle1 = sinAngle2;
        x1 = x2;
        y1 = y2;
    }
}

static char cvTextBuffer[MAX_TEXT_SIZE];
static C2D_TextBuf Clay_Citro2D_GetStaticTextBuffer(void) {
    static C2D_TextBuf buffer = NULL;

    if (buffer == NULL) {
        buffer = C2D_TextBufNew(MAX_TEXT_SIZE);
    } else {
        C2D_TextBufClear(buffer);
    }

    return buffer;
}

static C2D_Font fontList[MAX_FONTS];
static uint16_t numFonts = 0;
static C2D_Font Clay_Citro2D_GetFont(int32_t id) {
    if (id <= Clay_Citro2D_FONT_SYSTEM || id > numFonts) {
        return NULL;
    }

    return fontList[id - 1];
}

// Registers the specified custom font for use in text rendering.
//
// @return The font identifier if successful, or Clay3DS_FONT_INVALID if the maximum
//         number of registered fonts has been reached.
int32_t Clay_Citro2D_RegisterFont(C2D_Font font) {
    if (font == NULL || numFonts >= MAX_FONTS) {
        return Clay_Citro2D_FONT_INVALID;
    }

    fontList[numFonts++] = font;
    return Clay_Citro2D_FONT_SYSTEM + numFonts;
}

// Measures the dimensions of the specified text string based on the provided configuration.
Clay_Dimensions Clay_Citro2D_MeasureText(Clay_StringSlice slice, Clay_TextElementConfig *config, void *userData) {
    float maxTextWidth = 0.0f;
    float textWidth = 0.0f;

    C2D_Font font = Clay_Citro2D_GetFont(config->fontId);

    const FINF_s *fontInfo = C2D_FontGetInfo(font);
    const float scaleFactor = config->fontSize / (float)fontInfo->height;

    for (int32_t i = 0; i < slice.length;) {
        uint32_t code;
        ssize_t units = decode_utf8(&code, (const uint8_t *)slice.chars + i);

        if (units == -1) {
            code = 0xFFFD;
            units = 1;
        } else if (code == '\0' || i + units > slice.length) {
            break;
        }

        i += units;
        if (code == '\n') {
            maxTextWidth = CLAY__MAX(maxTextWidth, textWidth);
            textWidth = 0;

            continue;
        }

        charWidthInfo_s *info = C2D_FontGetCharWidthInfo(font, C2D_FontGlyphIndexFromCodePoint(font, code));
        textWidth += info->charWidth * scaleFactor;
    }

    return {
        .width = CLAY__MAX(maxTextWidth, textWidth),
        .height = scaleFactor * fontInfo->lineFeed};
}

// Renders the specified render commands to the given render target.
//
// This function should be executed in a loop, after C2D_SceneBegin has been called.
void Clay_Citro2D_Render(C3D_RenderTarget *renderTarget, Clay_Dimensions dimensions, Clay_RenderCommandArray renderCommands) {
    for (uint32_t i = 0; i < renderCommands.length; i++) {
        Clay_RenderCommand *renderCommand = Clay_RenderCommandArray_Get(&renderCommands, i);
        Clay_BoundingBox box = renderCommand->boundingBox;

        switch (renderCommand->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            Clay_RectangleRenderData *config = &renderCommand->renderData.rectangle;
            uint32_t color = CLAY_COLOR_TO_C2D(config->backgroundColor);
            float tlr = config->cornerRadius.topLeft;
            float trr = config->cornerRadius.topRight;
            float brr = config->cornerRadius.bottomRight;
            float blr = config->cornerRadius.bottomLeft;

            if (tlr <= 0.f && trr <= 0.f && brr <= 0.f && blr <= 0.f) {
                // If no rounding is used, fall back to the faster, simpler, rectangle drawing.
                C2D_DrawRectSolid(box.x, box.y, 0.f, box.width, box.height, color);
            } else {
                // Make sure that the rounding is not bigger than half of any side.
                float max = std::min(box.width, box.height) / 2.f;
                tlr = std::min(tlr, max);
                trr = std::min(trr, max);
                brr = std::min(brr, max);
                blr = std::min(blr, max);

                // clang-format off
        Clay_Citro2D_FillQuad(box.x + tlr, box.y,
                           box.x + box.width - trr, box.y,
                           box.x + box.width - trr, box.y + trr,
                           box.x + tlr, box.y + tlr,
                           color); // Top
        Clay_Citro2D_FillQuad(box.x + box.width - trr, box.y + trr,
                           box.x + box.width, box.y + trr,
                           box.x + box.width, box.y + box.height - brr,
                           box.x + box.width - brr, box.y + box.height - brr,
                           color); // Right
        Clay_Citro2D_FillQuad(box.x + blr, box.y + box.height - blr,
                           box.x + box.width - brr, box.y + box.height - brr,
                           box.x + box.width - brr, box.y + box.height,
                           box.x + blr, box.y + box.height,
                           color); // Bottom
        Clay_Citro2D_FillQuad(box.x, box.y + tlr,
                           box.x + tlr, box.y + tlr,
                           box.x + blr, box.y + box.height - blr,
                           box.x, box.y + box.height - blr,
                           color); // Left
        Clay_Citro2D_FillQuad(box.x + tlr, box.y + tlr,
                           box.x + box.width - trr, box.y + trr,
                           box.x + box.width - brr, box.y + box.height - brr,
                           box.x + blr, box.y + box.height - blr,
                           color); // Inner
                // clang-format on

                Clay_Citro2D_FillArc(box.x + tlr, box.y + tlr, tlr, 180.f, 270.f, color);                       // Top Left
                Clay_Citro2D_FillArc(box.x + box.width - trr, box.y + trr, trr, 270.f, 360.f, color);           // Top Right
                Clay_Citro2D_FillArc(box.x + box.width - brr, box.y + box.height - brr, brr, 0.f, 90.f, color); // Bottom Right
                Clay_Citro2D_FillArc(box.x + blr, box.y + box.height - blr, blr, 90.f, 180.f, color);           // Bottom Left
            }

            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData *config = &renderCommand->renderData.border;
            uint32_t c = CLAY_COLOR_TO_C2D(config->color);
            float lw = config->width.left;
            float tw = config->width.top;
            float rw = config->width.right;
            float bw = config->width.bottom;
            // Make sure that the rounding is not bigger than half of any side.
            float max = std::min(box.width, box.height) / 2.f;
            float tlr = std::min(config->cornerRadius.topLeft, max);
            float trr = std::min(config->cornerRadius.topRight, max);
            float brr = std::min(config->cornerRadius.bottomRight, max);
            float blr = std::min(config->cornerRadius.bottomLeft, max);

            if (tw > 0.f) {
                C2D_DrawRectSolid(box.x + tlr, box.y, 0.f, box.width - tlr - trr, tw, c);
            }
            if (lw > 0.f) {
                C2D_DrawRectSolid(box.x, box.y + tlr, 0.f, lw, box.height - tlr - blr, c);
            }
            if (rw > 0.f) {
                C2D_DrawRectSolid(box.x + box.width - rw, box.y + trr, 0.f, rw, box.height - trr - brr, c);
            }
            if (bw > 0.f) {
                C2D_DrawRectSolid(box.x + brr, box.y + box.height - bw, 0.f, box.width - blr - brr, bw, c);
            }
            if (tlr > 0.f) {
                Clay_Citro2D_DrawArc(box.x + tlr, box.y + tlr, tlr - tw / 2.f, 180.f, 270.f, tw, c);
            }
            if (trr > 0.f) {
                Clay_Citro2D_DrawArc(box.x + box.width - trr, box.y + trr, trr - tw / 2.f, 270.f, 360.f, tw, c);
            }
            if (blr > 0.f) {
                Clay_Citro2D_DrawArc(box.x + blr, box.y + box.height - blr, blr - bw / 2.f, 90.f, 180.f, bw, c);
            }
            if (brr > 0.f) {
                Clay_Citro2D_DrawArc(box.x + box.width - brr, box.y + box.height - brr, brr - bw / 2.f, 0.f, 90.f, bw, c);
            }

            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *config = &renderCommand->renderData.text;
            uint32_t color = CLAY_COLOR_TO_C2D(config->textColor);

            Clay_StringSlice string = config->stringContents;
            uint32_t length = std::min<int32_t>(string.length, MAX_TEXT_SIZE);
            memcpy(cvTextBuffer, string.chars, length);
            cvTextBuffer[length] = '\0';

            C2D_Font font = Clay_Citro2D_GetFont(config->fontId);
            float scale = CALC_FONT_SCALE(config->fontSize);

            C2D_Text text;
            C2D_TextBuf buffer = Clay_Citro2D_GetStaticTextBuffer();
            C2D_TextFontParse(&text, font, buffer, cvTextBuffer);
            C2D_TextOptimize(&text);
            C2D_DrawText(&text, C2D_WithColor, box.x, box.y, 0.f, scale, scale, color);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            Clay_ImageRenderData *config = &renderCommand->renderData.image;
            C2D_DrawParams params = {{box.x, box.y, box.width, box.height}, {0.f, 0.f}, 0.f, 0.f};

            if (config->imageData != NULL) {
                C2D_DrawImage(*(C2D_Image *)config->imageData, &params, NULL);
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
            C2D_SceneBegin(renderTarget);
            // clang-format off
      C3D_SetScissor(GPU_SCISSOR_NORMAL,
                     dimensions.height - box.height - box.y,
                     dimensions.width - box.width - box.x,
                     dimensions.height - box.y,
                     dimensions.width - box.x);
            // clang-format on
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
            C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
            break;
        }
        default: {
            fprintf(stderr, "error: unhandled render command: %d\n", renderCommand->commandType);
            exit(1);
        }
        }
    }
}
