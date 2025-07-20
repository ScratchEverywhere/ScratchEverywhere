#ifndef TEXT_HPP
#define TEXT_HPP
#include <citro2d.h>
#include <string>
#include <vector>

class Object {
  public:
    double x, y, globalX, globalY, layer;
    float scale = 1.0f;
    float rotation = 0.0;
};

class TextObject : public Object {
  private:
    u32 color = C2D_Color32(255, 255, 255, 255);
    C2D_TextBuf StaticTextBuffer;
    C2D_Font font;

    typedef struct {

        C2D_Text StaticText;
        float scale = 1;
        std::string text;
        bool centerAligned = true;

    } TextClass;

    void textParse(std::string txt, TextClass *staticText, float x = 0, float y = 0, float scale = 1);

  public:
    TextClass Text;

    /**
     * Set the color of the text.
     * @param color
     */
    void setColor(int clr);
    /**
     * Change the content of the text.
     * @param txt
     */
    void setText(std::string txt);
    /**
     * @return String of the currently displayed text.
     */
    std::string getText();

    void setScale(float scl);
    /**
     * Render the Text to the screen.
     * @param xPos
     * @param yPos
     */
    void render(int xPos, int yPos);

    /**
     * Gets the size of the text in pixels.
     */
    std::vector<float> getSize();

    /**
     * A basic Text Object.
     * @param text String of text to be displayed.
     * @param positionX
     * @param positionY
     * @param fontPath Path of a .bcfnt file. If unspecified, it uses the default 3DS font.
     */
    TextObject(std::string txt, double posX, double posY, std::string fontPath = "");

    ~TextObject();
};

#endif