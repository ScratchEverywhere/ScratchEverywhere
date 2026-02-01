#include "menuObjects.hpp"
#include <input.hpp>
#include <render.hpp>

#define REFERENCE_WIDTH 400
#define REFERENCE_HEIGHT 240

#ifdef __OGC__
static float guiScale = 1.3f;
#else
static float guiScale = 1.0f;
#endif

// turn off nineslice for low performance platforms
#if defined(__NDS__) || defined(__PSP__) || defined(RENDERER_SDL1) /* sdl1 just doesnt support it currently */
static bool enableNineslice = false;
#else
static bool enableNineslice = true;
#endif

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

ButtonObject::ButtonObject(std::string buttonText, std::string filePath, int xPos, int yPos, std::string fontPath) {
    x = xPos;
    y = yPos;
    scale = 1.0;
    textScale = 1.0;
    text = createTextObject(buttonText, x, y, fontPath);
    text->setCenterAligned(true);
    imageId = filePath;
    buttonTexture = new MenuImage(filePath);
}

bool ButtonObject::isPressed(std::vector<std::string> pressButton) {
    for (const auto &button : pressButton) {
        if ((isSelected || !needsToBeSelected) && Input::keyHeldDuration[button] == 1) return true;
    }

    if (!canBeClicked) return false;

    std::vector<int> touchPos = Input::getTouchPosition();

    int touchX = touchPos[0];
    int touchY = touchPos[1];

    // if not touching the screen on 3DS, set touch pos to the last frame one
    if (touchX == 0 && !lastFrameTouchPos.empty()) touchX = lastFrameTouchPos[0];
    if (touchY == 0 && !lastFrameTouchPos.empty()) touchY = lastFrameTouchPos[1];

    // get position based on scale
    std::vector<double> scaledPos = getScaledPosition(x - renderOffsetX, y - renderOffsetY);
    double scaledWidth = buttonTexture->image->getWidth() * buttonTexture->scale * getScaleFactor();
    double scaledHeight = buttonTexture->image->getHeight() * buttonTexture->scale * getScaleFactor();

    // simple box collision
    bool withinX = touchX >= (scaledPos[0] - (scaledWidth / 2)) && touchX <= (scaledPos[0] + (scaledWidth / 2));
    bool withinY = touchY >= (scaledPos[1] - (scaledHeight / 2)) && touchY <= (scaledPos[1] + (scaledHeight / 2));

    // if colliding and mouse state just changed
    if ((withinX && withinY) && pressedLastFrame != Input::mousePointer.isPressed) {

        pressedLastFrame = Input::mousePointer.isPressed;

        // if just stopped clicking, count as a button press
        if (!pressedLastFrame) {
            if (std::abs(lastFrameTouchPos[0] - touchX) < 8 * getScaleFactor() && std::abs(lastFrameTouchPos[1] - touchY) < 8 * getScaleFactor()) return true;
        } else {
            lastFrameTouchPos = touchPos;
        }
    }

    return false;
}

bool ButtonObject::isTouchingMouse() {
    if (!canBeClicked) return false;
    std::vector<int> touchPos = Input::getTouchPosition();

    int touchX = touchPos[0];
    int touchY = touchPos[1];

    // get position based on scale
    std::vector<double> scaledPos = getScaledPosition(x - renderOffsetX, y - renderOffsetY);
    double scaledWidth = buttonTexture->image->getWidth() * getScaleFactor();
    double scaledHeight = buttonTexture->image->getHeight() * getScaleFactor();

    // simple box collision
    bool withinX = touchX >= (scaledPos[0] - (scaledWidth / 2)) && touchX <= (scaledPos[0] + (scaledWidth / 2));
    bool withinY = touchY >= (scaledPos[1] - (scaledHeight / 2)) && touchY <= (scaledPos[1] + (scaledHeight / 2));

    if ((withinX && withinY)) return true;

    return false;
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

    if (enableNineslice) {
        buttonTexture->image->renderNineslice(scaledPos[0], scaledPos[1],
                                              std::max(text->getSize()[0], (float)buttonTexture->image->getWidth() * renderScale),
                                              std::max(text->getSize()[1], (float)buttonTexture->image->getHeight() * renderScale), 8, true);
    } else {
        buttonTexture->image->render(params);
    }

    text->setScale(renderScale * textScale);
    text->render(scaledPos[0], scaledPos[1]);
}

ButtonObject::~ButtonObject() {
    delete buttonTexture;
}

MenuImage::MenuImage(std::string filePath, int xPos, int yPos) {
    x = xPos;
    y = yPos;
    scale = 1.0;
    try {
        image = createImageFromFile(filePath, false);
    } catch (const std::runtime_error &e) {
        Log::logError("Failed to load Menu Image: " + std::string(e.what()));
    }
}

void MenuImage::render(double xPos, double yPos) {
    if (xPos == 0) xPos = x;
    if (yPos == 0) yPos = y;

    // image->scale = scale * getScaleFactor();
    const double proportionX = static_cast<double>(xPos) / REFERENCE_WIDTH;
    const double proportionY = static_cast<double>(yPos) / REFERENCE_HEIGHT;

    renderX = proportionX * Render::getWidth();
    renderY = proportionY * Render::getHeight();

    if (enableNineslice) {
        if (width <= 0 && height <= 0) {
            image->renderNineslice(renderX, renderY, image->getWidth() * scale, image->getHeight() * scale, 8 /* TODO: make this customizable */, true);
            return;
        }
        image->renderNineslice(renderX, renderY, width * scale, height * scale, 8 /* TODO: make this customizable */, true);
    } else {
        ImageRenderParams params;
        params.x = renderX;
        params.y = renderY;
        params.scale = scale;
        params.centered = true;
        image->render(params);
    }
}

