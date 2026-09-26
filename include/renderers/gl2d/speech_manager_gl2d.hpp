#pragma once
#include <se_export.hpp>

#include "speech_manager.hpp"

class SE_EXPORT SpeechManagerGL2D : public SpeechManager {
  protected:
    SpeechTextConfig getSpeechTextConfig() override {
        // GL2D speech text is rendered with a bolder font at a smaller,
        // fixed nominal size (14px, baked as a 16px atlas scaled down to
        // match the font atlas' default nominal size) and a narrower wrap
        // width to suit the NDS' small screens.
        return {"gfx/menu/Ubuntu-Bold", 14.0f / 16.0f, 100};
    }

    SpeechRenderConfig getSpeechRenderConfig() override {
        SpeechRenderConfig config;
        config.style = SpeechBubbleStyle::Fast;
        config.rescaleTextEachFrame = false;
        config.topGap = 30;
        config.indicatorSize = 20;
        config.indicatorYInsetDivisor = 4;
        config.clampTextYToTextHeight = false;
        return config;
    }

    // Image_GL2D::render() can't crop a subrect yet (see its TODO), so the
    // "say"/"think" half of the indicator sprite sheet has to be drawn by
    // hand here instead of through the generic Image path every other
    // backend uses.
    void renderIndicator(Sprite *sprite, int indicatorX, int indicatorY, int indicatorSize, bool flip) override;

  public:
    SpeechManagerGL2D();
    ~SpeechManagerGL2D() override;
};
