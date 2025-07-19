#include "text.hpp"
#include <3ds.h>

TextObject::TextObject(std::string txt,double posX,double posY,std::string fontPath) {
    x = posX;
    y = posY;
    StaticTextBuffer = C2D_TextBufNew(128);

    if(fontPath == "")
	font = C2D_FontLoadSystem(CFG_REGION_USA);
    else
    font = C2D_FontLoad(fontPath.c_str());
    textParse(txt,&Text,x,y,1);
}

TextObject::~TextObject(){
    if(StaticTextBuffer){
    C2D_TextBufDelete(StaticTextBuffer);
    StaticTextBuffer = nullptr;
    }
    if(font){
        C2D_FontFree(font);
        font = nullptr;
    }
}


void TextObject::textParse(std::string txt,TextClass* staticText,float x ,float y,float scale){

    // First clear the text buffer
    C2D_TextBufClear(StaticTextBuffer);

    // set and optimize the text
	C2D_TextFontParse(&staticText->StaticText,font,StaticTextBuffer,txt.c_str());
	C2D_TextOptimize(&staticText->StaticText);


	staticText->scale = scale;
    staticText->text = txt;


}

void TextObject::setColor(int clr){
color = clr;
}

void TextObject::setText(std::string txt){
    textParse(txt,&Text,x,y,Text.scale);
}

std::string TextObject::getText(){
    return Text.text;
}

void TextObject::setScale(float scl){
    Text.scale = scl;
}

std::vector<float> TextObject::getSize(){
    float width, height;
    C2D_TextGetDimensions(&Text.StaticText, Text.scale, Text.scale, &width, &height);
    return {width,height};
}

void TextObject::render(int xPos, int yPos){
    float totalScale = ((scale - 1.0) + (Text.scale));
    u32 flags = C2D_WithColor;
    if(Text.centerAligned){
        flags |= C2D_AlignCenter;
    }
    C2D_DrawText(&Text.StaticText, flags, xPos, yPos, 0, totalScale, totalScale, color);
}