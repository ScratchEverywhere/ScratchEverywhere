#pragma once
#include <image.hpp>
#include <os.hpp>
#include <text.hpp>

class Loading {
  private:
    std::shared_ptr<Image> block1 = nullptr;
    std::shared_ptr<Image> block2 = nullptr;
    std::shared_ptr<Image> block3 = nullptr;
    std::unique_ptr<TextObject> loadingStateText;
    Timer deltaTime;
    float block1Y;
    float block2Y;
    float block3Y;

  public:
    void init();
    void render();
    void cleanup();
};
