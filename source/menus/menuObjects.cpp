#include "menuObjects.hpp"
#include "text.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <input.hpp>
#include <limits>
#include <log.hpp>
#include <renderer.hpp>

#define REFERENCE_WIDTH 400
#define REFERENCE_HEIGHT 240

#ifdef __OGC__
static float guiScale = 1.3f;
#else
static float guiScale = 1.0f;
#endif

// turn off nineslice for low performance platforms
#if defined(__NDS__) || defined(__PSP__) || defined(RENDERER_SDL1) /* sdl1 just doesnt support it currently */
constexpr bool enableNineslice = false;
#else
constexpr bool enableNineslice = true;
#endif

namespace {
struct MenuPointerState {
    bool down = false;
    bool justDown = false;
    bool justUp = false;
    bool dragged = false;
    bool moved = false;
    bool valid = false;
    bool touchGesture = false;
    bool tapConsumed = false;
    bool suppressTap = false;
    bool flung = false;
    float velocityY = 0.0f;
    float deltaSeconds = 1.0f / 60.0f;
    int x = 0;
    int y = 0;
    int prevY = 0;
    int startX = 0;
    int startY = 0;
};

MenuPointerState menuPointer;
#if defined(PLATFORM_HAS_TOUCH) && !defined(PLATFORM_HAS_MOUSE)
// no visible cursor on touch devices, show only after keyboard/mouse/gamepad interacts with menu
bool menuTouchMode = true;
#else
bool menuTouchMode = false;
#endif
Timer lastTouchTimer;

constexpr float TOUCH_DRAG_SLOP = 8.0f; // distance (in render pixels) a finger can move before counting as a drag
constexpr uint64_t TOUCH_MOUSE_COOLDOWN_MS = 250; // time to ignore synthetic mouse events some backends emit after a touch

constexpr float TOUCH_FLING_MIN_SPEED = 40.0f;
constexpr float TOUCH_FLING_MAX_SPEED = 1800.0f;
constexpr float TOUCH_FLING_FRICTION = 3.0f;
Timer pointerTimer;
uint64_t lastPointerUpdateMs = 0;

void updateMenuPointerState(bool allowFling) {
    const uint64_t now = pointerTimer.getTimeMs();
    const float elapsed = static_cast<float>(now - lastPointerUpdateMs) / 1000.0f;
    lastPointerUpdateMs = now;
    menuPointer.deltaSeconds = std::clamp(elapsed, 0.001f, 0.05f);

    menuPointer.prevY = menuPointer.y;
    menuPointer.justDown = false;
    menuPointer.justUp = false;
    menuPointer.moved = false;
    menuPointer.tapConsumed = false;

    const bool wasDown = menuPointer.down;

    bool touchDown = false;
    int touchX = menuPointer.x;
    int touchY = menuPointer.y;

    InputDevice *device = Input::getGlobalInputDevice();
    std::vector<InputTouch> touches;
    if (device) touches = device->getTouches();
    if (!touches.empty()) {
        size_t best = 0;
        if (wasDown) {
            long bestDistance = std::numeric_limits<long>::max();
            for (size_t i = 0; i < touches.size(); i++) {
                const long dx = touches[i].x - menuPointer.x;
                const long dy = touches[i].y - menuPointer.y;
                const long distance = dx * dx + dy * dy;
                if (distance < bestDistance) {
                    bestDistance = distance;
                    best = i;
                }
            }
        }
        touchDown = true;
        touchX = touches[best].x;
        touchY = touches[best].y;
    } else {
#if defined(PLATFORM_HAS_TOUCH) && !defined(PLATFORM_HAS_MOUSE)
        if (Input::mousePointer.isPressed) {
            const std::array<int, 2> pos = Input::getTouchPosition();
            if (pos[0] != 0 || pos[1] != 0) {
                touchDown = true;
                touchX = pos[0];
                touchY = pos[1];
            }
        }
#endif
    }

    bool pointerDown = false;
#if defined(PLATFORM_HAS_MOUSE)
    bool hasPointerPosition = false;
    int pointerX = menuPointer.x;
    int pointerY = menuPointer.y;
    if (!touchDown) {
        const std::array<int, 2> pos = Input::getTouchPosition();
        pointerX = pos[0];
        pointerY = pos[1];
        hasPointerPosition = true;
        pointerDown = Input::mousePointer.isPressed;
    }
#endif

    const bool down = touchDown || pointerDown;
#if defined(PLATFORM_HAS_MOUSE)
    const bool releasingTouch = wasDown && menuPointer.touchGesture && !touchDown;
#endif
    int x = menuPointer.x;
    int y = menuPointer.y;
    bool valid = false;
    if (touchDown) {
        x = touchX;
        y = touchY;
        valid = true;
    }
#if defined(PLATFORM_HAS_MOUSE)
    else if (hasPointerPosition && !releasingTouch) {
        x = pointerX;
        y = pointerY;
        valid = true;
    }
#endif

    if (valid) {
        menuPointer.moved = (x != menuPointer.x) || (y != menuPointer.y);
        menuPointer.x = x;
        menuPointer.y = y;
    }
    menuPointer.valid = valid;

    menuPointer.touchGesture = touchDown || (wasDown && menuPointer.touchGesture);
    menuPointer.justDown = down && !wasDown;
    menuPointer.justUp = !down && wasDown;

    if (menuPointer.justDown) {
        menuPointer.startX = menuPointer.x;
        menuPointer.startY = menuPointer.y;
        menuPointer.suppressTap = allowFling && menuPointer.flung && touchDown;
        menuPointer.flung = false;
        menuPointer.velocityY = 0.0f;
        menuPointer.dragged = false;
        menuPointer.touchGesture = touchDown;
    } else if (down) {
        const float slop = TOUCH_DRAG_SLOP * std::max(1.0f, static_cast<float>(MenuObject::getScaleFactor()));
        const int dx = std::abs(menuPointer.x - menuPointer.startX);
        const int dy = std::abs(menuPointer.y - menuPointer.startY);
        if (dx > slop || dy > slop) menuPointer.dragged = true;

        if (allowFling && menuPointer.dragged && menuPointer.moved) {
            const float scale = Render::getHeight() > 0 ? static_cast<float>(REFERENCE_HEIGHT) / Render::getHeight() : 1.0f;
            const float instantVelocity = (menuPointer.prevY - menuPointer.y) * scale / menuPointer.deltaSeconds;
            menuPointer.velocityY += (instantVelocity - menuPointer.velocityY) * 0.4f;
            menuPointer.velocityY = std::clamp(menuPointer.velocityY, -TOUCH_FLING_MAX_SPEED, TOUCH_FLING_MAX_SPEED);
        } else if (!menuPointer.moved || !allowFling) {
            menuPointer.velocityY = 0.0f;
        }
    }
    menuPointer.down = down;

    if (menuPointer.justUp) {
        if (allowFling && menuPointer.touchGesture && menuPointer.dragged && std::abs(menuPointer.velocityY) >= TOUCH_FLING_MIN_SPEED) {
            menuPointer.flung = true;
        } else {
            menuPointer.velocityY = 0.0f;
        }
    }

    if (touchDown) {
        menuTouchMode = true;
        lastTouchTimer.start();
    } else if (menuPointer.justUp && menuPointer.touchGesture) {
        menuTouchMode = true;
        lastTouchTimer.start();
    }

    if (!Input::inputKeys.empty() || !Input::inputButtons.empty()) {
        menuTouchMode = false;
        menuPointer.flung = false;
        menuPointer.velocityY = 0.0f;
    }

#if defined(PLATFORM_HAS_MOUSE)
    if (!touchDown && lastTouchTimer.getTimeMs() > TOUCH_MOUSE_COOLDOWN_MS) {
        if (pointerDown || (hasPointerPosition && menuPointer.moved)) {
            menuTouchMode = false;
        }
    }
#endif
}
} // namespace

