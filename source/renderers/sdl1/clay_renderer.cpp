#include "clay_renderer.hpp"
#include "image.hpp"
#include <SDL.h>
#include <SDL_gfxPrimitives.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <clay.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct SDL_FPoint {
    float x;
    float y;
};

struct SDL_FRect {
    float x;
    float y;
    float w;
    float h;
};

struct SDL_Vertex {
    SDL_FPoint position;
    SDL_Color color;
    SDL_FPoint tex_coord;
};

#define CLAY_COLOR_TO_SDL_COLOR_ARGS(color) color.r, color.g, color.b, color.a

Clay_Dimensions SDL_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    SDL_Font *fonts = (SDL_Font *)userData;
    TTF_Font *font = fonts[config->fontId].font;

    char *chars = (char *)calloc(text.length + 1, 1);
    memcpy(chars, text.chars, text.length);
    int width = 0;
    int height = 0;
    if (TTF_SizeUTF8(font, chars, &width, &height) < 0) {
        fprintf(stderr, "Error: could not measure text: %s\n", TTF_GetError());
        exit(1);
    }
    free(chars);
    return (Clay_Dimensions){
        .width = (float)width,
        .height = (float)height,
    };
}

static constexpr int NUM_CIRCLE_SEGMENTS = 16;

static inline void SDL_Clay_AddQuadIndices(int *indices, int *indexCount, int topLeft, int topRight, int bottomRight, int bottomLeft) {
    indices[(*indexCount)++] = bottomLeft;
    indices[(*indexCount)++] = topLeft;
    indices[(*indexCount)++] = topRight;
    indices[(*indexCount)++] = bottomRight;
    indices[(*indexCount)++] = bottomLeft;
    indices[(*indexCount)++] = topRight;
}

static void SDL1_RenderGeometryFallback(SDL_Surface *surface, const SDL_Vertex *vertices, const int *indices, int indexCount, SDL_Color color) {
    for (int i = 0; i < indexCount; i += 3) {
        int idx0 = indices[i];
        int idx1 = indices[i + 1];
        int idx2 = indices[i + 2];

        int16_t vx[3] = {
            static_cast<int16_t>(vertices[idx0].position.x),
            static_cast<int16_t>(vertices[idx1].position.x),
            static_cast<int16_t>(vertices[idx2].position.x)};
        int16_t vy[3] = {
            static_cast<int16_t>(vertices[idx0].position.y),
            static_cast<int16_t>(vertices[idx1].position.y),
            static_cast<int16_t>(vertices[idx2].position.y)};

        filledPolygonRGBA(surface, vx, vy, 3, color.r, color.g, color.b, color.unused);
    }
}

