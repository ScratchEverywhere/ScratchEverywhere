#include "blockExecutor.hpp"
#include "collision.hpp"
#include <algorithm>
#include <input.hpp>
#include <renderer.hpp>
#include <runtime.hpp>
#include <string>
#include <unordered_map>
#include <vector>

// ToDo: That could be optimized, but it works for now and i want to move on to other stuff
void BlockExecutor::executeKeyHats() {
#if defined(__XBOX__) && defined(ENABLE_MENU)
    const bool paused = Scratch::pauseMenu != nullptr;
#else
    constexpr bool paused = false;
#endif
    for (const auto &key : Input::keyHeldDuration) {
        if (std::find(Input::inputKeys.begin(), Input::inputKeys.end(), key.first) == Input::inputKeys.end()) {
            Input::keyHeldDuration[key.first] = 0;
        } else {
            Input::keyHeldDuration[key.first]++;
        }
    }

    for (const auto &key : Input::inputKeys) {
        if (Input::keyHeldDuration.find(key) == Input::keyHeldDuration.end()) Input::keyHeldDuration[key] = 1;

        if (paused || key == "any" || Input::keyHeldDuration[key] != 1) continue;

        Input::codePressedBlockOpcodes.clear();
        std::string addKey = (key.find(' ') == std::string::npos) ? key : key.substr(0, key.find(' '));
        std::transform(addKey.begin(), addKey.end(), addKey.begin(), ::tolower);
        Input::inputBuffer.push_back(addKey);
        if (Input::inputBuffer.size() == 101) Input::inputBuffer.erase(Input::inputBuffer.begin());
    }

    // Menu navigation still needs the held counters above, but must not queue
    // game scripts (including Makey Makey sequences) for the next game frame.
    if (paused) return;

    for (Sprite *currentSprite : Scratch::sprites) {
        if (!currentSprite->hats["event_whenkeypressed"].empty()) {
            for (Block *block : currentSprite->hats["event_whenkeypressed"]) {
                std::string key = Scratch::getFieldValue(*block, "KEY_OPTION");
                if (Input::keyHeldDuration.find(key) != Input::keyHeldDuration.end() && (Input::keyHeldDuration.find(key)->second == 1 || Input::keyHeldDuration.find(key)->second > 15 * (Scratch::FPS / 30.0f))) {
                    BlockExecutor::startThread(currentSprite, block, false);
                }
            }
        }
        BlockExecutor::runAllBlocksByOpcodeInSprite("makeymakey_whenMakeyKeyPressed", currentSprite);
    }
    BlockExecutor::runAllBlocksByOpcode("makeymakey_whenCodePressed");
}

namespace {
std::unordered_map<Block *, bool> whenGreaterThanPrevState;
}

void BlockExecutor::resetWhenGreaterThanHats() {
    whenGreaterThanPrevState.clear();
}

void BlockExecutor::executeWhenGreaterThanHats() {
    static ScriptThread conditionThread;

    for (Sprite *currentSprite : Scratch::sprites) {
        auto hatIt = currentSprite->hats.find("event_whengreaterthan");
        if (hatIt == currentSprite->hats.end()) continue;

        for (Block *block : hatIt->second) {
            std::string menu = Scratch::getFieldValue(*block, "WHENGREATERTHANMENU");
            std::transform(menu.begin(), menu.end(), menu.begin(), ::tolower);

            double current = 0;
            if (menu == "timer") current = BlockExecutor::timer.getTimeMs() / 1000.0;

            double threshold = 0;
            Scratch::getInputValueAs(block, "VALUE", &conditionThread, currentSprite, threshold);
            Scratch::resetInput(block, "VALUE");

            bool &wasTrue = whenGreaterThanPrevState[block];
            const bool nowTrue = current > threshold;
            if (nowTrue && !wasTrue) {
                BlockExecutor::startThread(currentSprite, block, true);
            }
            wasTrue = nowTrue;
        }
    }
}

void BlockExecutor::doSpriteClicking() {
#if defined(__XBOX__) && defined(ENABLE_MENU)
    if (Scratch::pauseMenu != nullptr) {
        Input::mousePointer.heldFrames = 0;
        Input::draggingSprite = nullptr;
        return;
    }
#endif
    if (Input::mousePointer.isPressed) {
        Input::mousePointer.heldFrames++;

        bool blockedByMonitor = false;
#if defined(__PC__) // basically the only platform with mouse input and i don't want to waste perf on other platforms.
        if (Input::draggingSprite == nullptr) {
            std::array<int, 2> touchPos = Input::getTouchPosition();
            blockedByMonitor = Render::isPointOverMonitor(touchPos[0], touchPos[1]);
        }
#endif

        if (!blockedByMonitor) {
            bool hasClicked = false;
            for (auto &sprite : Scratch::sprites) {
                if (!sprite->visible || sprite->ghostEffect == 100.0) continue;

                // click a sprite
                if (sprite->shouldDoSpriteClick) {
                    bool colliding;
                    if (Scratch::accurateCollision) colliding = collision::pointInSprite(sprite, Input::mousePointer.x, Input::mousePointer.y, true);
                    else colliding = collision::pointInSpriteFast(sprite, Input::mousePointer.x, Input::mousePointer.y);
                    if (Input::mousePointer.heldFrames < 2 && colliding) {

                        // run all "when this sprite clicked" blocks in the sprite
                        hasClicked = true;
                        BlockExecutor::runAllBlocksByOpcodeInSprite("event_whenthisspriteclicked", sprite);
                        if (sprite->isStage) BlockExecutor::runAllBlocksByOpcodeInSprite("event_whenstageclicked", sprite);
                    }
                }
                // start dragging a sprite
                if (Input::draggingSprite == nullptr && Input::mousePointer.heldFrames < 2 && sprite->draggable && Scratch::isColliding("mouse", sprite)) {
                    Input::draggingSprite = sprite;
                    dragPositionOffsetX = Input::mousePointer.x - sprite->xPosition;
                    dragPositionOffsetY = Input::mousePointer.y - sprite->yPosition;
                }
                if (hasClicked) break;
            }
        }
    } else {
        Input::mousePointer.heldFrames = 0;
    }

    // move a dragging sprite
    if (Input::draggingSprite == nullptr) return;

    if (Input::mousePointer.heldFrames == 0) {
        Input::draggingSprite = nullptr;
        return;
    }
    Input::draggingSprite->xPosition = Input::mousePointer.x - dragPositionOffsetX;
    Input::draggingSprite->yPosition = Input::mousePointer.y - dragPositionOffsetY;
}
