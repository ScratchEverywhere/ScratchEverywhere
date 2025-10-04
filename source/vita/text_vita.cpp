#include "text_vita.hpp"
#include "os.hpp"

#define FONT_SIZE 16

std::unordered_map<std::string, size_t> TextObjectVita::fontUsageCount;

TextObjectVita::TextObjectVita(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {
    x = posX;
    y = posY;

    if (fontPath == "") {
        fontName = "SYSTEM";
    } else {
        fontName = fontPath + ".ttf";
    }

    if (fonts.find(fontName) == fonts.end()) {
        if (fontName == "SYSTEM") {
            fontClass.font = vita2d_load_default_pvf();
			fontClass.isPVF = true;
			fontClass.pvfDefaultSize = vita2d_pvf_text_height(fontClass.font, 1, "QWERTYUIOPASJKLZXCVBNM");
        } else {
            fontClass.font = vita2d_load_font_file(fontName.c_str());
			fontClass.isPVF = false;
        }
        fonts[fontName] = fontClass;
    }

    // set font pointer and increment usage count
    fontUsageCount[fontName] += 1;
    setText(txt);
}

TextObjectVita::~TextObjectVita() {
    if (!fontName.empty() && fontUsageCount.find(fontName) != fontUsageCount.end()) {
        fontUsageCount[fontName] -= 1;
        if (fontUsageCount[fontName] == 0) {
            if (fontName != "SYSTEM") { // Keep the system font loaded at all times
				if (fontClass.isPVF) vita2d_free_pvf(fonts[fontName].font);
				else vita2d_free_font(fonts[fontName].font);
				fonts.erase(fontName);
				fontUsageCount.erase(fontName);
            }
        }
    }
}

void TextObjectVita::setText(std::string txt) {
	text = txt; // No need to be complex here
}

std::vector<float> TextObjectVita::getSize() {
    int width, height;
	if (fontClass.isPVF) vita2d_pvf_text_dimensions(fontClass.font, scale * FONT_SIZE / fontClass.pvfDefaultSize, text.c_str(), &width, &height);
	else vita2d_font_text_dimensions(fontClass.font, scale * FONT_SIZE, text.c_str(), &width, &height);
    return {width, height};
}

void TextObjectVita::render(int xPos, int yPos) {
	std::vector<float> size = getSize();
    yPos -= size[1] / 2;
	if (centerAligned) xPos -= size[0] / 2;
	unsigned int colorU = *((unsigned int*) &color);
	if (fontClass.isPVF) vita2d_pvf_draw_text(fontClass.font, xPos, yPos, colorU, scale * FONT_SIZE / fontClass.pvfDefaultSize, text.c_str());
    else vita2d_font_draw_text(fontClass.font, xPos, yPos, colorU, scale * FONT_SIZE, text.c_str());
}

void TextObjectVita::cleanupText() {
    for (auto &[fontName, font] : fonts) {
		if (font.isPVF) vita2d_free_pvf(font.font);
		else vita2d_free_font(font.font);
    }
    fonts.clear();
    fontUsageCount.clear();

    Log::log("Cleaned up all text.");
}