double MenuObject::getScaleFactor() {
    double WindowScale = Render::getWidth() + Render::getHeight();

    if (WindowScale > 3500)
        guiScale = 3.6f;
    else if (WindowScale > 3000)
        guiScale = 3.0f;
    else if (WindowScale > 2400)
        guiScale = 2.4f;
    else if (WindowScale > 2000)
        guiScale = 1.9f;
    else if (WindowScale > 1600)
        guiScale = 1.5f;
    else if (WindowScale > 1000)
        guiScale = 1.3f;
    else if (WindowScale < 500)
        guiScale = 0.5f;
    else if (WindowScale < 780)
        guiScale = 0.7f;
    else
        guiScale = 1.0f;

    return guiScale;
}

std::vector<double> MenuObject::getScaledPosition(double xPos, double yPos) {
    std::vector<double> pos;
    double proportionX = static_cast<double>(xPos) / REFERENCE_WIDTH;
    double proportionY = static_cast<double>(yPos) / REFERENCE_HEIGHT;
    pos.push_back(proportionX * Render::getWidth());
    pos.push_back(proportionY * Render::getHeight());
    return pos;
}

ButtonObject::ButtonObject(std::string buttonText, std::string filePath, int xPos, int yPos, std::string fontPath, bool nineslice) {
    x = xPos;
    y = yPos;
    shouldNineslice = nineslice;
    scale = 1.0;
    textScale = 1.0;
    text = createTextObject(buttonText, x, y, fontPath);
    text->setCenterAligned(true);
    imageId = filePath;
    buttonTexture = new MenuImage(filePath, x, y, shouldNineslice);
}

