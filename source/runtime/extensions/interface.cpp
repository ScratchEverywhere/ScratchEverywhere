#include "interface.hpp"
#include "blockExecutor.hpp"
#include "color.hpp"
#include "files.hpp"
#include "json.hpp"
#include "log.hpp"
#include "meta.hpp"
#include "runtime.hpp"
#include "sprite.hpp"
#include "timer.hpp"
#include "value.hpp"
#include <os.hpp>
#include <runtime.hpp>
#include <sol/sol.hpp>

struct CallbackRegistry {
    std::unordered_map<std::string, std::vector<sol::protected_function>> specificCallbacks;
    std::vector<sol::protected_function> globalCallbacks;
};

std::unordered_map<extensions::Extension *, CallbackRegistry> extensionCallbacks;

void extensions::loadLua(Extension *extension, std::istream &data) {
    extension->luaState.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::io, sol::lib::bit32, sol::lib::table, sol::lib::coroutine);

    extension->luaState.new_usertype<Value>("Value", sol::call_constructor, sol::factories([](int val) { return Value(val); }, [](Color val) { return Value(val); }, [](double val) { return Value(val); }, [](std::string val) { return Value(val); }, [](bool val) { return Value(val); }), "asBoolean", &Value::asBoolean, "asString", &Value::asString, "asColor", &Value::asColor, "asNumber", &Value::asDouble, "isNumber", &Value::isDouble, "isString", &Value::isString, "isColor", &Value::isColor, "isBoolean", &Value::isBoolean, "isUndefined", &Value::isUndefined, "isNaN", &Value::isNaN, "isScratchInt", &Value::isScratchInt, "isNumeric", &Value::isNumeric);
    extension->luaState.new_usertype<Timer>("Timer", sol::call_constructor, sol::factories([]() { return Timer(); }, [](bool autoStart) { return Timer(autoStart); }), "start", &Timer::start, "getTimeMs", &Timer::getTimeMsDouble, "hasElapsed", &Timer::hasElapsed, "hasElapsedAndRestart", &Timer::hasElapsedAndRestart);
    extension->luaState.new_usertype<Color>("Color", sol::call_constructor, sol::factories([]() { return Color{0.0f, 0.0f, 0.0f, 1.0f}; }, [](float h, float s, float b, float t) { return Color{h, s, b, t}; }), "hue", &Color::hue, "saturation", &Color::saturation, "brightness", &Color::brightness, "transparency", &Color::transparency, "toRGBA", [](Color &color) { return CSBT2RGBA(color); });
    extension->luaState.new_usertype<ColorRGBA>("ColorRGBA", sol::call_constructor, sol::factories([]() { return ColorRGBA{0.0f, 0.0f, 0.0f, 1.0f}; }, [](float r, float g, float b, float a) { return ColorRGBA{r, g, b, a}; }), "r", &ColorRGBA::r, "g", &ColorRGBA::g, "b", &ColorRGBA::b, "a", &ColorRGBA::a, "toCSBT", [](ColorRGBA &rgba) { return RGBA2CSBO(rgba); });

    json::registerAPI(extension);
    files::registerAPI(extension);
    runtime::registerAPI(extension);

    // Extensions API
    if (extension->hasPermission(ExtensionPermission::EXTENSIONS)) {
        extension->luaState["extensions"] = extension->luaState.create_table();
        extension->luaState["extensions"][sol::metatable_key] = extension->luaState.create_table();

        extension->luaState["extensions"][sol::metatable_key][sol::meta_function::index] = [](sol::table t, std::string key) -> sol::object {
            auto luaState = t.lua_state();
            sol::state_view currentLua(luaState);

            if (key == "active") {
                sol::table activeTable = currentLua.create_table();
                for (const auto &ext : Scratch::extensions) {
                    activeTable.add(ext->id);
                }
                return activeTable;
            }

            if (key == "installed") {
                return currentLua.create_table(); // TODO: Implement :)
            }

            if (key == "embedded") {
                return currentLua.create_table(); // TODO: Implement :)
            }

            return t.raw_get<sol::object>(key);
        };

        extension->luaState["extensions"]["send"] = [extension](std::string targetId, std::string message) {
            for (const auto &ext : Scratch::extensions) {
                if (ext->id != targetId) continue;

                const auto &it = extensionCallbacks.find(ext.get());
                if (it != extensionCallbacks.end()) {
                    const auto &specificMap = it->second.specificCallbacks;
                    const auto &specificIt = specificMap.find(extension->id);
                    if (specificIt != specificMap.end()) {
                        for (const auto &cb : specificIt->second) {
                            if (cb.valid()) cb(message);
                        }
                    }

                    for (const auto &cb : it->second.globalCallbacks) {
                        if (cb.valid()) cb(message);
                    }
                }
                break;
            }
        };

        extension->luaState["extensions"]["sendAll"] = [extension](std::string message) {
            for (const auto &ext : Scratch::extensions) {
                const auto &it = extensionCallbacks.find(ext.get());
                if (it == extensionCallbacks.end()) continue;

                const auto &specificMap = it->second.specificCallbacks;
                const auto &specificIt = specificMap.find(extension->id);
                if (specificIt != specificMap.end()) {
                    for (const auto &cb : specificIt->second) {
                        if (cb.valid()) cb(message);
                    }
                }

                for (const auto &cb : it->second.globalCallbacks) {
                    if (cb.valid()) cb(message);
                }
            }
        };

        extension->luaState["extensions"]["receive"] = [extension](std::string senderId, sol::protected_function callback) {
            extensionCallbacks[extension].specificCallbacks[senderId].push_back(callback);
        };

        extension->luaState["extensions"]["receiveAll"] = [extension](sol::protected_function callback) {
            extensionCallbacks[extension].globalCallbacks.push_back(callback);
        };

        auto &lua = extension->luaState;
        extension->luaState["extensions"]["getInfo"] = [&lua](std::string targetId) -> sol::object {
            for (const auto &ext : Scratch::extensions) {
                if (ext->id != targetId) continue;

                sol::table info = lua.create_table();
                info["core"] = ext->core;
                info["id"] = ext->id;
                info["name"] = ext->name;
                info["description"] = ext->description;

                sol::table perms = lua.create_table();
                for (const auto &perm : ext->permissions) {
                    perms.add(permissionToString(perm));
                }
                info["permissions"] = perms;

                sol::table platforms = lua.create_table();
                for (const auto &plat : ext->platforms) {
                    platforms.add(platformToString(plat));
                }
                info["platforms"] = platforms;

                return info;
            }
            return sol::nil;
        };
    }

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

