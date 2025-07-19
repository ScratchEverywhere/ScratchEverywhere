#include "render.hpp"
#include "interpret.hpp"
#include "image.hpp"
#include "text.hpp"
#include "spriteSheet.hpp"
#include "../scratch/unzip.hpp"
#include "../scratch/input.hpp"
#include "../scratch/image.hpp"
#include "../scratch/render.hpp"

#define SCREEN_WIDTH 400
#define BOTTOM_SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240



C3D_RenderTarget* topScreen = nullptr;
C3D_RenderTarget* bottomScreen = nullptr;
u32 clrWhite = C2D_Color32f(1,1,1,1);
u32 clrBlack = C2D_Color32f(0,0,0,1);
u32 clrGreen = C2D_Color32f(0,0,1,1);
u32 clrScratchBlue = C2D_Color32(71, 107, 115,255);
std::chrono::_V2::system_clock::time_point startTime = std::chrono::high_resolution_clock::now();
std::chrono::_V2::system_clock::time_point endTime = std::chrono::high_resolution_clock::now();


Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;


void Render::Init(){
	gfxInitDefault();
	hidScanInput();
    u32 kDown = hidKeysHeld();
	if(kDown & KEY_SELECT) consoleInit(GFX_BOTTOM, NULL);
	osSetSpeedupEnable(true);

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
    topScreen = C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);
    bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);

    romfsInit();
}

bool Render::appShouldRun(){
    return aptMainLoop();
}

