#pragma once

#include "meta.hpp"
#include "sprite.hpp"
#include <sol/sol.hpp>

namespace extensions {
void loadLua(Extension *extension, std::istream &data);

Value objectToValue(sol::object object);

sol::table getBlockArgs(Extension *extension, Block *block, ScriptThread *thread, Sprite *sprite);
void registerHandlers(Extension *extension);
void registerHandlers();

enum ExtensionUpdateFunction {
    PRE_UPDATE,
    POST_UPDATE,
    PRE_RENDER,
    POST_RENDER
};

static inline std::string updateFunctionString(ExtensionUpdateFunction type) {
    switch (type) {
    case PRE_UPDATE:
        return "preUpdate";
    case POST_UPDATE:
        return "postUpdate";
    case PRE_RENDER:
        return "preRender";
    case POST_RENDER:
        return "postRender";
    }
}

void runUpdateFunctions(ExtensionUpdateFunction type);
void runUpdateFunction(Extension *extension, ExtensionUpdateFunction type);
} // namespace extensions
