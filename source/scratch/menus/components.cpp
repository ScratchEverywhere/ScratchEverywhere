#include "components.hpp"
#include "../os.hpp"

#ifdef SDL_BUILD
#include <SDL2/SDL_image.h>
#endif

namespace components {
Clay_TextElementConfig *defaultTextConfig;

Sidebar::Sidebar() {
    for (const auto &tab : tabs) {
#ifdef SDL_BUILD
        images[tab] = IMG_Load((OS::getRomFSLocation() + "gfx/menu/" + tab + ".svg").c_str());
        if (!images.contains(tab) || !images[tab]) Log::logError("Failed to load image for tab: " + tab);
#endif
    }
}

Sidebar::~Sidebar() {}

void Sidebar::renderItem(const std::string tab) {
    const std::string id = "sidebar_" + tab;
    Clay_String clayId = (Clay_String){false, static_cast<int32_t>(id.length()), id.c_str()};

    const std::string imageId = "sidebar_img_" + tab;
    Clay_String clayImageId = (Clay_String){false, static_cast<int32_t>(imageId.length()), imageId.c_str()};

    // clang-format off
	CLAY(CLAY_SID(clayId), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(100) },
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
		},
		.cornerRadius = {0, 16, 0, 16},
		.border = { .color = {255, 255, 255, 255}, .width = {0, 4, 4, 4} },
	}) {
		if (images.contains(tab) && images[tab]) CLAY(CLAY_SID(clayImageId), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_PERCENT(0.8) }
				},
				.aspectRatio = { 1 },
				.image = { .imageData = images[tab] }	
				});
	}
    // clang-format on
}

void Sidebar::render() {
    // clang-format off
	CLAY(CLAY_ID("sidebar"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_FIXED(60) },
			.childGap = 10,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
	}) {
		for (const auto &tab : tabs) renderItem(tab);
	}
    // clang-format on
}
} // namespace components