static void SDL_RenderFillRoundedRect(SDL_Surface *surface, const SDL_FRect rect, const Clay_CornerRadius cornerRadius, const Clay_Color _color) {
    const SDL_Color color = (SDL_Color){
        .r = (Uint8)_color.r,
        .g = (Uint8)_color.g,
        .b = (Uint8)_color.b,
        .unused = (Uint8)_color.a,
    };

    int indexCount = 0, vertexCount = 0;

    enum corner_e {
        TOP_LEFT,
        TOP_RIGHT,
        BOTTOM_RIGHT,
        BOTTOM_LEFT,
    };

    const float maxRadius = std::min(rect.w, rect.h) / 2.f;
    const float clampedRadius[4] = {
        std::min(cornerRadius.topLeft, maxRadius),
        std::min(cornerRadius.topRight, maxRadius),
        std::min(cornerRadius.bottomRight, maxRadius),
        std::min(cornerRadius.bottomLeft, maxRadius),
    };

    int numCircleSegments[4];
    int totalVertices = 4 + 4 + 2 * 4;
    int totalIndices = 6 + 6 * 8;
    for (unsigned i = 0; i < 4; i++) {
        const int n = std::max(NUM_CIRCLE_SEGMENTS, (int)(clampedRadius[i] * 0.5f));
        numCircleSegments[i] = n;
        totalVertices += n * 2;
        totalIndices += n * 3;
    }

    SDL_Vertex *vertices = (SDL_Vertex *)malloc(totalVertices * sizeof(SDL_Vertex));
    int *indices = (int *)malloc(totalIndices * sizeof(int));

    const float innerLeft = rect.x + std::max(clampedRadius[TOP_LEFT], clampedRadius[BOTTOM_LEFT]);
    const float innerTop = rect.y + std::max(clampedRadius[TOP_LEFT], clampedRadius[TOP_RIGHT]);
    const float innerRight = rect.x + rect.w - std::max(clampedRadius[TOP_RIGHT], clampedRadius[BOTTOM_RIGHT]);
    const float innerBottom = rect.y + rect.h - std::max(clampedRadius[BOTTOM_RIGHT], clampedRadius[BOTTOM_LEFT]);

    vertices[vertexCount++] = (SDL_Vertex){{innerLeft, innerTop}, color, {0, 0}};
    vertices[vertexCount++] = (SDL_Vertex){{innerRight, innerTop}, color, {1, 0}};
    vertices[vertexCount++] = (SDL_Vertex){{innerRight, innerBottom}, color, {1, 1}};
    vertices[vertexCount++] = (SDL_Vertex){{innerLeft, innerBottom}, color, {0, 1}};
    SDL_Clay_AddQuadIndices(indices, &indexCount, 0, 1, 2, 3);

    const SDL_FPoint cornerCenter[4] = {
        {rect.x + clampedRadius[TOP_LEFT], rect.y + clampedRadius[TOP_LEFT]},
        {rect.x + rect.w - clampedRadius[TOP_RIGHT], rect.y + clampedRadius[TOP_RIGHT]},
        {rect.x + rect.w - clampedRadius[BOTTOM_RIGHT], rect.y + rect.h - clampedRadius[BOTTOM_RIGHT]},
        {rect.x + clampedRadius[BOTTOM_LEFT], rect.y + rect.h - clampedRadius[BOTTOM_LEFT]},
    };

    vertices[vertexCount++] = (SDL_Vertex){cornerCenter[TOP_LEFT], color, {0, 0}};
    vertices[vertexCount++] = (SDL_Vertex){cornerCenter[TOP_RIGHT], color, {0, 0}};
    vertices[vertexCount++] = (SDL_Vertex){cornerCenter[BOTTOM_RIGHT], color, {0, 0}};
    vertices[vertexCount++] = (SDL_Vertex){cornerCenter[BOTTOM_LEFT], color, {0, 0}};

    int cornerStartIndex[4];

    for (int i = 0; i < 4; i++) {
        const float step = (M_PI / 2) / numCircleSegments[i];
        SDL_FPoint signedRadius;

        switch (i) {
        case TOP_LEFT:
            signedRadius = (SDL_FPoint){-clampedRadius[i], -clampedRadius[i]};
            break;
        case TOP_RIGHT:
            signedRadius = (SDL_FPoint){clampedRadius[i], -clampedRadius[i]};
            break;
        case BOTTOM_RIGHT:
            signedRadius = (SDL_FPoint){clampedRadius[i], clampedRadius[i]};
            break;
        case BOTTOM_LEFT:
            signedRadius = (SDL_FPoint){-clampedRadius[i], clampedRadius[i]};
            break;
        default:
            free(vertices);
            free(indices);
            return;
        }
        cornerStartIndex[i] = vertexCount;
        vertices[vertexCount++] = (SDL_Vertex){{cornerCenter[i].x + signedRadius.x, cornerCenter[i].y}, color, {0, 0}};

        for (int j = 0; j < numCircleSegments[i]; j++) {
            const float angle = ((float)j + 1.0f) * step;
            vertices[vertexCount++] = (SDL_Vertex){{cornerCenter[i].x + cosf(angle) * signedRadius.x, cornerCenter[i].y + sinf(angle) * signedRadius.y}, color, {0, 0}};

            indices[indexCount++] = 4 + i;
            indices[indexCount++] = vertexCount - 2;
            indices[indexCount++] = vertexCount - 1;
        }
    }

    vertices[vertexCount++] = (SDL_Vertex){{cornerCenter[TOP_RIGHT].x, innerTop}, color, {1, 0}};
    vertices[vertexCount++] = (SDL_Vertex){{cornerCenter[TOP_LEFT].x, innerTop}, color, {0, 0}};
    int cornerTLLastIndex = cornerStartIndex[TOP_LEFT] + numCircleSegments[TOP_LEFT];
    int cornerTRLastIndex = cornerStartIndex[TOP_RIGHT] + numCircleSegments[TOP_RIGHT];
    SDL_Clay_AddQuadIndices(indices, &indexCount, cornerTLLastIndex, cornerTRLastIndex, vertexCount - 2, vertexCount - 1);

    vertices[vertexCount++] = (SDL_Vertex){{cornerCenter[BOTTOM_LEFT].x, innerBottom}, color, {0, 1}};
    vertices[vertexCount++] = (SDL_Vertex){{cornerCenter[BOTTOM_RIGHT].x, innerBottom}, color, {1, 1}};
    int cornerBRLastIndex = cornerStartIndex[BOTTOM_RIGHT] + numCircleSegments[BOTTOM_RIGHT];
    int cornerBLLastIndex = cornerStartIndex[BOTTOM_LEFT] + numCircleSegments[BOTTOM_LEFT];
    SDL_Clay_AddQuadIndices(indices, &indexCount, vertexCount - 2, vertexCount - 1, cornerBRLastIndex, cornerBLLastIndex);

    vertices[vertexCount++] = (SDL_Vertex){{rect.x + rect.w, innerTop}, color, {1, 0}};
    vertices[vertexCount++] = (SDL_Vertex){{rect.x + rect.w, innerBottom}, color, {1, 1}};
    SDL_Clay_AddQuadIndices(indices, &indexCount, 1, vertexCount - 2, vertexCount - 1, 2);
    SDL_Clay_AddQuadIndices(indices, &indexCount, 4 + TOP_RIGHT, cornerStartIndex[TOP_RIGHT], vertexCount - 2, vertexCount - 6);
    SDL_Clay_AddQuadIndices(indices, &indexCount, vertexCount - 3, vertexCount - 1, cornerStartIndex[BOTTOM_RIGHT], 4 + BOTTOM_RIGHT);

    vertices[vertexCount++] = (SDL_Vertex){{rect.x, innerTop}, color, {0, 1}};
    vertices[vertexCount++] = (SDL_Vertex){{rect.x, innerBottom}, color, {0, 0}};
    SDL_Clay_AddQuadIndices(indices, &indexCount, vertexCount - 2, 0, 3, vertexCount - 1);
    SDL_Clay_AddQuadIndices(indices, &indexCount, cornerStartIndex[TOP_LEFT], 4 + TOP_LEFT, vertexCount - 7, vertexCount - 2);
    SDL_Clay_AddQuadIndices(indices, &indexCount, vertexCount - 1, vertexCount - 6, 4 + BOTTOM_LEFT, cornerStartIndex[BOTTOM_LEFT]);

    SDL1_RenderGeometryFallback(surface, vertices, indices, indexCount, color);

    free(vertices);
    free(indices);
}

