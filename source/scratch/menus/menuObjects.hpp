#pragma once
#include "../image.hpp"
#include "../text.hpp"

class MenuObject {
  public:
    double x;
    double y;
    double scale;
    virtual void render() = 0;
    double getScaleFactor(int windowX, int windowY);
    std::vector<double> getScaledPosition(double xPos, double yPos);
};

class MenuImage : public MenuObject {
  public:
    Image *image;
    void render() override;

    /*
     * Similar to Image class, but with auto scaling and positioning.
     * @param filePath
     * @param xPosition
     * @param yPosition
     */
    MenuImage(std::string filePath, int xPos = 0, int yPos = 0);
    virtual ~MenuImage();
};

class ButtonObject : public MenuObject {
  private:
    TextObject *text;
    bool pressedLastFrame = false;
    std::vector<int> lastFrameTouchPos;

  public:
    bool isSelected = false;
    MenuImage *buttonTexture;
    ButtonObject *buttonUp = nullptr;
    ButtonObject *buttonDown = nullptr;
    ButtonObject *buttonLeft = nullptr;
    ButtonObject *buttonRight = nullptr;

    void render() override;
    bool isPressed(std::string pressButton = "a");
    bool isTouchingMouse();

    /*
     * Simple button object.
     * @param buttonText
     * @param width
     * @param height
     * @param xPosition
     * @param yPosition
     */
    ButtonObject(std::string buttonText, std::string filePath, int xPos = 0, int yPos = 0);
    virtual ~ButtonObject();
};

class ControlObject : public MenuObject {
  public:
    std::vector<ButtonObject *> buttonObjects;
    ButtonObject *selectedObject = nullptr;
    void input();
    void render() override;
    ControlObject();
    virtual ~ControlObject();
};