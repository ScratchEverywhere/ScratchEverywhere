#pragma once
#include "text_vita.hpp"
#include <vita2d.h>
#include <unordered_map>

class TextObjectVita : public TextObject {
  private:
    static std::unordered_map<std::string, FontClass> fonts;
    static std::unordered_map<std::string, size_t> fontUsageCount;
	std::string fontName;
	std::string text;
	
	typedef struct {
		void* font;
		bool isPVF = false;
		float pvfDefaultSize = 1.0f;
	} FontClass;
	
	FontClass fontClass;

public:
    TextObjectVita(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectVita() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
    static void cleanupText();
};