void renderImage(C2D_Image *image, Sprite* currentSprite, std::string costumeId,bool bottom = false) {

    if(!currentSprite || currentSprite == nullptr) return;


    bool legacyDrawing = true;
    
    double screenOffset = (bottom && Render::renderMode != Render::BOTTOM_SCREEN_ONLY) ? -SCREEN_HEIGHT : 0;

    

        for(Image::ImageRGBA rgba : Image::imageRGBAS){
            if(rgba.name == costumeId){
                legacyDrawing = false;
                currentSprite->spriteWidth = rgba.width / 2;
                currentSprite->spriteHeight = rgba.height / 2;
                
                if(imageC2Ds.find(costumeId) == imageC2Ds.end() || image->tex == nullptr || image->subtex == nullptr){
                C2D_Image newImage = get_C2D_Image(rgba);
                imageC2Ds[costumeId].image = newImage;

                if(currentSprite->lastCostumeId == "") return;

                if(rgba.height > 254 || rgba.width > 254) costumeId = currentSprite->lastCostumeId;

                //return; // hacky solution to fix crashing, causes flickering, TODO fix that 😁
                }
                imageC2Ds[costumeId].freeTimer = 120;
                break;
            }
            else {
                legacyDrawing = true;
                currentSprite->spriteWidth = 64;
                currentSprite->spriteHeight = 64;

            }

        }

    
        

    

    //double maxLayer = getMaxSpriteLayer();
    double scaleX = static_cast<double>(SCREEN_WIDTH) / Scratch::projectWidth;
    double scaleY = static_cast<double>(SCREEN_HEIGHT) / Scratch::projectHeight;
    double spriteSizeX = currentSprite->size * 0.01;
    double spriteSizeY = currentSprite->size * 0.01;
    double scale;
    double heightMultiplier = 0.5;
    int screenWidth = SCREEN_WIDTH;

    if(Render::renderMode == Render::BOTH_SCREENS){
        scaleY = static_cast<double>(SCREEN_HEIGHT) / (Scratch::projectHeight / 2.0);
        heightMultiplier = 1.0;
    }
    if(bottom){
        screenWidth = BOTTOM_SCREEN_WIDTH;
    }


if (!legacyDrawing) {
    double rotation = Math::degreesToRadians(currentSprite->rotation - 90.0f);

    // check for rotation style
    if(currentSprite->rotationStyle == currentSprite->LEFT_RIGHT){
        if(rotation < 0){
            spriteSizeX *= -1;
        }
        rotation = 0;
    }
    if(currentSprite->rotationStyle == currentSprite->NONE){
        rotation = 0;
    }


   scale = bottom ? 1.0 : std::min(scaleX, scaleY);

   float alpha = 1.0f - (currentSprite->ghostEffect / 100.0f);
   C2D_ImageTint tinty;
   C2D_AlphaImageTint(&tinty,alpha);

    C2D_DrawImageAtRotated(
        imageC2Ds[costumeId].image,
        (currentSprite->xPosition * scale) + (screenWidth / 2) + ((currentSprite->spriteWidth - currentSprite->rotationCenterX) / 2),
        (currentSprite->yPosition * -1 * scale) + (SCREEN_HEIGHT * heightMultiplier) + screenOffset + ((currentSprite->spriteHeight - currentSprite->rotationCenterY) / 2) ,
        1,
        rotation,
        &tinty,
        (spriteSizeX) * scale / 2.0f,
        (spriteSizeY) * scale / 2.0f 
    );
} else {
    scale = bottom ? 1.0 : std::min(scaleX, scaleY);
    C2D_DrawRectSolid(
        (currentSprite->xPosition * scale) + (screenWidth / 2),
        (currentSprite->yPosition * -1 * scale) + (SCREEN_HEIGHT * heightMultiplier) + screenOffset,
        1,
        10 * scale,
        10 * scale, 
        clrBlack
    );
}


// Draw collision points
// auto collisionPoints = getCollisionPoints(currentSprite);
// for (const auto& point : collisionPoints) {
//     double screenOffset = bottom ? -SCREEN_HEIGHT : 0; // Adjust for bottom screen
//     double scale = bottom ? 1.0 : std::min(scaleX, scaleY); // Skip scaling if bottom is true

//     C2D_DrawRectSolid(
//         (point.first * scale) + (screenWidth / 2),
//         (point.second * -1 * scale) + (SCREEN_HEIGHT * heightMultiplier) + screenOffset,
//         1, // Layer depth
//         2 * scale, // Width of the rectangle
//         2 * scale, // Height of the rectangle
//         clrBlack
//     );
// }
    // Draw mouse pointer
    if(Input::mousePointer.isMoving)
    C2D_DrawRectSolid(Input::mousePointer.x + (screenWidth / 2), (Input::mousePointer.y * -1) + (SCREEN_HEIGHT * heightMultiplier) + screenOffset, 1, 5, 5, clrGreen);

    currentSprite->lastCostumeId = costumeId;
}