static void SDL_RenderCornerBorder(SDL_Surface *surface, Clay_BoundingBox *boundingBox, Clay_BorderRenderData *config, int cornerIndex, Clay_Color _color) {
    const SDL_Color color = (SDL_Color){
        .r = (Uint8)_color.r,
        .g = (Uint8)_color.g,
        .b = (Uint8)_color.b,
        .unused = (Uint8)_color.a,
    };

    float centerX, centerY, outerRadius, startAngle, borderWidth;
    const float maxRadius = std::min(boundingBox->width, boundingBox->height) / 2.0f;

    SDL_Vertex vertices[512];
    int indices[512];
    int indexCount = 0, vertexCount = 0;

    switch (cornerIndex) {
    case (0):
        startAngle = M_PI;
        outerRadius = std::min(config->cornerRadius.topLeft, maxRadius);
        centerX = boundingBox->x + outerRadius;
        centerY = boundingBox->y + outerRadius;
        borderWidth = config->width.top;
        break;
    case (1):
        startAngle = 3 * M_PI / 2;
        outerRadius = std::min(config->cornerRadius.topRight, maxRadius);
        centerX = boundingBox->x + boundingBox->width - outerRadius;
        centerY = boundingBox->y + outerRadius;
        borderWidth = config->width.top;
        break;
    case (2):
        startAngle = 0;
        outerRadius = std::min(config->cornerRadius.bottomRight, maxRadius);
        centerX = boundingBox->x + boundingBox->width - outerRadius;
        centerY = boundingBox->y + outerRadius;
        centerY = boundingBox->y + boundingBox->height - outerRadius;
        borderWidth = config->width.bottom;
        break;
    case (3):
        startAngle = M_PI / 2;
        outerRadius = std::min(config->cornerRadius.bottomLeft, maxRadius);
        centerX = boundingBox->x + outerRadius;
        centerY = boundingBox->y + boundingBox->height - outerRadius;
        borderWidth = config->width.bottom;
        break;
    default:
        break;
    }

    const float innerRadius = outerRadius - borderWidth;
    const int minNumOuterTriangles = NUM_CIRCLE_SEGMENTS;
    const int numOuterTriangles = std::max(minNumOuterTriangles, (int)ceilf(outerRadius * 0.5f));
    const float angleStep = M_PI / (2.0 * (float)numOuterTriangles);

    for (int i = 0; i < numOuterTriangles; i++) {
        float angle1 = startAngle + i * angleStep;
        float angle2 = startAngle + ((float)i + 0.5) * angleStep;
        float angle3 = startAngle + (i + 1) * angleStep;

        if (i == 0) {
            vertices[vertexCount++] = (SDL_Vertex){{centerX + cosf(angle1) * outerRadius, centerY + sinf(angle1) * outerRadius}, color, {0, 0}};
        }
        indices[indexCount++] = vertexCount - 1;

        vertices[vertexCount++] = (innerRadius > 0) ? (SDL_Vertex){{centerX + cosf(angle2) * (innerRadius), centerY + sinf(angle2) * (innerRadius)}, color, {0, 0}} : (SDL_Vertex){{centerX, centerY}, color, {0, 0}};
        indices[indexCount++] = vertexCount - 1;

        vertices[vertexCount++] = (SDL_Vertex){{centerX + cosf(angle3) * outerRadius, centerY + sinf(angle3) * outerRadius}, color, {0, 0}};
        indices[indexCount++] = vertexCount - 1;
    }

    if (innerRadius > 0) {
        for (int i = 0; i < numOuterTriangles - 1; i++) {
            if (i == 0) {
                indices[indexCount++] = 1;
                indices[indexCount++] = 2;
                indices[indexCount++] = 3;
            } else {
                int baseIndex = 3;
                indices[indexCount++] = baseIndex + (i - 1) * 2;
                indices[indexCount++] = baseIndex + (i - 1) * 2 + 1;
                indices[indexCount++] = baseIndex + (i - 1) * 2 + 2;
            }
        }

        float endAngle = startAngle + M_PI / 2.0;

        indices[indexCount++] = vertexCount - 2;
        indices[indexCount++] = vertexCount - 1;
        vertices[vertexCount++] = (SDL_Vertex){{centerX + cosf(endAngle) * innerRadius, centerY + sinf(endAngle) * innerRadius}, color, {0, 0}};
        indices[indexCount++] = vertexCount - 1;

        indices[indexCount++] = 0;
        indices[indexCount++] = 1;
        vertices[vertexCount++] = (SDL_Vertex){{centerX + cosf(startAngle) * innerRadius, centerY + sinf(startAngle) * innerRadius}, color, {0, 0}};
        indices[indexCount++] = vertexCount - 1;
    }

    SDL1_RenderGeometryFallback(surface, vertices, indices, indexCount, color);
}

