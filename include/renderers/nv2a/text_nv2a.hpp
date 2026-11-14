#pragma once
#include <se_export.hpp>
#include <text.hpp>

class Renderer_NV2A;

class SE_EXPORT TextObjectNV2A : public TextObjectBase {
  private:
    Renderer_NV2A *renderer = nullptr;

  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectNV2A(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectNV2A() override;

    void render(int xPos, int yPos) override;
};