Value extensions::objectToValue(sol::object object) {
    if (object.is<Value>()) return object.as<Value>();
    if (object.is<std::string>()) return Value(object.as<std::string>());
    if (object.is<int>()) return Value(object.as<int>());
    if (object.is<double>()) return Value(object.as<double>());
    if (object.is<bool>()) return Value(object.as<bool>());
    if (object.is<Color>()) return Value(object.as<Color>());
    Log::logWarning("Unknown object type from Lua.");
    return Value(Undefined{});
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
            runtime::setThread(thread);
            runtime::setSprite(sprite);
            runtime::setBlock(block);

            sol::protected_function func = extension->luaState["blocks"][extensionBlock.first];
            sol::protected_function_result result = func(extensions::getBlockArgs(extension, block, thread, sprite));
            if (!result.valid()) {
                Log::logError("Error running extension block '" + block->opcode + "': " + static_cast<sol::error>(result).what());
                runtime::clearData();
                return BlockResult::CONTINUE;
            }
            sol::object resultObj = result;
            switch (extensionBlock.second) {
            case ExtensionBlockType::COMMAND:
                break;
            case ExtensionBlockType::BOOLEAN:
            case ExtensionBlockType::REPORTER:
                *outValue = objectToValue(resultObj);
                break;
            case ExtensionBlockType::HAT:
            case ExtensionBlockType::EVENT:
                if (!resultObj.is<bool>()) {
                    Log::logError("Extension block '" + block->opcode + "' returned an invalid type.");
                    runtime::clearData();
                    return BlockResult::RETURN;
                }

                runtime::clearData();
                return result.get<bool>() ? BlockResult::CONTINUE : BlockResult::RETURN;
            }
            runtime::clearData();
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

void extensions::cleanup() {
    Scratch::extensions.clear();
    extensionCallbacks.clear();
}
