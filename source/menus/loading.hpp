#pragma once
#include <se_export.hpp>
#include <image.hpp>
#include <os.hpp>
#include <text.hpp>

class SE_EXPORT Loading {
  private:
    std::shared_ptr<Image> block1 = nullptr;
    std::shared_ptr<Image> block2 = nullptr;
    std::shared_ptr<Image> block3 = nullptr;
    std::unique_ptr<TextObject> loadingStateText;
    Timer deltaTime;
    float block1Y;
    float block2Y;
    float block3Y;
    bool renderBlocks = true;

  public:
    void init();
    void render();
    void cleanup();
};
