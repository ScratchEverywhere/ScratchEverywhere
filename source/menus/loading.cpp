#include "loading.hpp"
#include "image.hpp"
#include "log.hpp"
#include "render.hpp"
#include "unzip.hpp"
#include <algorithm>
#include <cmath>
#include <string>

LoadingMenu::LoadingMenu(void *userdata) {
    block1 = createImageFromFile("gfx/menu/block1.svg", false).value(); // TODO: Error handling
    block2 = createImageFromFile("gfx/menu/block2.svg", false).value(); // TODO: Error handling
    block3 = createImageFromFile("gfx/menu/block3.svg", false).value(); // TODO: Error handling

    block1Y = -1.0f;
    block2Y = -1.0f;
    block3Y = -1.0f;
    endDelayTimer = 0.0f;

    deltaTime.start();
}

void LoadingMenu::render() {
    if (block1Y < 0.0f) {
        block1Y = Render::getHeight() * 2.0f;
        block2Y = Render::getHeight() * 2.0f;
        block3Y = Render::getHeight() * 2.0f;
    }

    const float delta = deltaTime.getTimeMs() / 1000.0f;
    deltaTime.start();
    const float lerpSpeed = 12.0f * delta;

    float w1 = block1->getWidth() * 0.5f * menuManager->scale;
    float h1 = block1->getHeight() * 0.5f * menuManager->scale;
    float w2 = block2->getWidth() * 0.5f * menuManager->scale;
    float h2 = block2->getHeight() * 0.5f * menuManager->scale;
    float w3 = block3->getWidth() * 0.5f * menuManager->scale;
    float h3 = block3->getHeight() * 0.5f * menuManager->scale;

    const float spacing = 2.0f * menuManager->scale;
    const float targetY1 = (Render::getHeight() * 0.3f);
    const float targetY2 = targetY1 + h1 + spacing;
    const float targetY3 = targetY2 + h2 + spacing;

    if (std::abs(block1Y - targetY1) > 0.5f) {
        block1Y += (targetY1 - block1Y) * lerpSpeed;
    } else {
        block1Y = targetY1;

        if (std::abs(block2Y - targetY2) > 0.5f) {
            block2Y += (targetY2 - block2Y) * lerpSpeed;
        } else {
            block2Y = targetY2;

            if (std::abs(block3Y - targetY3) > 0.5f) {
                block3Y += (targetY3 - block3Y) * lerpSpeed;
            } else {
                block3Y = targetY3;

                endDelayTimer += delta;
                if (endDelayTimer >= 0.5f) {
                    block1Y = Render::getHeight() * 2.0f;
                    block2Y = Render::getHeight() * 2.0f;
                    block3Y = Render::getHeight() * 2.0f;
                    endDelayTimer = 0.0f;
                }
            }
        }
    }

    float maxBlockWidth = std::max({w1, w2, w3});
    float leftX = (Render::getWidth() - maxBlockWidth) / 2.0f;

    Clay_String loadingText = {
        .length = (int)Unzip::loadingState.length(),
        .chars = Unzip::loadingState.c_str()};

    // clang-format off
    CLAY(CLAY_ID("main"), (Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        }
    }) {
        CLAY(CLAY_IDI_LOCAL("block", 1), (Clay_ElementDeclaration){
            .layout = { .sizing = { .width = CLAY_SIZING_FIXED(w1), .height = CLAY_SIZING_FIXED(h1) } },
            .image = { .imageData = block1.get() },
            .floating = {
                .offset = { .x = leftX, .y = block1Y },
                .attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP },
                .attachTo = CLAY_ATTACH_TO_PARENT,
            },
        }) {}

        CLAY(CLAY_IDI_LOCAL("block", 2), (Clay_ElementDeclaration){
            .layout = { .sizing = { .width = CLAY_SIZING_FIXED(w2), .height = CLAY_SIZING_FIXED(h2) } },
            .image = { .imageData = block2.get() },
            .floating = {
                .offset = { .x = leftX, .y = block2Y },
                .attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP },
                .attachTo = CLAY_ATTACH_TO_PARENT,
            },
        }) {}

        CLAY(CLAY_IDI_LOCAL("block", 3), (Clay_ElementDeclaration){
            .layout = { .sizing = { .width = CLAY_SIZING_FIXED(w3), .height = CLAY_SIZING_FIXED(h3) } },
            .image = { .imageData = block3.get() },
            .floating = {
                .offset = { .x = leftX, .y = block3Y },
                .attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP },
                .attachTo = CLAY_ATTACH_TO_PARENT,
            },
        }) {}

        CLAY(CLAY_ID_LOCAL("spacer"), (Clay_ElementDeclaration){
            .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } }
        }) {}

        CLAY(CLAY_ID_LOCAL("status"), (Clay_ElementDeclaration){
            .layout = { 
                .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) },
                .padding = { .bottom = static_cast<uint16_t>(Render::getHeight() * 0.2f) } 
            }
        }) {
            CLAY_TEXT(loadingText, CLAY_TEXT_CONFIG({
                .textColor = { 255, 255, 255, 255 },
                .fontSize = static_cast<uint16_t>(16 * menuManager->scale)
            }));
        }
    }
    // clang-format on
}
