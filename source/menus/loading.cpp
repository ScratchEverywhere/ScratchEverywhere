#include "loading.hpp"
#include <render.hpp>
#include <unzip.hpp>

void Loading::init() {
    try {
        block1 = createImageFromFile("gfx/menu/block1.svg", false);
        block2 = createImageFromFile("gfx/menu/block2.svg", false);
        block3 = createImageFromFile("gfx/menu/block3.svg", false);
    } catch (std::runtime_error &e) {
        renderBlocks = false;
    }
    loadingStateText = createTextObject("", 0, 0);
    block1Y = Render::getHeight() * 2;
    block2Y = Render::getHeight() * 2;
    block3Y = Render::getHeight() * 2;
    deltaTime.start();
}
void Loading::render() {
    float delta = deltaTime.getTimeMs() / 1000.0f;
    deltaTime.start();
    const float lerpSpeed = 12.0f * delta;

    float targetY1 = (Render::getHeight() * 0.3);
    float targetY2 = (Render::getHeight() * 0.3) + 30;
    float targetY3 = (Render::getHeight() * 0.3) + 60;

    if (abs(block1Y - targetY1) > 2) {
        block1Y += (targetY1 - block1Y) * lerpSpeed;
    } else if (abs(block2Y - targetY2) > 2) {
        block2Y += (targetY2 - block2Y) * lerpSpeed;
    } else if (abs(block3Y - targetY3) > 2) {
        block3Y += (targetY3 - block3Y) * lerpSpeed;
    } else {
        // reset so animation loops
        block1Y = Render::getHeight() * 2;
        block2Y = Render::getHeight() * 2;
        block3Y = Render::getHeight() * 2;
    }

    if (Render::renderMode != Render::BOTTOM_SCREEN_ONLY)
        Render::beginFrame(0, 0, 0, 0);
    else Render::beginFrame(1, 0, 0, 0);

    ImageRenderParams p1 = {
        .x = Render::getWidth() / 2,
        .y = static_cast<int>(block1Y),
        .scale = 0.5f,
        .centered = true};
    ImageRenderParams p2 = {
        .x = Render::getWidth() / 2,
        .y = static_cast<int>(block2Y),
        .scale = 0.5f,
        .centered = true};
    ImageRenderParams p3 = {
        .x = Render::getWidth() / 2,
        .y = static_cast<int>(block3Y),
        .scale = 0.5f,
        .centered = true};

    if (renderBlocks) {
        block1->render(p1);
        block2->render(p2);
        block3->render(p3);
    }

    loadingStateText->setText(Unzip::loadingState);
    loadingStateText->render(Render::getWidth() / 2, Render::getHeight() * 0.8);

    if (Render::renderMode != Render::BOTTOM_SCREEN_ONLY)
        Render::beginFrame(1, 0, 0, 0);
    else Render::beginFrame(0, 0, 0, 0);

    Render::endFrame();
}

void Loading::cleanup() {
    if (Render::renderMode != Render::BOTTOM_SCREEN_ONLY)
        Render::beginFrame(0, 0, 0, 0);
    else Render::beginFrame(1, 0, 0, 0);

    loadingStateText->render(Render::getWidth() / 2, Render::getHeight() * 0.8);

    if (Render::renderMode != Render::BOTTOM_SCREEN_ONLY)
        Render::beginFrame(1, 0, 0, 0);
    else Render::beginFrame(0, 0, 0, 0);

    Render::endFrame();
}
