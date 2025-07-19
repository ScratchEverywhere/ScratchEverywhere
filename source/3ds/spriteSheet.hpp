#ifndef SPRITE_SHEET_HPP
#define SPRITE_SHEET_HPP
#include <string>
#include <citro2d.h>
#include <vector>
#include <bit>

class SpriteSheetObject{
private:
C2D_SpriteSheet sheet;
std::vector<C2D_Sprite> sprites;
int frame = 0;

public:

int x;
int y;
float rotation;


/**
 * A sprite sheet object. code taken from another project of mine.
 * @param FilePath, xPosition,yPosition
 */
SpriteSheetObject(std::string filePath,double posX,double posY);

~SpriteSheetObject();

int getMaxFrames();

void process(double deltaTime);
void render(float xPosition, float yPosition);
void rotate(float rotationRadians);
int getFrameNumber() const;
bool setFrame(int frameNumber);
void setUVPosition(float xPos, float yPos, unsigned int width, unsigned int height);
void debugUV(int xPos, int yPos, int width, int height);
};

#endif