SDL_Rect currentClippingRectangle;

void Clay_SDL_Render(SDL_Surface *surface, Clay_RenderCommandArray renderCommands, SDL_Font *fonts) {
    for (uint32_t i = 0; i < renderCommands.length; i++) {
        Clay_RenderCommand *renderCommand = Clay_RenderCommandArray_Get(&renderCommands, i);
        Clay_BoundingBox boundingBox = renderCommand->boundingBox;
        switch (renderCommand->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            Clay_RectangleRenderData *config = &renderCommand->renderData.rectangle;
            Clay_Color color = config->backgroundColor;

            SDL_FRect rect = (SDL_FRect){
                .x = boundingBox.x,
                .y = boundingBox.y,
                .w = boundingBox.width,
                .h = boundingBox.height,
            };
            if (config->cornerRadius.topLeft > 1.f || config->cornerRadius.topRight > 1.f || config->cornerRadius.bottomLeft > 1.f || config->cornerRadius.bottomRight > 1.f) {
                SDL_RenderFillRoundedRect(surface, rect, config->cornerRadius, color);
            } else {
                boxRGBA(surface, (int16_t)rect.x, (int16_t)rect.y, (int16_t)(rect.x + rect.w), (int16_t)(rect.y + rect.h), color.r, color.g, color.b, color.a);
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *config = &renderCommand->renderData.text;
            char *cloned = (char *)calloc(config->stringContents.length + 1, 1);
            memcpy(cloned, config->stringContents.chars, config->stringContents.length);
            TTF_Font *font = fonts[config->fontId].font;

            SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, cloned, (SDL_Color){
                                                                                .r = (Uint8)config->textColor.r,
                                                                                .g = (Uint8)config->textColor.g,
                                                                                .b = (Uint8)config->textColor.b,
                                                                                .unused = (Uint8)config->textColor.a,
                                                                            });

            SDL_Rect destination = (SDL_Rect){
                .x = (int16_t)boundingBox.x,
                .y = (int16_t)boundingBox.y,
                .w = (Uint16)boundingBox.width,
                .h = (Uint16)boundingBox.height,
            };

            SDL_BlitSurface(textSurface, NULL, surface, &destination);
            SDL_FreeSurface(textSurface);
            free(cloned);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
            currentClippingRectangle = (SDL_Rect){
                .x = (int16_t)boundingBox.x,
                .y = (int16_t)boundingBox.y,
                .w = (Uint16)boundingBox.width,
                .h = (Uint16)boundingBox.height,
            };
            SDL_SetClipRect(surface, &currentClippingRectangle);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
            SDL_SetClipRect(surface, NULL);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            Clay_ImageRenderData *config = &renderCommand->renderData.image;

            if (config->imageData != NULL) {
                auto &image = *(Image *)config->imageData;

                ImageRenderParams params;
                params.centered = false;
                params.x = boundingBox.x;
                params.y = boundingBox.y;
                params.scale = boundingBox.width / static_cast<double>(image.getWidth());

                image.render(params);
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData *config = &renderCommand->renderData.border;
            Clay_Color color = config->color;

            if (boundingBox.width > 0 && boundingBox.height > 0) {
                const float maxRadius = std::min(boundingBox.width, boundingBox.height) / 2.0f;

                if (config->width.left > 0) {
                    const float clampedRadiusTop = std::min((float)config->cornerRadius.topLeft, maxRadius);
                    const float clampedRadiusBottom = std::min((float)config->cornerRadius.bottomLeft, maxRadius);
                    int16_t x1 = (int16_t)boundingBox.x;
                    int16_t y1 = (int16_t)(boundingBox.y + clampedRadiusTop);
                    int16_t x2 = (int16_t)(boundingBox.x + config->width.left);
                    int16_t y2 = (int16_t)(boundingBox.y + boundingBox.height - clampedRadiusBottom);
                    boxRGBA(surface, x1, y1, x2, y2, color.r, color.g, color.b, color.a);
                }

                if (config->width.right > 0) {
                    const float clampedRadiusTop = std::min((float)config->cornerRadius.topRight, maxRadius);
                    const float clampedRadiusBottom = std::min((float)config->cornerRadius.bottomRight, maxRadius);
                    int16_t x1 = (int16_t)(boundingBox.x + boundingBox.width - config->width.right);
                    int16_t y1 = (int16_t)(boundingBox.y + clampedRadiusTop);
                    int16_t x2 = (int16_t)(boundingBox.x + boundingBox.width);
                    int16_t y2 = (int16_t)(boundingBox.y + boundingBox.height - clampedRadiusBottom);
                    boxRGBA(surface, x1, y1, x2, y2, color.r, color.g, color.b, color.a);
                }

                if (config->width.top > 0) {
                    const float clampedRadiusLeft = std::min((float)config->cornerRadius.topLeft, maxRadius);
                    const float clampedRadiusRight = std::min((float)config->cornerRadius.topRight, maxRadius);
                    int16_t x1 = (int16_t)(boundingBox.x + clampedRadiusLeft);
                    int16_t y1 = (int16_t)boundingBox.y;
                    int16_t x2 = (int16_t)(boundingBox.x + boundingBox.width - clampedRadiusRight);
                    int16_t y2 = (int16_t)(boundingBox.y + config->width.top);
                    boxRGBA(surface, x1, y1, x2, y2, color.r, color.g, color.b, color.a);
                }

                if (config->width.bottom > 0) {
                    const float clampedRadiusLeft = std::min((float)config->cornerRadius.bottomLeft, maxRadius);
                    const float clampedRadiusRight = std::min((float)config->cornerRadius.bottomRight, maxRadius);
                    int16_t x1 = (int16_t)(boundingBox.x + clampedRadiusLeft);
                    int16_t y1 = (int16_t)(boundingBox.y + boundingBox.height - config->width.bottom);
                    int16_t x2 = (int16_t)(boundingBox.x + boundingBox.width - clampedRadiusRight);
                    int16_t y2 = (int16_t)(boundingBox.y + boundingBox.height);
                    boxRGBA(surface, x1, y1, x2, y2, color.r, color.g, color.b, color.a);
                }

                if (config->width.top > 0 && config->cornerRadius.topLeft > 0) {
                    SDL_RenderCornerBorder(surface, &boundingBox, config, 0, config->color);
                }
                if (config->width.top > 0 && config->cornerRadius.topRight > 0) {
                    SDL_RenderCornerBorder(surface, &boundingBox, config, 1, config->color);
                }
                if (config->width.bottom > 0 && config->cornerRadius.bottomRight > 0) {
                    SDL_RenderCornerBorder(surface, &boundingBox, config, 2, config->color);
                }
                if (config->width.bottom > 0 && config->cornerRadius.bottomLeft > 0) {
                    SDL_RenderCornerBorder(surface, &boundingBox, config, 3, config->color);
                }
            }
            break;
        }
        default: {
            fprintf(stderr, "Error: unhandled render command: %d\n", renderCommand->commandType);
            exit(1);
        }
        }
    }
}