void Render::renderSprites(){

    
    C3D_FrameBegin(C3D_FRAME_NONBLOCK);
    C2D_TargetClear(topScreen,clrWhite);
    C2D_TargetClear(bottomScreen,clrWhite);

    if(Render::renderMode != Render::BOTTOM_SCREEN_ONLY){
    C2D_SceneBegin(topScreen);

    //int times = 1;
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    // Sort sprites by layer (lowest to highest)
    std::vector<Sprite*> spritesByLayer = sprites;
    std::sort(spritesByLayer.begin(), spritesByLayer.end(), 
        [](const Sprite* a, const Sprite* b) {
            return a->layer < b->layer;
        });

// Now render sprites in order from lowest to highest layer
    for(Sprite* currentSprite : spritesByLayer) {
        if(!currentSprite->visible) continue;
    
    // look through every costume in sprite for correct one
        int costumeIndex = 0;
        for(const auto& costume : currentSprite->costumes) {
            if(costumeIndex == currentSprite->currentCostume) {
                currentSprite->rotationCenterX = costume.rotationCenterX;
                currentSprite->rotationCenterY = costume.rotationCenterY;
                renderImage(&imageC2Ds[costume.id].image, currentSprite, costume.id);
                break;
            }
            costumeIndex++;
        }
    }
}

    if(Render::renderMode == Render::BOTH_SCREENS || Render::renderMode == Render::BOTTOM_SCREEN_ONLY){
    C2D_SceneBegin(bottomScreen);
    // Sort sprites by layer (lowest to highest)
    std::vector<Sprite*> spritesByLayer = sprites;
    std::sort(spritesByLayer.begin(), spritesByLayer.end(), 
        [](const Sprite* a, const Sprite* b) {
            return a->layer < b->layer;
        });

    // Now render sprites in order from lowest to highest layer
    for(Sprite* currentSprite : spritesByLayer) {
        if(!currentSprite->visible) continue;
        
        // look through every costume in sprite for correct one
        int costumeIndex = 0;
        for(const auto& costume : currentSprite->costumes) {
            if(costumeIndex == currentSprite->currentCostume) {
                currentSprite->rotationCenterX = costume.rotationCenterX;
                currentSprite->rotationCenterY = costume.rotationCenterY;
                renderImage(&imageC2Ds[costume.id].image, currentSprite, costume.id,true);
                break;
            }
            costumeIndex++;
        }
    }
        }


    C2D_Flush();
    C3D_FrameEnd(0);
    gspWaitForVBlank();
    Image::FlushImages();
    endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = endTime - startTime;
    //int FPS = 1000.0 / std::round(duration.count());
   //std::cout << "\x1b[8;0HCPU: " <<C3D_GetProcessingTime()*6.0f<<"\nGPU: "<< C3D_GetDrawingTime()*6.0f << "\nCmdBuf: " <<C3D_GetCmdBufUsage()*100.0f << "\nFPS: " << FPS <<  std::endl;
    startTime = std::chrono::high_resolution_clock::now();
    osSetSpeedupEnable(true);
}

void LoadingScreen::renderLoadingScreen(){
    C3D_FrameBegin(C3D_FRAME_NONBLOCK);
    C2D_TargetClear(topScreen,clrScratchBlue);
    C2D_SceneBegin(topScreen);

    // if(text != nullptr){
    // text->render();
    // }
    for(squareObject& square : squares){
        square.y -= square.size * 0.1;
        if(square.x > 400 + square.size) square.x = 0 - square.size;
        if(square.y < 0 - square.size) square.y = 240 + square.size;
        C2D_DrawRectSolid(square.x,square.y,1,square.size,square.size,C2D_Color32(255,255,255,75));
    }

    C3D_FrameEnd(0);
}

void LoadingScreen::init(){
    //text = new TextObject("Loading...",200,120);
    createSquares(20);
}

void LoadingScreen::cleanup(){
    // if(text && text != nullptr)
    // delete text;
    squares.clear();

    C2D_Flush();
    C3D_FrameBegin(C3D_FRAME_NONBLOCK);
    C2D_TargetClear(topScreen,clrBlack);
    C2D_SceneBegin(topScreen);

    C2D_TargetClear(bottomScreen,clrBlack);
    C2D_SceneBegin(bottomScreen);

    C3D_FrameEnd(0);
    gspWaitForVBlank();

}

static std::vector<TextObject*> projectTexts;
static std::chrono::steady_clock::time_point logoStartTime = std::chrono::steady_clock::now();
TextObject* selectedText = nullptr;
TextObject* infoText = nullptr;
TextObject* errorTextInfo = nullptr;
int selectedTextIndex = 0;
SpriteSheetObject* logo;

