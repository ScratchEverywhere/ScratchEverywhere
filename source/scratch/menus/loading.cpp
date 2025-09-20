#include "loading.hpp"
#include "../render.hpp"
#include "../unzip.hpp"

void Loading::init() {
    block1 = new Image("gfx/menu/block1.svg");
    block2 = new Image("gfx/menu/block2.svg");
    block3 = new Image("gfx/menu/block3.svg");
    loadingStateText = createTextObject("", 0, 0);
    block1->scale = 0.5;
    block2->scale = 0.5;
    block3->scale = 0.5;
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

    block1->render((Render::getWidth() / 2), block1Y, true);
    block2->render((Render::getWidth() / 2) - 25, block2Y, true);
    block3->render((Render::getWidth() / 2) - 10, block3Y, true);

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
    delete block1;
    delete block2;
    delete block3;
    delete loadingStateText;
}