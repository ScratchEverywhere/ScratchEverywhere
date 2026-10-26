#include "loading.hpp"
#include "image.hpp"
#include <algorithm>
#include <memory>
#include <renderer.hpp>
#include <string>
#include <unzip.hpp>

void Loading::init() {
    auto loadBlock = [&](const uint8_t number) -> std::shared_ptr<Image> {
        auto image = createImageFromFile("gfx/menu/block" + std::to_string(number) + ".svg", false);
        if (!image.has_value()) {
            renderBlocks = false;
            return nullptr;
        }
        return image.value();
    };

    block1 = loadBlock(1);
    block2 = loadBlock(2);
    block3 = loadBlock(3);
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

    if (renderBlocks) {
        constexpr float blockScale = 0.5f;

        // Left-align the blocks as a group, and center that group on screen.
        const float w1 = block1->getWidth() * blockScale;
        const float w2 = block2->getWidth() * blockScale;
        const float w3 = block3->getWidth() * blockScale;
        const float groupWidth = std::max({w1, w2, w3});
        const float groupLeft = (Render::getWidth() - groupWidth) / 2.0f;

        // Images are rendered centered, so offset each by half its own width.
        ImageRenderParams p1 = {
            .x = groupLeft + w1 / 2.0f,
            .y = block1Y,
            .scale = blockScale,
            .centered = true};
        ImageRenderParams p2 = {
            .x = groupLeft + w2 / 2.0f,
            .y = block2Y,
            .scale = blockScale,
            .centered = true};
        ImageRenderParams p3 = {
            .x = groupLeft + w3 / 2.0f,
            .y = block3Y,
            .scale = blockScale,
            .centered = true};

        block1->render(p1);
        block2->render(p2);
        block3->render(p3);
    }

#ifdef __XBOX__
    loadingStateText->setText(Unzip::getLoadingState());
#else
    loadingStateText->setText(Unzip::loadingState);
#endif
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

    block1.reset();
    block2.reset();
    block3.reset();
    loadingStateText.reset();
}
