#pragma once
#include "miniz/miniz.h"
#include <string>
#include <vector>

class Image {
  public:
    struct ImageRGBA {
        std::string name;     // "image"
        std::string fullName; // "image.png"
        int width;
        int height;
        bool isSVG = false;

        //  same as width/height but as powers of 2 for 3DS
        int textureWidth;
        int textureHeight;

        size_t textureMemSize;
        unsigned char *data;
    };

    /**
     * `3DS`: Takes every Image in a Scratch sb3 file and converts them to RGBA data.
     * `SDL`: Takes every image in a Scratch sb3 file and turns it into an 'SDL_Image' object.
     */
    static void loadImages(mz_zip_archive *zip);

    /**
     * `3DS`: Turns a single image from an unzipped Scratch project into RGBA data.
     * `SDL`: Loads a single `SDL_Image` from an unzipped filepath.
     */
    static void loadImageFromFile(std::string filePath);

    /**
     * `3DS`: Frees a `C2D_Image` from memory.
     * `SDL`: Frees an `SDL_Image` from memory.
     */
    static void freeImage(const std::string &costumeId);

    /**
     * `3DS`: Queues a `C2D_Image` to be freed using `costumeId` to find it.
     *        The image will be freed once `FlushImages()` is called.
     * `SDL`: Currently does nothing 😁😁
     */
    static void queueFreeImage(const std::string &costumeId);

    /**
     * Checks every Image in memory to see if they can be freed.
     */
    static void FlushImages();

    static std::vector<ImageRGBA> imageRGBAS;
};