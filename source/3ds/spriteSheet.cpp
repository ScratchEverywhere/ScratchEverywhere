#include "spriteSheet.hpp"

SpriteSheetObject::SpriteSheetObject(std::string filePath,double posX,double posY){

    x = posX;
    y=posY;


    sheet = C2D_SpriteSheetLoad(filePath.c_str());
    if(!sheet){
    printf("error: could not load spriteSheet.");
    }
   

    size_t numImages = C2D_SpriteSheetCount(sheet);
    for(size_t i=0;i<numImages;i++){
        C2D_Sprite sprite;
        C2D_SpriteFromSheet(&sprite,sheet,i);
		C2D_SpriteSetCenter(&sprite, 0.0f, 0.0f);
        sprites.push_back(sprite);
    }

}

SpriteSheetObject::~SpriteSheetObject(){
    C2D_SpriteSheetFree(sheet);
}

int SpriteSheetObject::getFrameNumber() const {
    return frame;
}

bool SpriteSheetObject::setFrame(int frameNumber){
    if (frameNumber < 0 || frameNumber >= static_cast<int>(sprites.size())) {
        return false;
    }
    frame = frameNumber;
    return true;
}

int SpriteSheetObject::getMaxFrames(){
    return sprites.size() - 1;
}

void SpriteSheetObject::rotate(float rotationRadians){
    rotation += rotationRadians;
    C2D_SpriteRotate(&sprites[frame],rotation);
}

void SpriteSheetObject::render(float xPosition, float yPosition){
    float totalScale = 1;
    C2D_SpriteSetScale(&sprites[frame], totalScale, totalScale);
    
    // Scale the position offset
    float scaledOffsetX = (sprites[frame].image.subtex->width / 2) * totalScale;
    float scaledOffsetY = (sprites[frame].image.subtex->height / 2) * totalScale;
    
    C2D_SpriteSetPos(&sprites[frame], xPosition - scaledOffsetX, yPosition - scaledOffsetY);
    C2D_DrawSprite(&sprites[frame]);
}

