#pragma once
#include <image.hpp>
#include <os.hpp>
#include <render.hpp>
#include <text.hpp>

class MenuObject {
  public:
    double x = 0;
    double y = 0;
    double scale;
    bool hidden = false;
    virtual void render(double xPos = 0, double yPos = 0) = 0;
    static double getScaleFactor();
    std::vector<double> getScaledPosition(double xPos, double yPos);
};

class JollySnow {
  private:
    typedef struct {
        float x, y;
        float fallSpeed;
    } SnowFall;
    std::vector<SnowFall> snow;

    int oldWindowWidth, oldWindowHeight;

  public:
    std::shared_ptr<Image> image;
    JollySnow() {
#ifndef RENDERER_HEADLESS
        oldWindowWidth = Render::getWidth();
        oldWindowHeight = Render::getHeight();

        for (size_t i = 0; i < 30; i++) {
            SnowFall ball = {
                .x = (float)(rand() % Render::getWidth()),
                .y = (float)(rand() % Render::getHeight()),
                .fallSpeed = ((float)rand() / RAND_MAX) * 1.1f + 0.2f};
            snow.push_back(std::move(ball));
        }
#endif
    }

    void render(float xOffset = 0.0f, float yOffset = 0.0f) {
#ifndef RENDERER_HEADLESS
        for (auto &ball : snow) {

            ImageRenderParams params;
            params.x = ball.x + xOffset;
            params.y = ball.y + yOffset;
            params.centered = true;

            image->render(params);
            ball.y += ball.fallSpeed;
            if (ball.y > Render::getHeight() + 20 - yOffset) {
                ball.y = -20 - yOffset;
                ball.x = (float)(rand() % Render::getWidth());
            }
        }

        if (oldWindowWidth != Render::getWidth() || oldWindowHeight != Render::getHeight()) {
            oldWindowWidth = Render::getWidth();
            oldWindowHeight = Render::getHeight();
            for (auto &ball : snow) {
                ball.x = (float)(rand() % Render::getWidth());
                ball.y = (float)(rand() % Render::getHeight());
            }
        }
#endif
    }
};

class MenuImage : public MenuObject {
  public:
    std::shared_ptr<Image> image;
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
    std::unique_ptr<TextObject> text;
    std::string imageId;
    double textScale;
    bool isSelected = false;
    bool needsToBeSelected = true;
    bool canBeClicked = true;
    MenuImage *buttonTexture;
    ButtonObject *buttonUp = nullptr;
    ButtonObject *buttonDown = nullptr;
    ButtonObject *buttonLeft = nullptr;
    ButtonObject *buttonRight = nullptr;
    int renderOffsetX = 0;
    int renderOffsetY = 0;

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
    int cameraX = 0;
    int cameraY = 0;
    int minY, maxY;
    std::vector<int> lastFrameTouchPos;

  public:
    std::vector<ButtonObject *> buttonObjects;
    ButtonObject *selectedObject = nullptr;
    bool enableScrolling = false;
    void input();
    void render(double xPos = 0, double yPos = 0) override;
    ButtonObject *getClosestObject();
    void setScrollLimits();
    ControlObject();
    virtual ~ControlObject();
};
