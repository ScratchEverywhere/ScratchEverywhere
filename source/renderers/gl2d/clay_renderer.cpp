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

#define MAX_FONTS 3
// is this too low of a limit?
#define MAX_TEXT_SIZE 128

static char cvTextBuffer[MAX_TEXT_SIZE];
static std::vector<std::unique_ptr<TextObject>> fontList;
static uint16_t numFonts = 0;

static inline unsigned int CLAY_COLOR_TO_GL2D(const Clay_Color &color) {
    int r5 = (int)color.r >> 3;
    int g5 = (int)color.g >> 3;
    int b5 = (int)color.b >> 3;
    return RGB15(r5, g5, b5);
}

bool Clay_GL2D_RegisterFont(const std::string &fontPath) {
    if (numFonts >= MAX_FONTS) return false;
    std::unique_ptr<TextObject> obj = createTextObject("", 0, 0, fontPath);
    obj->setCenterAligned(false);
    numFonts++;
    fontList.push_back(std::move(obj));
    return true;
}

Clay_Dimensions Clay_GL2D_MeasureText(Clay_StringSlice slice, Clay_TextElementConfig *config, void *userData) {
    if (numFonts == 0 || config->fontId + 1 > numFonts) {
        return {.width = 0.0f, .height = 0.0f};
    }

    const auto &obj = fontList[config->fontId];

    uint32_t length = std::min<int32_t>(slice.length, MAX_TEXT_SIZE);
    memcpy(cvTextBuffer, slice.chars, length);
    cvTextBuffer[length] = '\0';

    obj->setScale((float)config->fontSize / 16.0 /* <- gl2d default text size*/);
    const auto size = obj->getStringSize(cvTextBuffer);

    return {
        .width = size[0],
        .height = size[1]};
}

void Clay_GL2D_Render(Clay_Dimensions dimensions, Clay_RenderCommandArray renderCommands) {
    for (uint32_t i = 0; i < renderCommands.length; i++) {
        Clay_RenderCommand *renderCommand = Clay_RenderCommandArray_Get(&renderCommands, i);
        Clay_BoundingBox box = renderCommand->boundingBox;

        switch (renderCommand->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            Clay_RectangleRenderData *config = &renderCommand->renderData.rectangle;
            unsigned int color = CLAY_COLOR_TO_GL2D(config->backgroundColor);

            glBoxFilled(
                box.x,
                box.y,
                box.x + box.width,
                box.y + box.height,
                color);

            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            // TODO: implement
            Log::logWarning("Clay: Border command not implemented!");
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *config = &renderCommand->renderData.text;

            if (numFonts == 0 || config->fontId + 1 > numFonts) {
                break;
            }

            uint32_t color = CLAY_COLOR_TO_GL2D(config->textColor);
            const auto &obj = fontList[config->fontId];

            Clay_StringSlice string = config->stringContents;
            uint32_t length = std::min<int32_t>(string.length, MAX_TEXT_SIZE);
            memcpy(cvTextBuffer, string.chars, length);
            cvTextBuffer[length] = '\0';

            obj->setColor(color);
            obj->setScale((float)config->fontSize / 16.0 /* <- gl2d default text size*/);
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
            // TODO: implement
            Log::logWarning("Clay: Scissor start command not implemented!");

            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
            // TODO: implement
            Log::logWarning("Clay: Scissor end command not implemented!");
            break;
        }
        default: {
            Log::logWarning("Clay: Unhandled render command: " + std::to_string(renderCommand->commandType));
        }
        }
    }
}
