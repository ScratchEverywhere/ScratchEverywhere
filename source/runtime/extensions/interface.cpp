#include "interface.hpp"
#include "meta.hpp"
#include <os.hpp>
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