bool ButtonObject::isPressed(std::vector<std::string> pressButton) {
    for (const auto &button : pressButton) {
        if ((isSelected || !needsToBeSelected) && Input::keyHeldDuration[button] == 1) return true;
    }

    if (!canBeClicked) return false;

    if (!menuPointer.justUp || menuPointer.dragged || menuPointer.tapConsumed) return false;
    if (menuPointer.suppressTap && menuPointer.touchGesture) return false;
    if (!containsPoint(menuPointer.x, menuPointer.y)) return false;
    if (!containsPoint(menuPointer.startX, menuPointer.startY)) return false;

    menuPointer.tapConsumed = true;
    return true;
}

bool ButtonObject::containsPoint(int pointX, int pointY) {
    if (!buttonTexture || !buttonTexture->image) return false;

    const std::vector<double> scaledPos = getScaledPosition(x - renderOffsetX, y - renderOffsetY);
    const double renderScale = scale * getScaleFactor();

    double scaledWidth = buttonTexture->image->getWidth() * renderScale;
    double scaledHeight = buttonTexture->image->getHeight() * renderScale;

    if (enableNineslice && shouldNineslice && text) {
        scaledWidth = std::max(text->getSize()[0], static_cast<float>(scaledWidth));
        scaledHeight = std::max(text->getSize()[1], static_cast<float>(scaledHeight));
    }

    const bool withinX = pointX >= (scaledPos[0] - (scaledWidth / 2)) && pointX <= (scaledPos[0] + (scaledWidth / 2));
    const bool withinY = pointY >= (scaledPos[1] - (scaledHeight / 2)) && pointY <= (scaledPos[1] + (scaledHeight / 2));

    return withinX && withinY;
}

bool ButtonObject::isTouchingMouse() {
    if (!canBeClicked || !menuPointer.valid) return false;
    return containsPoint(menuPointer.x, menuPointer.y);
}

void ButtonObject::render(double xPos, double yPos) {
    if (xPos == 0) xPos = x;
    if (yPos == 0) yPos = y;

    std::vector<double> scaledPos = getScaledPosition(xPos, yPos);
    float renderScale = scale * getScaleFactor();

    buttonTexture->x = xPos;
    buttonTexture->y = yPos;
    buttonTexture->scale = scale * getScaleFactor();
    ImageRenderParams params;
    params.x = scaledPos[0];
    params.y = scaledPos[1];
    params.scale = scale * getScaleFactor();
    params.centered = true;

#if defined(__XBOX__)
    if (!buttonTexture || !buttonTexture->image) return;
#endif

    text->setScale(renderScale * textScale);

    if (enableNineslice && this->shouldNineslice) {
        buttonTexture->image->renderNineslice(scaledPos[0], scaledPos[1],
                                              std::max(text->getSize()[0], (float)buttonTexture->image->getWidth() * renderScale),
                                              std::max(text->getSize()[1], (float)buttonTexture->image->getHeight() * renderScale), 8, true);
    } else {
        buttonTexture->image->render(params);
    }

    text->render(scaledPos[0], scaledPos[1]);
}

ButtonObject::~ButtonObject() {
    delete buttonTexture;
}

MenuImage::MenuImage(std::string filePath, int xPos, int yPos, bool nineslice) {
    x = xPos;
    y = yPos;
    shouldNineslice = nineslice;
    scale = 1.0;
    auto potentialImage = createImageFromFile(filePath, false);
    if (!potentialImage.has_value()) Log::logError("Failed to load Menu Image: " + potentialImage.error());
    else image = potentialImage.value();
}

