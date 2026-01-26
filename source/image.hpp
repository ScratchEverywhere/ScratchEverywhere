#pragma once
#include <memory.h>
#include <miniz.h>
#include <sprite.hpp>
#include <string>

struct ImageRenderParams {
    int x;
    int y;
    float scale;
    bool centered;
    float opacity;
    int brightness;
    float rotation;
    bool flip;
};

class Image {
  protected:
    int width, height;
    void *pixels;
    std::string id;

  public:
    Image(std::string filePath, bool fromScratchProject = true);
    Image(std::string filePath, mz_zip_archive *zip);
    virtual ~Image();

    int getWidth();
    int getHeight();
    std::string getID();

    virtual void render(ImageRenderParams &params) = 0;
    virtual void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) = 0;
};

std::shared_ptr<Image> createImageFromFile(std::string filePath, bool fromScratchProject = true);
std::shared_ptr<Image> createImageFromZip(std::string filePath, mz_zip_archive *zip);