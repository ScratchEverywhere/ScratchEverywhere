#include "interface.hpp"
#include "blockExecutor.hpp"
#include "color.hpp"
#include "files.hpp"
#include "json.hpp"
#include "log.hpp"
#include "meta.hpp"
#include "sprite.hpp"
#include <os.hpp>
#include <runtime.hpp>
#include <sol/sol.hpp>

void extensions::loadLua(Extension *extension, std::istream &data) {
    extension->luaState.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::io, sol::lib::bit32, sol::lib::table, sol::lib::coroutine);

    extension->luaState.new_usertype<Color>("Color", sol::call_constructor, sol::factories([]() { return Color{0.0f, 0.0f, 0.0f, 1.0f}; }, [](float h, float s, float b, float t) { return Color{h, s, b, t}; }), "hue", &Color::hue, "saturation", &Color::saturation, "brightness", &Color::brightness, "transparency", &Color::transparency, "toRGBA", [](Color &color) { return CSBT2RGBA(color); });
    extension->luaState.new_usertype<ColorRGBA>("ColorRGBA", sol::call_constructor, sol::factories([]() { return ColorRGBA{0.0f, 0.0f, 0.0f, 1.0f}; }, [](float r, float g, float b, float a) { return ColorRGBA{r, g, b, a}; }), "r", &ColorRGBA::r, "g", &ColorRGBA::g, "b", &ColorRGBA::b, "a", &ColorRGBA::a, "toCSBT", [](ColorRGBA &rgba) { return RGBA2CSBO(rgba); });

    json::registerAPI(extension);
    files::registerAPI(extension);

    extension->luaState["blocks"] = extension->luaState.create_table();
    if (extension->hasPermission(ExtensionPermission::UPDATE)) extension->luaState["update"] = extension->luaState.create_table();

    char buffer[1024];
    struct ReadData {
        std::istream *in;
        char *buffer;
    } readData = {&data, buffer};
    sol::load_result loadResult = extension->luaState.load(
        [](lua_State *L, void *data, size_t *size) -> const char * {
            auto readData = reinterpret_cast<ReadData *>(data);
            if (!readData->in || readData->in->fail() || readData->in->eof()) {
                *size = 0;
                return nullptr;
            }

            readData->in->read(readData->buffer, sizeof(buffer));
            *size = static_cast<size_t>(readData->in->gcount());
            return readData->buffer;
        },
        &readData);

    if (!loadResult.valid()) {
        Log::logError("Failed to load lua bytecode for '" + extension->id + "': " + static_cast<sol::error>(loadResult).what());
        return;
    }

    sol::protected_function_result result = static_cast<sol::protected_function>(loadResult)();
    if (!result.valid()) Log::logError("Error while running lua bytecode for extension '" + extension->id + "': " + static_cast<sol::error>(result).what());
}

sol::table extensions::getBlockArgs(Extension *extension, Block *block, ScriptThread *thread, Sprite *sprite) {
    sol::table table = extension->luaState.create_table();
    for (const auto &input : block->inputs) {
        Value value;
        if (!Scratch::getInput(block, input.first, thread, sprite, value)) continue;
        if (value.isUndefined()) continue;
        if (value.isBoolean()) {
            table[input.first] = value.asBoolean();
            continue;
        }
        if (value.isDouble()) {
            table[input.first] = value.asDouble();
            continue;
        }
        if (value.isColor()) {
            table[input.first] = value.asColor();
            continue;
        }
        table[input.first] = value.asString();
    }
    for (const auto &field : block->fields)
        table[field.first] = field.second.value;
    return table;
}

void extensions::registerHandlers(Extension *extension) {
    for (const auto &extensionBlock : extension->blockTypes) {
        std::string blockId;
        if (extension->core) blockId = extensionBlock.first;
        else blockId = extension->id + "_" + extensionBlock.first;

        BlockExecutor::getHandlers()[blockId] = [extension, extensionBlock](Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue) -> BlockResult {
            sol::protected_function func = extension->luaState["blocks"][extensionBlock.first];
            sol::protected_function_result result = func(extensions::getBlockArgs(extension, block, thread, sprite));
            if (!result.valid()) Log::logError("Error running extension block '" + block->opcode + "': " + static_cast<sol::error>(result).what());
            sol::object resultObj = result;
            switch (extensionBlock.second) {
            case ExtensionBlockType::COMMAND:
                break;
            case ExtensionBlockType::BOOLEAN:
            case ExtensionBlockType::REPORTER:
                if (resultObj.is<std::string>()) *outValue = Value(resultObj.as<std::string>());
                else if (resultObj.is<int>()) *outValue = Value(resultObj.as<int>());
                else if (resultObj.is<double>()) *outValue = Value(resultObj.as<double>());
                else if (resultObj.is<bool>()) *outValue = Value(resultObj.as<bool>());
                else if (resultObj.is<Color>()) *outValue = Value(resultObj.as<Color>());
                else if (resultObj.is<ColorRGBA>()) *outValue = Value(RGBA2CSBO(resultObj.as<ColorRGBA>()));
                else {
                    Log::logWarning("Unknown return type from extension block: " + block->opcode);
                    *outValue = Value(Undefined{});
                }

                break;
            case ExtensionBlockType::HAT:
            case ExtensionBlockType::EVENT:
                if (!resultObj.is<bool>()) {
                    Log::logError("Extension block '" + block->opcode + "' returned an invalid type.");
                    return BlockResult::RETURN;
                }

                return result.get<bool>() ? BlockResult::CONTINUE : BlockResult::RETURN;
            }
            return BlockResult::CONTINUE;
        };
    }
}

void extensions::registerHandlers() {
    for (const auto &extension : Scratch::extensions) {
        registerHandlers(extension.get());
    }
}

void extensions::runUpdateFunctions(ExtensionUpdateFunction type) {
    for (const auto &extension : Scratch::extensions)
        runUpdateFunction(extension.get(), type);
}

void extensions::runUpdateFunction(Extension *extension, ExtensionUpdateFunction type) {
    if (!extension->hasPermission(ExtensionPermission::UPDATE)) return;

    const sol::object updateFn = extension->luaState["update"][updateFunctionString(type)];
    if (!updateFn.is<sol::function>()) return;
    sol::protected_function_result result = updateFn.as<sol::protected_function>()();
    if (!result.valid()) Log::logError("Error running update function for extension '" + extension->id + "': " + static_cast<sol::error>(result).what());
}
