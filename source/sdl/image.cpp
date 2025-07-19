#include "../scratch/image.hpp"
#include "image.hpp"
#include "render.hpp"
#include <iostream>

std::vector<Image::ImageRGBA> Image::imageRGBAS;
std::unordered_map<std::string,SDL_Image*> images;

void Image::loadImages(mz_zip_archive *zip){
    std::cout << "Loading images..." << std::endl;
    int file_count = (int)mz_zip_reader_get_num_files(zip);

    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip, i, &file_stat)) continue;

        std::string zipFileName = file_stat.m_filename;

        // Check if file is PNG or JPG
    if (zipFileName.size() >= 4 && 
        (zipFileName.substr(zipFileName.size() - 4) == ".png" || zipFileName.substr(zipFileName.size() - 4) == ".PNG"\
        || zipFileName.substr(zipFileName.size() - 4) == ".jpg" || zipFileName.substr(zipFileName.size() - 4) == ".JPG")) {

            size_t file_size;
            void* file_data = mz_zip_reader_extract_to_heap(zip, i, &file_size, 0);
            if (!file_data) {
                std::cout << "Failed to extract: " << zipFileName << std::endl;
                continue;
            }

            // Use SDL_RWops to load image from memory
            SDL_RWops* rw = SDL_RWFromMem(file_data, file_size);
            if (!rw) {
                std::cout << "Failed to create RWops for: " << zipFileName << std::endl;
                mz_free(file_data);
                continue;
            }

            SDL_Surface* surface = IMG_Load_RW(rw, 0);
            SDL_RWclose(rw);
            mz_free(file_data);

            if (!surface) {
                std::cout << "Failed to load image from memory: " << zipFileName << std::endl;
                continue;
            }

            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (!texture) {
                std::cout << "Failed to create texture: " << zipFileName << std::endl;
                SDL_FreeSurface(surface);
                continue;
            }

            SDL_FreeSurface(surface);

            // Build SDL_Image object
            SDL_Image* image = new SDL_Image();
            image->spriteTexture = texture;
            SDL_QueryTexture(texture, nullptr, nullptr, &image->width, &image->height);
            image->renderRect = {0, 0, image->width, image->height};
            image->textureRect = {0, 0, image->width, image->height};

            // Strip extension from filename for the ID
            std::string imageId = zipFileName.substr(0, zipFileName.find_last_of('.'));
            images[imageId] = image;
        }
    }
}
void Image::loadImageFromFile(std::string filePath){
    SDL_Image* image = new SDL_Image(filePath);
    images[filePath] = image;
}
void Image::freeImage(const std::string& costumeId){
    auto image = images.find(costumeId);
    if(image != images.end()){
        images.erase(image);
    }
}

void Image::FlushImages(){
    std::vector<std::string> toDelete;
    
    for(auto& [id, img] : images){
        if(img->freeTimer <= 0){
            toDelete.push_back(id);
        } else {
            img->freeTimer -= 1;
        }
    }
    
    for(const std::string& id : toDelete){
        Image::freeImage(id);
    }
}

SDL_Image::SDL_Image(){}


SDL_Image::SDL_Image(std::string filePath){
    spriteSurface = IMG_Load(filePath.c_str());
    if (spriteSurface == NULL) {
        std::cout << "Error loading image: " << IMG_GetError();
        return;
    }
    spriteTexture = SDL_CreateTextureFromSurface(renderer, spriteSurface);
    if (spriteTexture == NULL) {
        std::cout << "Error creating texture";
        return;
    }
    SDL_FreeSurface(spriteSurface);

    // get width and height of image
    int texW = 0;
    int texH = 0;
    SDL_QueryTexture(spriteTexture,NULL,NULL,&texW,&texH);
    width = texW;
    height = texH;
    renderRect.w = width;
    renderRect.h = height;
    textureRect.w = width;
    textureRect.h = height;
    textureRect.x = 0;
    textureRect.y = 0;

}

void Image::queueFreeImage(const std::string& costumeId){

}

SDL_Image::~SDL_Image(){
    SDL_DestroyTexture(spriteTexture);
}

void SDL_Image::setScale(float amount){
    scale = amount;
    renderRect.w = width * amount;
    renderRect.h = height * amount;
}

void SDL_Image::setRotation(float rotate){
    rotation = rotate;
}