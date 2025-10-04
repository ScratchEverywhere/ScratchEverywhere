#pragma once
#include "image.hpp"
#include "os.hpp"
#include "text.hpp"

class MenuObject {
  public:
    double x;
    double y;
    double scale;
    virtual void render(double xPos = 0, double yPos = 0) = 0;
    static double getScaleFactor();
    std::vector<double> getScaledPosition(double xPos, double yPos);
};

class MenuImage : public MenuObject {
  public:
    Image *image;
    void render(double xPos = 0, double yPos = 0) override;

    // These override scale if they are greater than 0.
    double width = 0;
    double height = 0;

    /*
     * Similar to Image class, but with auto scaling and positioning.
     * @param filePath
     * @param xPosition
     * @param yPosition
     */
    MenuImage(std::string filePath, int xPos = 0, int yPos = 0);
    virtual ~MenuImage();

    double renderX;
    double renderY;
};

class ButtonObject : public MenuObject {
  private:
    bool pressedLastFrame = false;
    std::vector<int> lastFrameTouchPos;

  public:
    TextObject *text;
    double textScale;
    bool isSelected = false;
    bool needsToBeSelected = true;
    bool canBeClicked = true;
    MenuImage *buttonTexture;
    ButtonObject *buttonUp = nullptr;
    ButtonObject *buttonDown = nullptr;
    ButtonObject *buttonLeft = nullptr;
    ButtonObject *buttonRight = nullptr;

    void render(double xPos = 0, double yPos = 0) override;
    bool isPressed(std::vector<std::string> pressButton = {"a", "x"});
    bool isTouchingMouse();

    /*
     * Simple button object.
     * @param buttonText
     * @param width
     * @param height
     * @param xPosition
     * @param yPosition
     */
    ButtonObject(std::string buttonText, std::string filePath, int xPos = 0, int yPos = 0, std::string fontPath = "");
    virtual ~ButtonObject();
};

class ControlObject : public MenuObject {
  private:
    Timer animationTimer;

  public:
    std::vector<ButtonObject *> buttonObjects;
    ButtonObject *selectedObject = nullptr;
    void input();
    void render(double xPos = 0, double yPos = 0) override;
    ControlObject();
    virtual ~ControlObject();
};
