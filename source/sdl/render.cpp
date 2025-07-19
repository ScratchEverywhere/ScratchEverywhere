#include "../scratch/render.hpp"
#include "render.hpp"
#include "interpret.hpp"
int windowWidth = 480;
int windowHeight = 360;
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;

void Render::Init(){
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    window = SDL_CreateWindow("Scratch Runtime",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,windowWidth,windowHeight,SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);

}
void Render::deInit(){
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}
void Render::renderSprites(){
    SDL_GetWindowSizeInPixels(window,&windowWidth,&windowHeight);
    //SDL_SetWindowSize(window,Scratch::projectWidth,Scratch::projectHeight);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    double scaleX = static_cast<double>(windowWidth) / Scratch::projectWidth;
    double scaleY = static_cast<double>(windowHeight) / Scratch::projectHeight;
    double scale;
    scale = std::min(scaleX, scaleY);

    
    // Sort sprites by layer first
    std::vector<Sprite*> spritesByLayer = sprites;
    std::sort(spritesByLayer.begin(), spritesByLayer.end(), 
    [](const Sprite* a, const Sprite* b) {
        return a->layer < b->layer;
    });


    for(Sprite* currentSprite : spritesByLayer) {
        if(!currentSprite->visible) continue;

        bool legacyDrawing = false;
        auto imgFind = images.find(currentSprite->costumes[currentSprite->currentCostume].id); // long ahh line
        if(imgFind == images.end()){
            legacyDrawing = true;
        }
        if(!legacyDrawing){
            SDL_Image* image = imgFind->second;
            SDL_RendererFlip flip = SDL_FLIP_NONE;


            image->setScale((currentSprite->size * 0.01) * scale / 2.0f);
            currentSprite->spriteWidth = image->renderRect.w;
            currentSprite->spriteHeight = image->renderRect.h;
            image->renderRect.x = currentSprite->xPosition;
            image->renderRect.y = currentSprite->yPosition;
            image->setRotation(Math::degreesToRadians(currentSprite->rotation - 90.0f));

            if(currentSprite->rotationStyle == currentSprite->LEFT_RIGHT){
                if(image->rotation < 0){
                    flip = SDL_FLIP_HORIZONTAL;
                }
                image->setRotation(0);
            }
            if(currentSprite->rotationStyle == currentSprite->NONE){
                image->setRotation(0);
            }
            
            image->renderRect.x = (currentSprite->xPosition * scale) + (windowWidth / 2) - (image->renderRect.w / 2);
            image->renderRect.y = (currentSprite->yPosition * -scale) + (windowHeight / 2) - (image->renderRect.h / 2);
            SDL_Point center = {image->renderRect.w / 2,image->renderRect.h / 2};

            SDL_RenderCopyEx(renderer,image->spriteTexture,&image->textureRect,&image->renderRect,image->rotation,&center,flip);
        }
        else{
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_Rect rect;
            rect.x = (currentSprite->xPosition * scale) + (windowWidth / 2);
            rect.y = (currentSprite->yPosition * -1 * scale) + (windowHeight * 0.5);
            rect.w = 16;
            rect.h = 16;
            SDL_RenderDrawRect(renderer,&rect);
        }

        // Draw collision points (for debugging)
        // std::vector<std::pair<double, double>> collisionPoints = getCollisionPoints(currentSprite);
        // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black points

        // for (const auto& point : collisionPoints) {
        //     double screenX = (point.first * scale) + (windowWidth / 2);
        //     double screenY = (point.second * -scale) + (windowHeight / 2);

        //     SDL_Rect debugPointRect;
        //     debugPointRect.x = static_cast<int>(screenX - scale); // center it a bit
        //     debugPointRect.y = static_cast<int>(screenY - scale);
        //     debugPointRect.w = static_cast<int>(2 * scale);
        //     debugPointRect.h = static_cast<int>(2 * scale);

        //     SDL_RenderFillRect(renderer, &debugPointRect);
        // }

    }


    SDL_RenderPresent(renderer);
}


bool Render::appShouldRun(){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        if(event.type == SDL_QUIT){
            return false;
        }
    }
    return true;
}


// i dont think we need these tbh
void LoadingScreen::init(){

}
void LoadingScreen::renderLoadingScreen(){

}
void LoadingScreen::cleanup(){

}
// or these,,,
void MainMenu::init(){

}
void MainMenu::render(){

}
void MainMenu::cleanup(){

}