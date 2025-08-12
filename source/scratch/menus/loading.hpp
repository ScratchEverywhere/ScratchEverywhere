#pragma once
#include "../os.hpp"
#include "menuObjects.hpp"

class Loading {
  private:
    Image *block1 = nullptr;
    Image *block2 = nullptr;
    Image *block3 = nullptr;
    TextObject *loadingStateText;
    Timer deltaTime;
    float block1Y;
    float block2Y;
    float block3Y;

  public:
    void init();
    void render();
    void cleanup();
};