void MainMenu::init(){

    std::vector<std::string> projectFiles = Unzip::getProjectFiles(".");

    int yPosition = 120;
    for(std::string& file : projectFiles){
        TextObject* text = new TextObject(file,145,yPosition);
        text->setColor(C2D_Color32f(0,0,0,1));
        text->y -= text->getSize()[1] / 2;
        projectTexts.push_back(text);
        yPosition += 50;
    }

    if(projectFiles.size() == 0){
        errorTextInfo = new TextObject("No Scratch projects found!\n Go download a Scratch project and put it\n in the 3ds folder of your SD card!\nPress Start to exit.",BOTTOM_SCREEN_WIDTH/2,SCREEN_HEIGHT/2);
        errorTextInfo->setScale(0.6);
        hasProjects = false;
    }
    else{
        selectedText = projectTexts.front();
        hasProjects = true;
    }

    logo = new SpriteSheetObject("romfs:/gfx/menuElements.t3x",SCREEN_WIDTH/2,SCREEN_HEIGHT/2);
    infoText = new TextObject("Runtime by NateXS",340,225);
    infoText->setScale(0.5);

}
void MainMenu::render(){

    if(hasProjects){

        hidScanInput();
        u32 kJustPressed = hidKeysDown();

    if (kJustPressed & (KEY_DDOWN | KEY_CPAD_DOWN)) {
        if (selectedTextIndex < (int)projectTexts.size() - 1) {
            selectedTextIndex++;
            selectedText = projectTexts[selectedTextIndex];
        }
    }
    if (kJustPressed & (KEY_DUP | KEY_CPAD_UP)) {
        if (selectedTextIndex > 0) {
            selectedTextIndex--;
            selectedText = projectTexts[selectedTextIndex];
        }
    }
    cameraY = selectedText->y;

    if (kJustPressed & KEY_A){
        Unzip::filePath = selectedText->getText();
    }
}
else{
    hidScanInput();
    u32 kJustPressed = hidKeysDown();
    if (kJustPressed & (KEY_DDOWN | KEY_START)){
        shouldExit = true;
    }
}


    C3D_FrameBegin(C3D_FRAME_NONBLOCK);
    C2D_TargetClear(topScreen,clrScratchBlue);
    C2D_SceneBegin(topScreen);

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed = now - logoStartTime;

    float timeSeconds = elapsed.count();
    float bobbingOffset = std::sin(timeSeconds * 2.0f) * 5.0f; // speed * amplitude

    logo->render(logo->x, (SCREEN_HEIGHT / 2) + bobbingOffset);
    infoText->render(infoText->x,infoText->y);

    C2D_TargetClear(bottomScreen,clrScratchBlue);
    C2D_SceneBegin(bottomScreen);

    for(TextObject* text : projectTexts){
        if(text == nullptr || text->y > 300) continue;

        if(selectedText == text)
        text->setColor(C2D_Color32(237, 223, 214,255));
        else
        text->setColor(C2D_Color32(0,0,0,255));

        text->render(text->x + cameraX,text->y - (cameraY - (SCREEN_HEIGHT / 2)));
    }

    if(errorTextInfo != nullptr){
        errorTextInfo->render(errorTextInfo->x,errorTextInfo->y);
    }

    //C2D_Flush();
    C3D_FrameEnd(0);
    gspWaitForVBlank();

}
void MainMenu::cleanup(){
    for(TextObject* text : projectTexts){
        delete text;
    }
    projectTexts.clear();

    delete logo;
    delete infoText;
    selectedText = nullptr;
    if(errorTextInfo) delete errorTextInfo;

    C2D_Flush();
    C3D_FrameBegin(C3D_FRAME_NONBLOCK);
    C2D_TargetClear(topScreen, clrScratchBlue);
    C2D_SceneBegin(topScreen);

    C2D_TargetClear(bottomScreen,clrScratchBlue);
    C2D_SceneBegin(bottomScreen);

    C3D_FrameEnd(0);
    gspWaitForVBlank();
}



void Render::deInit(){
    C2D_Fini();
    C3D_Fini();
    for(auto &[id,data] : imageC2Ds){
        if(data.image.tex){
        C3D_TexDelete(data.image.tex);
        free(data.image.tex);
        }
    
        if(data.image.subtex){
            free((Tex3DS_SubTexture*)data.image.subtex);
        }
    }
    Image::imageRGBAS.clear();

    romfsExit();
	gfxExit();

}
