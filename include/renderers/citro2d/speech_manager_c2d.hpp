#pragma once
#include <se_export.hpp>

#include <speech_manager.hpp>

class SE_EXPORT SpeechManagerC2D : public SpeechManager {
  protected:
    SpeechTextConfig getSpeechTextConfig() override {
        // 3DS speech text uses a bolder font, rasterised at the font atlas'
        // default 30px nominal size then scaled down to 16px, and a
        // narrower wrap width to suit its small screens.
        return {"gfx/menu/Ubuntu-Bold", 16.0f / 30.0f, 100};
    }

    SpeechRenderConfig getSpeechRenderConfig() override {
        SpeechRenderConfig config;
        config.style = SpeechBubbleStyle::Fast;
        config.rescaleTextEachFrame = false;
        config.topGap = 30;
        config.clampTextYToTextHeight = false;
        return config;
    }

    void getScreenSize(int &outWidth, int &outHeight) override;

  public:
    SpeechManagerC2D();
    ~SpeechManagerC2D() override;
};
