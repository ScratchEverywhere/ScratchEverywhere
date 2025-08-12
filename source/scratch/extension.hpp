#pragma once

#include "interpret.hpp"
#include "sprite.hpp"
#include <nlohmann/json.hpp>
#include <scratch-3ds.hpp>
#include <string>
#include <vector>

extern std::vector<struct Extension> extensions;

#ifdef SDL_BUILD
extern SDL_GameController *controller;
extern SDL_Window *window;
extern SDL_Renderer *renderer;
#elif defined(__3DS__)
extern C3D_RenderTarget *topScreen;
extern C3D_RenderTarget *bottomScreen;
#endif

struct Extension {
    std::string name;
    nlohmann::json types;
    void *handle;
};

inline ExtensionSound convertSoundToExtensionSound(Sound *sound) {
    return { sound->id, sound->name, sound->dataFormat, sound->fullName, sound->sampleRate, sound->sampleCount };
}

inline ExtensionComment convertCommentToExtensionComment(Comment *comment) {
    return { comment->id, comment->blockId, comment->text, comment->minimized, comment->x, comment->y, comment->width, comment->height };
}

inline ExtensionVariable convertVariableToExtensionVariable(Variable *variable) {
#ifdef CLOUDVARS_ENABLED
    return { variable->name, variable->id, variable->cloud, variable->value.asAny() };
#endif
    return { variable->name, variable->id, false, variable->value.asAny() };
}

inline ExtensionList convertListToExtensionList(List *list) {
    std::vector<std::any> values;
    for (const auto &item : list->items)
        values.push_back(item.asAny());

    return { list->name, list->id, values };
}

inline ExtensionCostume convertCostumeToExtensionCostume(Costume *costume) {
    return { costume->id, costume->name, costume->fullName, costume->dataFormat, costume->bitmapResolution, costume->rotationCenterX, costume->rotationCenterY };
}

inline ExtensionBroadcast convertBroadcastToExtensionBroadcast(Broadcast *broadcast) {
    return { broadcast->id, broadcast->name };
}

ExtensionSprite convertSpriteToExtensionSprite(Sprite *sprite);

inline ExtensionData createExtensionData(Sprite *sprite) {
    return {
        [sprite]() {
            return convertSpriteToExtensionSprite(sprite);
        },
        []() {
            std::vector<ExtensionSprite> extensionSprites;
            for (const auto &sprite : sprites)
                extensionSprites.push_back(convertSpriteToExtensionSprite(sprite));
            return extensionSprites;
        },
        [](std::string message) { Log::log(message); },
        [](std::string message) { Log::logWarning(message); },
        [](std::string message) { Log::logError(message); },
#ifdef SDL_BUILD
        controller,
        window,
        renderer
#elif defined(__3DS__)
        topScreen,
        bottomScreen
#endif
    };
}
