#pragma once
#include <se_export.hpp>

#include "meta.hpp"
#include "sprite.hpp"
#include <sol/sol.hpp>

namespace extensions {
SE_EXPORT void loadLua(Extension *extension, std::istream &data);

SE_EXPORT sol::object valueToObject(sol::state_view luaState, Value val);
SE_EXPORT Value objectToValue(sol::object object);

SE_EXPORT sol::table getBlockArgs(Extension *extension, Block *block, ScriptThread *thread, Sprite *sprite);
SE_EXPORT void registerHandlers(Extension *extension);
SE_EXPORT void registerHandlers();

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

SE_EXPORT void runUpdateFunctions(ExtensionUpdateFunction type);
SE_EXPORT void runUpdateFunction(Extension *extension, ExtensionUpdateFunction type);

SE_EXPORT void cleanup();
} // namespace extensions
