#pragma once
#include <string>
#include <vector>

class Object {
  public:
    double x, y, globalX, globalY, layer;
    float scale = 1.0f;
    float rotation = 0.0;
};

class TextObject : public Object {
  protected:
    int color = 0xFFFFFFFF;
    std::string text;
    bool centerAligned = true;

  public:
    /**
     * A basic Text Object.
     * @param text String of text to be displayed.
     * @param positionX
     * @param positionY
     * @param fontPath Path of a font file (.ttf for SDL, .bcfnt for 3DS)
     */
    TextObject(std::string txt, double posX, double posY, std::string fontPath = "");
    virtual ~TextObject() = default;

    /**
     * Cleans up and frees every text currently in memory, as well as frees every font in memory.
     */
    static void cleanupText();

    /**
     * Set the color of the text.
     * @param clr Color value
     */
    virtual void setColor(int clr) { color = clr; }

    /**
     * Change the content of the text.
     * @param txt New text content
     */
    virtual void setText(std::string txt) = 0;

    /**
     * @return String of the currently displayed text.
     */
    virtual std::string getText() const { return text; }

    /**
     * Set the scale of the text
     * @param scl Scale factor
     */
    virtual void setScale(float scl) { scale = scl; }

    /**
     * Get the scale of the text
     * @return Current scale factor
     */
    virtual float getScale() const { return scale; }

    /**
     * Set text alignment
     * @param centered True for center alignment, false for left alignment
     */
    virtual void setCenterAligned(bool centered) { centerAligned = centered; }

    /**
     * Render the Text to the screen.
     * @param xPos X position to render at
     * @param yPos Y position to render at
     */
    virtual void render(int xPos, int yPos) = 0;

    /**
     * Gets the size of the text in pixels.
     * @return Vector containing width and height
     */
    virtual std::vector<float> getSize() = 0;

    virtual void setRenderer(void *renderer) {}
};

TextObject *createTextObject(std::string txt, double posX, double posY, std::string fontPath = "");