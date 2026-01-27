#pragma once
#include <memory.h>
#include <miniz.h>
#include <sprite.hpp>
#include <string>

struct ImageSubrect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct ImageRenderParams {
    int x = 0;
    int y = 0;
    float scale = 1.0f;
    bool centered = true;
    float opacity = 1.0f;
    int brightness = 0;
    float rotation = 0;
    bool flip = false;
    ImageSubrect *subrect = nullptr;
};

class Image {
  private:
    std::vector<char> readFileToBuffer(const std::string &filePath, bool fromScratchProject);
    unsigned char *loadSVGFromMemory(const char *data, size_t size, int &width, int &height);
    unsigned char *loadRasterFromMemory(const unsigned char *data, size_t size, int &width, int &height);

  protected:
    int width, height;
    void *pixels;

  public:
    Image(std::string filePath, bool fromScratchProject = true);
    Image(std::string filePath, mz_zip_archive *zip);
    virtual ~Image();

    int getWidth();
    int getHeight();

    virtual void render(ImageRenderParams &params) = 0;
    virtual void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) = 0;
};

std::shared_ptr<Image> createImageFromFile(std::string filePath, bool fromScratchProject = true);
std::shared_ptr<Image> createImageFromZip(std::string filePath, mz_zip_archive *zip);