void MenuImage::render(double xPos, double yPos) {
#if defined(__XBOX__)
    if (!image) return;
#endif
    if (xPos == 0) xPos = x;
    if (yPos == 0) yPos = y;

    // image->scale = scale * getScaleFactor();
    const double proportionX = static_cast<double>(xPos) / REFERENCE_WIDTH;
    const double proportionY = static_cast<double>(yPos) / REFERENCE_HEIGHT;

    renderX = proportionX * Render::getWidth();
    renderY = proportionY * Render::getHeight();

    const float renderScale = scale * getScaleFactor();

    if (enableNineslice && this->shouldNineslice) {
        if (width <= 0 && height <= 0) {
            image->renderNineslice(renderX, renderY, image->getWidth() * renderScale, image->getHeight() * renderScale, 8 /* TODO: make this customizable */, true);
            return;
        }
        image->renderNineslice(renderX, renderY, width * renderScale, height * renderScale, 8 /* TODO: make this customizable */, true);
    } else {
        ImageRenderParams params;
        params.x = renderX;
        params.y = renderY;
        params.scale = renderScale;
        params.centered = true;
        image->render(params);
    }
}

MenuImage::~MenuImage() {
    // delete image;
}

MenuText::MenuText(std::string txt, double xPos, double yPos) {
    x = xPos;
    y = yPos;
    scale = 1.0;
    text = createTextObject(txt, xPos, yPos);
    text->setCenterAligned(false);
    originalText = txt;
}

void MenuText::setText(const std::string &txt) {
    text->setText(txt);
    originalText = txt;
}

void MenuText::render(double xPos, double yPos) {
    if (xPos == 0) xPos = x;
    if (yPos == 0) yPos = y;

    const double proportionX = static_cast<double>(xPos) / REFERENCE_WIDTH;
    const double proportionY = static_cast<double>(yPos) / REFERENCE_HEIGHT;

    renderX = proportionX * Render::getWidth();
    renderY = proportionY * Render::getHeight();

    const float renderScale = scale * getScaleFactor();
    std::string wrapped = text->wrap(Render::getWidth());
    text->setText(wrapped);

    text->setScale(renderScale);
    text->render(renderX, renderY);
    text->setText(originalText);
}

MenuText::~MenuText() {
}

ControlObject::ControlObject() {
}

void ControlObject::input() {
    updateMenuPointerState(enableScrolling);

    if (selectedObject == nullptr) return;

    ButtonObject *newSelection = nullptr;

    if (Input::keyHeldDuration["up arrow"] == 1 || Input::keyHeldDuration["u"] == 1) newSelection = selectedObject->buttonUp;
    else if (Input::keyHeldDuration["down arrow"] == 1 || Input::keyHeldDuration["h"] == 1) newSelection = selectedObject->buttonDown;
    else if (Input::keyHeldDuration["left arrow"] == 1 || Input::keyHeldDuration["g"] == 1) newSelection = selectedObject->buttonLeft;
    else if (Input::keyHeldDuration["right arrow"] == 1 || Input::keyHeldDuration["j"] == 1) newSelection = selectedObject->buttonRight;
    else if (enableScrolling && Input::keyHeldDuration["r"] == 1) {
        this->y += REFERENCE_HEIGHT;
        setSelected(getClosestObject());
    } else if (enableScrolling && Input::keyHeldDuration["l"] == 1) {
        this->y -= REFERENCE_HEIGHT;
        setSelected(getClosestObject());
    }

    if (menuPointer.justDown && menuPointer.valid) {
        for (ButtonObject *button : buttonObjects) {
            if (button->hidden) continue;
            if (button->containsPoint(menuPointer.x, menuPointer.y)) {
                setSelected(button);
                break;
            }
        }
    }

    if (newSelection != nullptr) {
        setSelected(newSelection);
    } else if (!menuTouchMode && menuPointer.valid && menuPointer.moved) {
        for (ButtonObject *button : buttonObjects) {
            if (button->isTouchingMouse()) {
                setSelected(button);
                break;
            }
        }
    }
}

void ControlObject::setSelected(ButtonObject *object) {
    if (object == nullptr) return;

    if (selectedObject == object) {
        object->isSelected = true;
        return;
    }

    if (selectedObject != nullptr) selectedObject->isSelected = false;
    selectedObject = object;
    selectedObject->isSelected = true;
}

