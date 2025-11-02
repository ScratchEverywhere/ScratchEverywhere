#include "menuObjects.hpp"
#include "input.hpp"
#include "render.hpp"

#define REFERENCE_WIDTH 400
#define REFERENCE_HEIGHT 240

#ifdef __OGC__
static float guiScale = 1.3f;
#else
static float guiScale = 1.0f;
#endif

double MenuObject::getScaleFactor() {
#ifdef __3DS__
    guiScale = 1.0f;
    return guiScale;
#else
    double WindowScale = Render::getWidth() + Render::getHeight();

    if (WindowScale > 1600)
        guiScale = 1.5f;
    else if (WindowScale > 1000)
        guiScale = 1.3f;
    else if (WindowScale < 780)
        guiScale = 0.7f;
    else guiScale = 1.0f;

    return guiScale;
#endif
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
    buttonTexture = new MenuImage(filePath);
}

bool ButtonObject::isPressed(std::vector<std::string> pressButton) {
    for (const auto &button : pressButton) {
        if ((isSelected || !needsToBeSelected) && Input::isKeyJustPressed(button)) return true;
    }

    if (!canBeClicked) return false;

    std::vector<int> touchPos = Input::getTouchPosition();

    int touchX = touchPos[0];
    int touchY = touchPos[1];

    // if not touching the screen on 3DS, set touch pos to the last frame one
    if (touchX == 0 && !lastFrameTouchPos.empty()) touchX = lastFrameTouchPos[0];
    if (touchY == 0 && !lastFrameTouchPos.empty()) touchY = lastFrameTouchPos[1];

    // get position based on scale
    std::vector<double> scaledPos = getScaledPosition(x, y);
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
            if (std::abs(lastFrameTouchPos[0] - touchX) < 10 && std::abs(lastFrameTouchPos[1] - touchY) < 10) return true;
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
    std::vector<double> scaledPos = getScaledPosition(x, y);
    double scaledWidth = buttonTexture->image->getWidth() * buttonTexture->scale * getScaleFactor();
    double scaledHeight = buttonTexture->image->getHeight() * buttonTexture->scale * getScaleFactor();

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
    buttonTexture->image->renderNineslice(scaledPos[0], scaledPos[1],
                                          std::max(text->getSize()[0] * renderScale, (float)buttonTexture->image->getWidth() * renderScale),
                                          std::max(text->getSize()[1] * renderScale, (float)buttonTexture->image->getHeight() * renderScale), 8, true);

    text->setScale(renderScale * textScale);
    text->render(scaledPos[0], scaledPos[1]);
}

ButtonObject::~ButtonObject() {
    delete text;
    delete buttonTexture;
}

MenuImage::MenuImage(std::string filePath, int xPos, int yPos) {
    x = xPos;
    y = yPos;
    scale = 1.0;
    image = new Image(filePath);
}

void MenuImage::render(double xPos, double yPos) {
    if (xPos == 0) xPos = x;
    if (yPos == 0) yPos = y;

    image->scale = scale * getScaleFactor();
    const double proportionX = static_cast<double>(xPos) / REFERENCE_WIDTH;
    const double proportionY = static_cast<double>(yPos) / REFERENCE_HEIGHT;

    renderX = proportionX * Render::getWidth();
    renderY = proportionY * Render::getHeight();

    if (width <= 0 && height <= 0) {
        image->renderNineslice(renderX, renderY, image->getWidth() * scale, image->getHeight() * scale, 8 /* TODO: make this customizable */, true);
        return;
    }
    image->renderNineslice(renderX, renderY, width * scale, height * scale, 8 /* TODO: make this customizable */, true);
}

MenuImage::~MenuImage() {
    delete image;
}

ControlObject::ControlObject() {
}

void ControlObject::input() {
    if (selectedObject == nullptr) return;

    ButtonObject *newSelection = nullptr;

    if (Input::isKeyJustPressed("up arrow") || Input::isKeyJustPressed("u")) newSelection = selectedObject->buttonUp;
    else if (Input::isKeyJustPressed("down arrow") || Input::isKeyJustPressed("h")) newSelection = selectedObject->buttonDown;
    else if (Input::isKeyJustPressed("left arrow") || Input::isKeyJustPressed("g")) newSelection = selectedObject->buttonLeft;
    else if (Input::isKeyJustPressed("right arrow") || Input::isKeyJustPressed("j")) newSelection = selectedObject->buttonRight;

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

void ControlObject::render(double xPos, double yPos) {
    if (selectedObject != nullptr) {
        // Get the button's scaled position (center point)
        std::vector<double> buttonCenter = getScaledPosition(selectedObject->x + xPos, selectedObject->y - yPos);

        // Calculate the scaled dimensions of the button
        float renderScale = selectedObject->scale * getScaleFactor();

        double scaledWidth = std::max(selectedObject->text->getSize()[0] * renderScale, (float)selectedObject->buttonTexture->image->getWidth() * renderScale);
        double scaledHeight = std::max(selectedObject->text->getSize()[1] * renderScale, (float)selectedObject->buttonTexture->image->getHeight() * renderScale);

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
}

ControlObject::~ControlObject() {
    selectedObject = nullptr;
    buttonObjects.clear();
}
