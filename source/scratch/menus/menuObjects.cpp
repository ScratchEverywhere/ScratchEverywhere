#include "menuObjects.hpp"
#include "../input.hpp"
#include "../render.hpp"

#define REFERENCE_WIDTH 400
#define REFERENCE_HEIGHT 240

double MenuObject::getScaleFactor(int windowX, int windowY) {
    double scaleX = static_cast<double>(Render::getWidth()) / windowX;
    double scaleY = static_cast<double>(Render::getHeight()) / windowY;
    return std::min(scaleX, scaleY);
}

std::vector<double> MenuObject::getScaledPosition(double xPos, double yPos) {
    std::vector<double> pos;
    double proportionX = static_cast<double>(xPos) / REFERENCE_WIDTH;
    double proportionY = static_cast<double>(yPos) / REFERENCE_HEIGHT;
    pos.push_back(proportionX * Render::getWidth());
    pos.push_back(proportionY * Render::getHeight());
    return pos;
}

ButtonObject::ButtonObject(std::string buttonText, std::string filePath, int xPos, int yPos) {
    x = xPos;
    y = yPos;
    scale = 1.0;
    text = createTextObject(buttonText, x, y);
    text->setCenterAligned(true);
    buttonTexture = new MenuImage(filePath);
}

bool ButtonObject::isPressed() {
    if (isSelected && Input::isKeyJustPressed("a")) {
        return true;
    }
    std::vector<int> touchPos = Input::getTouchPosition();

    int touchX = touchPos[0];
    int touchY = touchPos[1];

    // if not touching the screen on 3DS, set touch pos to the last frame one
    if (touchX == 0 && !lastFrameTouchPos.empty()) touchX = lastFrameTouchPos[0];
    if (touchY == 0 && !lastFrameTouchPos.empty()) touchY = lastFrameTouchPos[1];

    // get position based on scale
    std::vector<double> scaledPos = getScaledPosition(x, y);
    double scaledWidth = buttonTexture->image->getWidth() * buttonTexture->scale;
    double scaledHeight = buttonTexture->image->getHeight() * buttonTexture->scale;

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
    std::vector<int> touchPos = Input::getTouchPosition();

    int touchX = touchPos[0];
    int touchY = touchPos[1];

    // get position based on scale
    std::vector<double> scaledPos = getScaledPosition(x, y);
    double scaledWidth = buttonTexture->image->getWidth() * buttonTexture->scale;
    double scaledHeight = buttonTexture->image->getHeight() * buttonTexture->scale;

    // simple box collision
    bool withinX = touchX >= (scaledPos[0] - (scaledWidth / 2)) && touchX <= (scaledPos[0] + (scaledWidth / 2));
    bool withinY = touchY >= (scaledPos[1] - (scaledHeight / 2)) && touchY <= (scaledPos[1] + (scaledHeight / 2));

    if ((withinX && withinY)) {
        return true;
    }

    return false;
}

void ButtonObject::render() {
    double scaleFactor = getScaleFactor(REFERENCE_WIDTH, REFERENCE_HEIGHT);
    std::vector<double> scaledPos = getScaledPosition(x, y);

    buttonTexture->x = x;
    buttonTexture->y = y;
    buttonTexture->scale = scale * scaleFactor;
    buttonTexture->render();

    text->setScale(scale * scaleFactor);
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

void MenuImage::render() {

    image->scale = scale;
    double proportionX = static_cast<double>(x) / REFERENCE_WIDTH;
    double proportionY = static_cast<double>(y) / REFERENCE_HEIGHT;

    double actualX = proportionX * Render::getWidth();
    double actualY = proportionY * Render::getHeight();

    image->render(actualX, actualY, true);
}

MenuImage::~MenuImage() {
    delete image;
}

ControlObject::ControlObject() {
}

void ControlObject::input() {
    if (selectedObject == nullptr) return;

    ButtonObject *newSelection = nullptr;

    if (Input::isKeyJustPressed("up arrow")) newSelection = selectedObject->buttonUp;
    else if (Input::isKeyJustPressed("down arrow")) newSelection = selectedObject->buttonDown;
    else if (Input::isKeyJustPressed("left arrow")) newSelection = selectedObject->buttonLeft;
    else if (Input::isKeyJustPressed("right arrow")) newSelection = selectedObject->buttonRight;

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

void ControlObject::render() {
}

ControlObject::~ControlObject() {
}