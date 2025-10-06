#pragma once
#include "miniz/miniz.h"
#include <string>

class Image {
  private:
    int width;
    int height;

  public:
    std::string imageId;
    double scale;
    double opacity;
    double rotation;
    /**
     * Constructor for an image, good if you want to use images outside of a Scratch project.
     * `3DS`: Loads an image RGBA and C2D_Image from a given filepath.
     * `SDL`: Loads an SDL_Image from a given filepath.
     * @param filepath does NOT need `romfs:/`, it will automatically be added.
     */
    Image(std::string filePath);

    ~Image();

    int getWidth() { return width; }
    int getHeight() { return height; }

    void render(double xPos, double yPos, bool centered = false);

    void renderNineslice(double xPos, double yPos, double width, double height, double padding /* IDK if that's the correct name */, bool centered = false);

    /**
     * `3DS`: Turns a single image from an unzipped Scratch project into RGBA data.
     * `SDL`: Loads a single `SDL_Image` from an unzipped filepath.
     * @param filePath
     * @param fromScratchProject
     */
    static bool loadImageFromFile(std::string filePath, bool fromScratchProject = true);

    /**
     * `3DS`: Nothing yet yippie
     * `SDL`: Loads a single `SDL_Image` from a zip file.
     */
    static void loadImageFromSB3(mz_zip_archive *zip, const std::string &costumeId);

    /**
     * `3DS`: Frees a `C2D_Image` from memory.
     * `SDL`: Frees an `SDL_Image` from memory.
     */
    static void freeImage(const std::string &costumeId);

    static void cleanupImages();

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
};
