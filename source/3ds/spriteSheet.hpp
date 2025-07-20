#ifndef SPRITE_SHEET_HPP
#define SPRITE_SHEET_HPP
#include <bit>
#include <citro2d.h>
#include <string>
#include <vector>

class SpriteSheetObject {
  private:
    C2D_SpriteSheet sheet;
    std::vector<C2D_Sprite> sprites;
    int frame = 0;

  public:
    int x;
    int y;
    float rotation;

    /**
     * A 3DS Sprite Sheet object using .t3x textures.
     * @param FilePath Path of the compiled .t3x texture. Usually in 'romfs:/gfx/'
     * @param xPosition
     * @param yPosition
     */
    SpriteSheetObject(std::string filePath, double posX, double posY);

    ~SpriteSheetObject();

    /**
     * @return The number of frames (images) the Spritesheet contains
     */
    int getMaxFrames();
    /**
     * Renders the Sprite Sheet object to the screen.
     * @param xPosition
     * @param yPosition
     */
    void render(float xPosition, float yPosition);
    /**
     * Adds rotation to the sprite.
     * @param rotation In radians
     */
    void rotate(float rotationRadians);
    /**
     * @return The current Frame (image) number of the Sprite Sheet.
     */
    int getFrameNumber() const;
    /**
     * Sets the Sprite Sheet to a new frame (image) number.
     * @param frameNumber
     */
    bool setFrame(int frameNumber);
};

#endif