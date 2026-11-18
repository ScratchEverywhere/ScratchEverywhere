#include "loadingMenu.hpp"
#include "menuManager.hpp"
#include <algorithm>
#include <cmath>
#include <image.hpp>
#include <renderer.hpp>
#include <string>
#include <unzip.hpp>

LoadingMenu::LoadingMenu() {
    MenuManager::canChangeTabs = false;

    for (int i = 0; i < 3; i++) {
        const std::string path = "gfx/menu/block" + std::to_string(i + 1) + ".svg";
        if (MenuManager::elements && MenuManager::elements->imageData.count(path)) {
            blockWidths[i] = MenuManager::elements->imageData[path]->getWidth();
        } else {
            auto img = createImageFromFile(path, false, false, 1.0f);
            if (img.has_value()) {
                blockWidths[i] = img.value()->getWidth();
                if (MenuManager::elements) {
                    MenuManager::elements->imageData.emplace(path, std::move(img.value()));
                }
            } else {
                hasBlocks = false;
            }
        }
    }

    const float startY = (static_cast<float>(Render::getHeight()) / MenuManager::builder.scale) * 2.0f;
    blockY.fill(startY);
    deltaTime.start();
}

LoadingMenu::~LoadingMenu() {
    MenuManager::elements->imageData.erase("gfx/menu/block1.svg");
    MenuManager::elements->imageData.erase("gfx/menu/block2.svg");
    MenuManager::elements->imageData.erase("gfx/menu/block3.svg");
    MenuManager::canChangeTabs = true;
}

void LoadingMenu::build(UI::Builder &b) {
    if (hasBlocks) {
        constexpr float blockScale = 0.5f;
        const float virtualW = static_cast<float>(Render::getWidth()) / b.scale;

        const float w1 = blockWidths[0] * blockScale;
        const float w2 = blockWidths[1] * blockScale;
        const float w3 = blockWidths[2] * blockScale;
        const float groupWidth = std::max({w1, w2, w3});
        const float groupLeft = (virtualW - groupWidth) / 2.0f;

        for (int i = 0; i < 3; i++) {
            b.element({
                          .flags = UI::Flag::Floating,
                          .x = static_cast<int16_t>(std::lround(groupLeft)),
                          .y = static_cast<int16_t>(std::lround(blockY[i])),
                      },
                      [&] {
                          b.image({
                              .imagePath = "gfx/menu/block" + std::to_string(i + 1) + ".svg",
                              .scale = blockScale,
                          });
                      });
        }
    }

    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childDirection = UI::ChildDir::UpDown,
                  .childGap = 10,
                  .padding = UI::Padding::all(20),
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.text({
                      .text = Unzip::getLoadingState(),
                      .fontId = 0,
                      .scale = 1.0f,
                      .color = {255, 255, 255, 255},
                      .align = UI::AlignMode::Center,
                  });
              });
}

void LoadingMenu::callback(UI::Builder &b) {
    if (hasBlocks) {
        float delta = deltaTime.getTimeMs() / 1000.0f;
        if (delta <= 0.001f) delta = 1.0f / 60.0f;
        deltaTime.start();

        const float lerpSpeed = std::min(1.0f, 12.0f * delta);
        const float virtualH = static_cast<float>(Render::getHeight()) / b.scale;

        const float targetY1 = virtualH * 0.32f;
        const float targetY2 = targetY1 + 30.0f;
        const float targetY3 = targetY1 + 60.0f;

        if (std::abs(blockY[0] - targetY1) > 2.0f) {
            blockY[0] += (targetY1 - blockY[0]) * lerpSpeed;
        } else if (std::abs(blockY[1] - targetY2) > 2.0f) {
            blockY[1] += (targetY2 - blockY[1]) * lerpSpeed;
        } else if (std::abs(blockY[2] - targetY3) > 2.0f) {
            blockY[2] += (targetY3 - blockY[2]) * lerpSpeed;
        } else {
            blockY.fill(virtualH * 2.0f);
        }
    }

    b.dirty = true;
}