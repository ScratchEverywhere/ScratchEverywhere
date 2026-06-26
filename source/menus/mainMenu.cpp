#include "mainMenu.hpp"
#include "audiostack.hpp"
#include "components.hpp"
#include "image.hpp"
#include "log.hpp"
#include "menuManager.hpp"
#include "render.hpp"
#include "translation.hpp"

std::string MainMenu::splash = "";

MainMenu::MainMenu(void *userdata) {
#ifdef __NDS__
    constexpr const char *songName = "gfx/nds/mm_ds.wav";
#else
    constexpr const char *songName = "gfx/menu/mm_splash.ogg";
#endif
    if (!Mixer::isSoundPlaying(songName)) {
        SoundStream *strm = new SoundStream(songName);
        if (strm->error.has_value()) {
            Log::logError(strm->error.value());
            delete strm;
        } else {
            Mixer::setAutoClean(songName, true);

            const auto &settings = SettingsManager::getConfigSettings();
            Mixer::setSoundVolume(songName, settings.value("musicVolume", 100));
        }
    }

    const auto maybe = createImageFromFile("gfx/menu/logo.svg", false);
    if (!maybe.has_value()) {
        Log::logError("Failed to load logo image: " + maybe.error());
    }
    logo = maybe.value();

    if (splash == "") splash = TranslationManager::getSplashText();
}

void MainMenu::onResize() {
    logo->resizeSVG(Render::getWidth() / 550.0f);
}

void MainMenu::render() {
    // clang-format off
	CLAY(CLAY_ID("main"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = {115, 75, 115, 255},
		.cornerRadius = {15 * menuManager->scale, 0, 15 * menuManager->scale, 0}
	}) {
		if (logo) {
			CLAY(CLAY_ID("logo"), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_PERCENT(0.5) },
					.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_BOTTOM }
				},
				.aspectRatio = { logo->getWidth() / static_cast<float>(logo->getHeight()) },
				.image = { logo.get() }
			});
			const Clay_String claySplash = {true, static_cast<int32_t>(splash.length()), splash.c_str()};
			constexpr uint16_t minFontSize = 18;
			uint16_t fontSize = Clay_GetElementData(CLAY_ID("logo")).boundingBox.width / 12.5;
			if (fontSize < minFontSize) fontSize = minFontSize;
			CLAY_TEXT(claySplash, CLAY_TEXT_CONFIG({.textColor = {255, 150, 35, 255}, .fontId = components::FONT_ID_BODY_BOLD_48, .fontSize = fontSize }));
		}
	}
    // clang-format on
}