ButtonObject *ControlObject::getClosestObject() {
    ButtonObject *closest = nullptr;
    float closestDist = std::numeric_limits<float>::max();

    for (ButtonObject *object : buttonObjects) {
        if (object->hidden) continue;
        float dx = object->x - this->x;
        float dy = object->y - this->y - (REFERENCE_HEIGHT / 2);
        float dist = dx * dx + dy * dy;

        if (dist < closestDist) {
            closestDist = dist;
            closest = object;
        }
    }

    return closest;
}

void ControlObject::setScrollLimits() {

    minY = std::numeric_limits<int>::max();
    maxY = -std::numeric_limits<int>::max();

    for (ButtonObject *object : buttonObjects) {
#if defined(__XBOX__)
        int height = 0;
        if (object->buttonTexture && object->buttonTexture->image) {
            height = object->buttonTexture->image->getHeight();
        }
#else
        int height = object->buttonTexture->image->getHeight();
#endif

        if (object->y + height - REFERENCE_HEIGHT > maxY) {
            maxY = object->y + height - REFERENCE_HEIGHT;
        }
        if (object->y - height < minY) {
            minY = object->y - height;
        }
    }
}

void ControlObject::render(double xPos, double yPos) {
    if (selectedObject == nullptr) return;
    const float lerpSpeed = 0.1f;

    bool touchDragging = false;
    if (enableScrolling) {
        if (menuPointer.down) {
            if (menuPointer.dragged && menuPointer.valid) {
                float scrollAmount = static_cast<float>(menuPointer.prevY - menuPointer.y);
                if (Render::getHeight() > 0) scrollAmount *= static_cast<float>(REFERENCE_HEIGHT) / Render::getHeight();
                y += scrollAmount;
                touchDragging = true;
                if (std::abs(scrollAmount) > 0.01f) setSelected(getClosestObject());
            }
            menuPointer.flung = false;
        } else if (menuPointer.flung) {
            y += menuPointer.velocityY * menuPointer.deltaSeconds;
            menuPointer.velocityY *= std::exp(-TOUCH_FLING_FRICTION * menuPointer.deltaSeconds);
            if (std::abs(menuPointer.velocityY) < TOUCH_FLING_MIN_SPEED) {
                menuPointer.flung = false;
                menuPointer.velocityY = 0.0f;
            }
            touchDragging = true;
            setSelected(getClosestObject());
        } else {
            // Controller scrolling
            if (std::abs((y - cameraY) * lerpSpeed) < 1) {
#if defined(__XBOX__)
                int height = (selectedObject && selectedObject->buttonTexture && selectedObject->buttonTexture->image) ? selectedObject->buttonTexture->image->getHeight() : 32;
#else
                int height = selectedObject->buttonTexture->image->getHeight();
#endif

                if (selectedObject->y > (y + yPos) - (height * 0.15) + REFERENCE_HEIGHT) { // going down
                    y = selectedObject->y - height * 0.25;
                } else if (selectedObject->y < (y + yPos) + (height * 0.15)) { // going up
                    y = (selectedObject->y + height * 0.25) - REFERENCE_HEIGHT;
                }
            }
        }
        if (maxY >= minY && (y > maxY || y < minY)) {
            y = std::clamp(y, static_cast<double>(minY), static_cast<double>(maxY));
            menuPointer.flung = false;
            menuPointer.velocityY = 0.0f;
        }
    }

    cameraX += (x - cameraX) * lerpSpeed;
    if (touchDragging) {
        cameraY = y;
    } else {
        cameraY += (y - cameraY) * lerpSpeed;
    }
    xPos += cameraX;
    yPos += cameraY;

    for (ButtonObject *object : buttonObjects) {
#if defined(__XBOX__)
        int objHeight = (object && object->buttonTexture && object->buttonTexture->image) ? object->buttonTexture->image->getHeight() : 32;
        if (object->hidden || object->y + objHeight - yPos < 0 || object->y - objHeight - yPos > REFERENCE_HEIGHT) continue;
#else
        if (object->hidden || object->y + object->buttonTexture->image->getHeight() - yPos < 0 || object->y - object->buttonTexture->image->getHeight() - yPos > REFERENCE_HEIGHT) continue;
#endif

        object->renderOffsetX = xPos;
        object->renderOffsetY = yPos;

        object->render(object->x - xPos, object->y - yPos);
    }

    if (!menuTouchMode) {
        std::vector<double> buttonCenter = getScaledPosition(selectedObject->x - xPos, selectedObject->y - yPos);
        float renderScale = selectedObject->scale * getScaleFactor();

        double scaledWidth;
        double scaledHeight;

#if defined(__XBOX__)
        int selImgWidth = (selectedObject && selectedObject->buttonTexture && selectedObject->buttonTexture->image) ? selectedObject->buttonTexture->image->getWidth() : 32;
        int selImgHeight = (selectedObject && selectedObject->buttonTexture && selectedObject->buttonTexture->image) ? selectedObject->buttonTexture->image->getHeight() : 32;

        if (enableNineslice) {
            scaledWidth = std::max(selectedObject->text->getSize()[0], (float)selImgWidth * renderScale);
            scaledHeight = std::max(selectedObject->text->getSize()[1], (float)selImgHeight * renderScale);
        } else {
            scaledWidth = (float)selImgWidth * renderScale;
            scaledHeight = (float)selImgHeight * renderScale;
        }
#else
        if (!selectedObject->buttonTexture || !selectedObject->buttonTexture->image) return;

        if (enableNineslice) {
            scaledWidth = std::max(selectedObject->text->getSize()[0], (float)selectedObject->buttonTexture->image->getWidth() * renderScale);
            scaledHeight = std::max(selectedObject->text->getSize()[1], (float)selectedObject->buttonTexture->image->getHeight() * renderScale);
        } else {
            scaledWidth = (float)selectedObject->buttonTexture->image->getWidth() * renderScale;
            scaledHeight = (float)selectedObject->buttonTexture->image->getHeight() * renderScale;
        }
#endif

        // animation effect
        double time = animationTimer.getTimeMs() / 1000.0;
        double breathingOffset = sin(time * 12.0) * 2.0 * getScaleFactor();

        // corner positions
        double leftX = buttonCenter[0] - (scaledWidth / 2) - breathingOffset;
        double rightX = buttonCenter[0] + (scaledWidth / 2) + breathingOffset;
        double topY = buttonCenter[1] - (scaledHeight / 2) - breathingOffset;
        double bottomY = buttonCenter[1] + (scaledHeight / 2) + breathingOffset;

        // Render boxes at all 4 corners
        Render::drawBox(4 * getScaleFactor(), 4 * getScaleFactor(), leftX, topY, 255, 255, 255, 255);     // Top-left
        Render::drawBox(4 * getScaleFactor(), 4 * getScaleFactor(), rightX, topY, 255, 255, 255, 255);    // Top-right
        Render::drawBox(4 * getScaleFactor(), 4 * getScaleFactor(), leftX, bottomY, 255, 255, 255, 255);  // Bottom-left
        Render::drawBox(4 * getScaleFactor(), 4 * getScaleFactor(), rightX, bottomY, 255, 255, 255, 255); // Bottom-right

        // Corner thingies for the corners
        Render::drawBox(4 * getScaleFactor(), 4 * getScaleFactor(), (leftX + 4 * getScaleFactor()), topY, 255, 255, 255, 255); // |""
        Render::drawBox(4 * getScaleFactor(), 4 * getScaleFactor(), leftX, (topY + 4 * getScaleFactor()), 255, 255, 255, 255);
        Render::drawBox(4 * getScaleFactor(), 4 * getScaleFactor(), (leftX + 4 * getScaleFactor()), bottomY, 255, 255, 255, 255); // |_
        Render::drawBox(4 * getScaleFactor(), 4 * getScaleFactor(), leftX, (bottomY - 4 * getScaleFactor()), 255, 255, 255, 255);
        Render::drawBox(4 * getScaleFactor(), 4 * getScaleFactor(), (rightX - 4 * getScaleFactor()), bottomY, 255, 255, 255, 255); // _|
        Render::drawBox(4 * getScaleFactor(), 4 * getScaleFactor(), rightX, (bottomY - 4 * getScaleFactor()), 255, 255, 255, 255);
        Render::drawBox(4 * getScaleFactor(), 4 * getScaleFactor(), (rightX - 4 * getScaleFactor()), topY, 255, 255, 255, 255); // ""|
        Render::drawBox(4 * getScaleFactor(), 4 * getScaleFactor(), rightX, (topY + 4 * getScaleFactor()), 255, 255, 255, 255);
    }
}

ControlObject::~ControlObject() {
    selectedObject = nullptr;
    buttonObjects.clear();
}