#pragma once
#include <se_export.hpp>
#include <text.hpp>

class Renderer_Xbox;

class SE_EXPORT TextObjectXbox : public TextObjectBase {
  private:
    Renderer_Xbox *renderer = nullptr;

  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectXbox(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectXbox() override;

    void render(int xPos, int yPos) override;
};
