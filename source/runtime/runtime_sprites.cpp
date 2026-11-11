#include "collision.hpp"
#include "runtime.hpp"
#include <algorithm>
#include <cmath>
#include <input.hpp>
#include <limits>
#include <log.hpp>
#include <renderer.hpp>
#include <speech_manager.hpp>
#include <utility>

std::pair<float, float> Scratch::screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight) {
    if ((Render::currentRenderer == Renderers::Citro2D || Render::currentRenderer == Renderers::GX2) && Render::renderMode == Render::BOTH_SCREENS) {
        // 3DS/Wii U res with both screens combined
        windowWidth = 400;
        windowHeight = 480;
    }

    float screenAspect = static_cast<float>(windowWidth) / windowHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    float scratchX, scratchY;

    if (screenAspect > projectAspect) {
        // Vertical black bars
        float scale = static_cast<float>(windowHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (windowWidth - scaledProjectWidth) / 2.0f;

        // Remove bar offset and scale to project space
        float adjustedX = screenX - barWidth;
        scratchX = (adjustedX / scaledProjectWidth) * Scratch::projectWidth - (Scratch::projectWidth / 2.0f);
        scratchY = (Scratch::projectHeight / 2.0f) - (screenY / windowHeight) * Scratch::projectHeight;

    } else if (screenAspect < projectAspect) {
        // Horizontal black bars
        float scale = static_cast<float>(windowWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (windowHeight - scaledProjectHeight) / 2.0f;

        // Remove bar offset and scale to project space
        float adjustedY = screenY - barHeight;
        scratchX = (screenX / windowWidth) * Scratch::projectWidth - (Scratch::projectWidth / 2.0f);
        scratchY = (Scratch::projectHeight / 2.0f) - (adjustedY / scaledProjectHeight) * Scratch::projectHeight;

    } else {
        // no black bars..
        float scale = static_cast<float>(windowWidth) / Scratch::projectWidth;
        scratchX = (screenX / scale) - (Scratch::projectWidth / 2.0f);
        scratchY = (Scratch::projectHeight / 2.0f) - (screenY / scale);
        if ((Render::currentRenderer == Renderers::Citro2D || Render::currentRenderer == Renderers::GX2) && Render::renderMode == Render::BOTH_SCREENS) {
            scratchY -= 120;
        }
    }

    return std::make_pair(scratchX, scratchY);
}

void Scratch::cleanupSprites() {
    SpeechManager *speechManager = Render::getSpeechManager();
    for (Sprite *sprite : Scratch::sprites) {
        if (sprite) {
            if (speechManager) {
                speechManager->clearSpeech(sprite);
            }
            delete sprite;
        }
    }
    Scratch::sprites.clear();
}

bool Scratch::isColliding(const std::string &collisionType, Sprite *currentSprite, Sprite *targetSprite, const std::string &targetName) {
    if (collisionType == "mouse") {
        if (accurateCollision) return collision::pointInSprite(currentSprite, Input::mousePointer.x, Input::mousePointer.y);
        else return collision::pointInSpriteFast(currentSprite, Input::mousePointer.x, Input::mousePointer.y);
    } else if (collisionType == "edge") {
        if (accurateCollision) return collision::spriteOnEdge(currentSprite);
        else return collision::spriteOnEdgeFast(currentSprite);
    } else if (collisionType == "sprite") {
        if (targetSprite == nullptr && !targetName.empty()) {
            for (Sprite *sprite : sprites) {
                if (sprite->name == targetName && sprite->visible) {
                    targetSprite = sprite;
                    break;
                }
            }
        }

        if (targetSprite == nullptr || !targetSprite->visible || !currentSprite->visible) {
            return false;
        }

        if (accurateCollision) return collision::spriteInSprite(currentSprite, targetSprite);
        else return collision::spriteInSpriteFast(currentSprite, targetSprite);
    } else {
        Log::logWarning("Invalid collision type " + collisionType);
        return false;
    }
}

void Scratch::gotoXY(Sprite *sprite, double x, double y) {
    if (sprite->isStage) return;

    const double oldX = sprite->xPosition;
    const double oldY = sprite->yPosition;
    if (Scratch::fencing) fenceSpriteWithinBounds(sprite, x, y);
    sprite->xPosition = x;
    sprite->yPosition = y;

    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) {
        if (accuratePen) Render::penMoveAccurate(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);
        else Render::penMoveFast(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);
    }
    if (sprite->visible) Scratch::forceRedraw = true;
}

void Scratch::fenceSpriteWithinBounds(Sprite *sprite, double &x, double &y) {
    if (std::abs(x) < Scratch::projectWidth * 0.3 && std::abs(y) < Scratch::projectHeight * 0.3)
        return;

    if (sprite->spriteWidth == 0 || sprite->spriteHeight == 0) loadCurrentCostumeImage(sprite);

    const collision::AABB bounds = collision::getFenceBounds(sprite);
    constexpr double fenceWidth = 15.0;

    const double width = bounds.right - bounds.left;
    const double height = bounds.top - bounds.bottom;
    const double inset = std::floor(std::min(width, height) * 0.5);

    const double sx = (Scratch::projectWidth * 0.5) - std::min(fenceWidth, inset);
    const double sy = (Scratch::projectHeight * 0.5) - std::min(fenceWidth, inset);

    const double dx = x - sprite->xPosition;
    const double dy = y - sprite->yPosition;

    if (bounds.right + dx < -sx) {
        x = std::ceil(sprite->xPosition - (sx + bounds.right));
    } else if (bounds.left + dx > sx) {
        x = std::floor(sprite->xPosition + (sx - bounds.left));
    }

    if (bounds.top + dy < -sy) {
        y = std::ceil(sprite->yPosition - (sy + bounds.top));
    } else if (bounds.bottom + dy > sy) {
        y = std::floor(sprite->yPosition + (sy - bounds.bottom));
    }
}

void Scratch::setDirection(Sprite *sprite, double direction) {
    if (sprite->isStage) return;
    if (direction == std::numeric_limits<double>::infinity() || direction == -std::numeric_limits<double>::infinity() || std::isnan(direction)) {
        return;
    }

    sprite->rotation = direction - floor((direction + 179) / 360) * 360;
    if (sprite->visible) Scratch::forceRedraw = true;
}

void Scratch::sortSprites() {
    std::sort(sprites.begin(), sprites.end(),
              [](const Sprite *a, const Sprite *b) {
                  if (a->isStage && !b->isStage) return false;
                  if (!a->isStage && b->isStage) return true;
                  return a->layer > b->layer;
              });

    unsigned int currentLayer = sprites.size() - 1;
    for (auto *sprite : sprites)
        sprite->layer = currentLayer--;
}
