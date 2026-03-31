#include "interface.hpp"
#include "blockExecutor.hpp"
#include "meta.hpp"
#include <os.hpp>
#include <runtime.hpp>
#include <sol/sol.hpp>

void extensions::loadLua(Extension *extension, std::istream &data) {
    extension->luaState.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::io, sol::lib::bit32, sol::lib::table, sol::lib::coroutine);

    // TODO: Load our APIs

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

void extensions::registerHandlers(Extension *extension) {
    for (const auto &extensionBlock : extension->blockTypes) {
        std::string blockId;
        if (extension->core) blockId = extensionBlock.first;
        else blockId = extension->id + "_" + extensionBlock.first;

        switch (extensionBlock.second) {
        case ExtensionBlockType::COMMAND:
            BlockExecutor::getHandlers()[blockId] = [extension, extensionBlock](Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) -> BlockResult {
                sol::protected_function func = extension->luaState["blocks"][extensionBlock.first];
                sol::protected_function_result result = func();
                if (!result.valid()) Log::logError("Error running extension block '" + block.opcode + "': " + static_cast<sol::error>(result).what());
                return BlockResult::CONTINUE;
            };
            break;
        case ExtensionBlockType::BOOLEAN:
        case ExtensionBlockType::REPORTER:
            break;
        case ExtensionBlockType::HAT:
        case ExtensionBlockType::EVENT:
            break;
        }
    }
}

void extensions::registerHandlers() {
    for (const auto &extension : Scratch::extensions) {
        registerHandlers(extension.get());
    }
}