MenuImage::~MenuImage() {
    // delete image;
}

ControlObject::ControlObject() {
}

void ControlObject::input() {
    if (selectedObject == nullptr) return;

    ButtonObject *newSelection = nullptr;

    if (Input::keyHeldDuration["up arrow"] == 1 || Input::keyHeldDuration["u"] == 1) newSelection = selectedObject->buttonUp;
    else if (Input::keyHeldDuration["down arrow"] == 1 || Input::keyHeldDuration["h"] == 1) newSelection = selectedObject->buttonDown;
    else if (Input::keyHeldDuration["left arrow"] == 1 || Input::keyHeldDuration["g"] == 1) newSelection = selectedObject->buttonLeft;
    else if (Input::keyHeldDuration["right arrow"] == 1 || Input::keyHeldDuration["j"] == 1) newSelection = selectedObject->buttonRight;

    if (newSelection != nullptr) {
        selectedObject->isSelected = false;
        selectedObject = newSelection;
        selectedObject->isSelected = true;
    } else {
        for (ButtonObject *button : buttonObjects) {
            if (button->isTouchingMouse()) {
                selectedObject->isSelected = false;
                selectedObject = button;
                selectedObject->isSelected = true;
            }
        }
    }
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
        int height = object->buttonTexture->image->getHeight();

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

    if (enableScrolling) {

        if (Input::mousePointer.isPressed) {
            // Touch scrolling
            std::vector<int> touchPos = Input::getTouchPosition();
            if (!lastFrameTouchPos.empty()) {
                float scrollAmount = lastFrameTouchPos[1] - touchPos[1];
                scrollAmount /= guiScale;
                y += scrollAmount;
                if (std::abs(scrollAmount) > 3)
                    selectedObject = getClosestObject();
            }
            lastFrameTouchPos = touchPos;
        } else {
            lastFrameTouchPos.clear();

            // Controller scrolling
            if (std::abs((y - cameraY) * lerpSpeed) < 1) {
                int height = selectedObject->buttonTexture->image->getHeight();

                if (selectedObject->y > (y + yPos) - (height * 0.15) + REFERENCE_HEIGHT) { // going down
                    y = selectedObject->y - height * 0.25;
                } else if (selectedObject->y < (y + yPos) + (height * 0.15)) { // going up
                    y = (selectedObject->y + height * 0.25) - REFERENCE_HEIGHT;
                }
            }
        }
        if (y > maxY) y = maxY;
        if (y < minY) y = minY;
    }

    cameraX += (x - cameraX) * lerpSpeed;
    cameraY += (y - cameraY) * lerpSpeed;
    xPos += cameraX;
    yPos += cameraY;

    for (ButtonObject *object : buttonObjects) {
        if (object->hidden || object->y + object->buttonTexture->image->getHeight() - yPos < 0 || object->y - object->buttonTexture->image->getHeight() - yPos > REFERENCE_HEIGHT) continue;

        object->renderOffsetX = xPos;
        object->renderOffsetY = yPos;

        if (std::abs((y - cameraY) * lerpSpeed) < 1) {
            object->canBeClicked = true;
        } else {
            object->canBeClicked = false;
        }

        object->render(object->x - xPos, object->y - yPos);
    }

    // Get the button's scaled position (center point)
    std::vector<double> buttonCenter = getScaledPosition(selectedObject->x - xPos, selectedObject->y - yPos);

    // Calculate the scaled dimensions of the button
    float renderScale = selectedObject->scale * getScaleFactor();

    double scaledWidth;
    double scaledHeight;

    if (enableNineslice) {
        scaledWidth = std::max(selectedObject->text->getSize()[0], (float)selectedObject->buttonTexture->image->getWidth() * renderScale);
        scaledHeight = std::max(selectedObject->text->getSize()[1], (float)selectedObject->buttonTexture->image->getHeight() * renderScale);
    } else {
        scaledWidth = (float)selectedObject->buttonTexture->image->getWidth() * renderScale;
        scaledHeight = (float)selectedObject->buttonTexture->image->getHeight() * renderScale;
    }

    // animation effect
    double time = animationTimer.getTimeMs() / 1000.0;
    double breathingOffset = sin(time * 12.0) * 2.0 * getScaleFactor();

    // corner positions
    double leftX = buttonCenter[0] - (scaledWidth / 2) - breathingOffset;
    double rightX = buttonCenter[0] + (scaledWidth / 2) + breathingOffset;
    double topY = buttonCenter[1] - (scaledHeight / 2) - breathingOffset;
    double bottomY = buttonCenter[1] + (scaledHeight / 2) + breathingOffset;

    // Render boxes at all 4 corners
    Render::drawBox(6 * getScaleFactor(), 6 * getScaleFactor(), leftX, topY, 33, 34, 36, 255);     // Top-left
    Render::drawBox(6 * getScaleFactor(), 6 * getScaleFactor(), rightX, topY, 33, 34, 36, 255);    // Top-right
    Render::drawBox(6 * getScaleFactor(), 6 * getScaleFactor(), leftX, bottomY, 33, 34, 36, 255);  // Bottom-left
    Render::drawBox(6 * getScaleFactor(), 6 * getScaleFactor(), rightX, bottomY, 33, 34, 36, 255); // Bottom-right
}

ControlObject::~ControlObject() {
    selectedObject = nullptr;
    buttonObjects.clear();
}
