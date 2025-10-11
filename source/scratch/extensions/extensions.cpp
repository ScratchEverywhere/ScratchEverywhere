#include "extensions.hpp"
#include "interpret.hpp"
#include "os.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <istream>
#include <string_view>

static_assert(sizeof(bool) == 1, "Unsupported bool type detected.");

namespace extensions {

std::map<std::string, std::unique_ptr<Extension>> extensions;

static std::expected<std::string, std::string> readNullTerminatedString(std::istream &data) {
    std::string out;
    if (std::getline(data, out, '\0')) return out;
    if (data.eof()) return std::unexpected("End of file.");
    return std::unexpected("I/O Error.");
}

std::expected<Extension, std::string> parseMetadata(std::istream &data) {
    constexpr std::string_view magicString = "SE! EXTENSION";
    constexpr size_t magicLength = magicString.length();

    Extension out = {};

    // Magic String Check
    std::array<char, magicLength> magicBuffer;
    if (!data.read(magicBuffer.data(), magicLength)) return std::unexpected("Could not read " + std::to_string(magicLength) + "bytes for magic string. File too short or I/O error.");
    if (!std::equal(magicBuffer.begin(), magicBuffer.end(), magicString.begin())) return std::unexpected("Invalid magic string: " + std::string(magicBuffer.begin(), magicBuffer.end()));

    // Version Stuff
    char formatVersion; // unused for now but once there are multiple versions of the format this will be necessary.
    if (!data.read(&formatVersion, 1)) return std::unexpected("Could not read 1 byte for file format version. File too short or I/O error.");
    if (!data.read(reinterpret_cast<char *>(&out.minApiVersion.major), 1)) return std::unexpected("Could not read 1 byte for major API version. File too short or I/O error.");
    if (!data.read(reinterpret_cast<char *>(&out.minApiVersion.minor), 1)) return std::unexpected("Could not read 1 byte for minor API version. File too short or I/O error.");

    // Core
    if (!data.read(reinterpret_cast<char *>(&out.core), 1)) return std::unexpected("Could not read 1 byte for core flag. File too short or I/O error.");

    // Info (ID, Name, and Description)
    auto id = readNullTerminatedString(data);
    if (!id.has_value()) return std::unexpected("Error parsing ID: " + id.error());
    out.id = id.value();

    auto name = readNullTerminatedString(data);
    if (!name.has_value()) return std::unexpected("Error parsing name: " + name.error());
    out.name = name.value();

    auto description = readNullTerminatedString(data);
    if (!description.has_value()) return std::unexpected("Error parsing ID: " + description.error());
    out.description = description.value();

    // Permissions
    uint8_t permissionBytes[2];
    if (!data.read(reinterpret_cast<char *>(permissionBytes), 2)) return std::unexpected("Could not read 2 bytes for permissions. Fille too short or I/O error.");
    for (unsigned int i = 0; i < 10; i++) // Update as more permissions are added
        if ((((permissionBytes[0] << 8) | permissionBytes[1]) >> i) & 1) out.permissions.push_back(static_cast<ExtensionPermission>(i));

    // Platforms
    char platforms;
    if (!data.read(&platforms, 1)) return std::unexpected("Could not read 1 byte for platforms. Fille too short or I/O error.");
    for (unsigned int i = 0; i < 7; i++) // Update as more platforms are added
        if ((platforms >> i) & 1) out.platforms.push_back(static_cast<ExtensionPlatform>(i));

    // Settings
    char numSettings;
    if (!data.read(&numSettings, 1)) return std::unexpected("Could not read 1 byte for number of settings. File too short or I/O error.");
    for (int i = 0; i < numSettings; i++) {
        ExtensionSetting setting;
        if (!data.read(reinterpret_cast<char *>(&setting.type), 1)) return std::unexpected("Could not read 1 byte for setting type. File too short or I/O error.");
        if (setting.type != SLIDER && setting.type != TEXT && setting.type != TOGGLE) return std::unexpected("Unknown setting type: '" + std::to_string(setting.type) + "'");

        auto id = readNullTerminatedString(data);
        auto name = readNullTerminatedString(data);
        if (!name.has_value() || !id.has_value()) return std::unexpected("Error parsing settings: " + name.error());
        setting.name = name.value();

        switch (setting.type) {
        case TOGGLE: {
            char toggleDefault;
            if (!data.read(&toggleDefault, 1)) return std::unexpected("Could not read 1 byte for default value. File too short or I/O error.");
            setting.defaultValue = toggleDefault != 0;
            break;
        }
        case TEXT: {
            auto textDefault = readNullTerminatedString(data);
            if (!textDefault.has_value()) return std::unexpected("Error parsing settings: " + textDefault.error());
            setting.defaultValue = textDefault.value();

            auto prompt = readNullTerminatedString(data);
            if (!prompt.has_value()) return std::unexpected("Error parsing settings: " + prompt.error());
            setting.prompt = prompt.value();
            break;
        }
        case SLIDER: {
            float sliderDefault;
            if (!data.read(reinterpret_cast<char *>(&sliderDefault), 4)) return std::unexpected("Could not read 4 bytes for default value. File too short or I/O error.");
            setting.defaultValue = sliderDefault;
            if (!data.read(reinterpret_cast<char *>(&setting.min), 4)) return std::unexpected("Could not read 4 bytes for slider minimum. File too short or I/O error.");
            if (!data.read(reinterpret_cast<char *>(&setting.max), 4)) return std::unexpected("Could not read 4 bytes for slider maximum. File too short or I/O error.");
            if (!data.read(reinterpret_cast<char *>(&setting.snap), 4)) return std::unexpected("Could not read 4 bytes for slider snap. File too short or I/O error.");
            break;
        }
        }

        out.settings[id.value()] = setting;
    }

    // Block Types
    std::vector<ExtensionBlockType> blockTypes;
    char c;
    bool success = false;
    while (data.get(c)) {
        if (c == '\0') {
            success = true;
            break;
        }
        blockTypes.push_back(static_cast<ExtensionBlockType>(c));
    }
    if (!success) return std::unexpected(data.eof() ? "Reached end of file while reading block types." : "Unknown error occurred while reading block types.");

    std::vector<std::string> blockIds;
    for (unsigned int i = 0; i < blockTypes.size(); i++) {
        auto id = readNullTerminatedString(data);
        if (!id.has_value()) return std::unexpected("Error parsing block types: " + id.error());
        blockIds.push_back(id.value());
    }

    std::transform(blockIds.begin(), blockIds.end(), blockTypes.begin(), std::inserter(out.blockTypes, out.blockTypes.end()), [](const std::string &id, ExtensionBlockType type) { return std::make_pair(id, type); });

    return out;
}

void loadLua(Extension &extension, std::istream &data) {
    extension.luaState.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::io, sol::lib::bit32);

    extension.luaState["blocks"] = extension.luaState.create_table();

    char buffer[1024];
    struct ReadData {
        std::istream *in;
        char *buffer;
    } readData = {&data, buffer};
    sol::load_result loadResult = extension.luaState.load(
        [](lua_State *L, void *data, size_t *size) -> const char * {
            auto readData = reinterpret_cast<ReadData *>(data);
            if (!readData->in || readData->in->fail() || readData->in->eof()) {
                *size = 0;
                return nullptr;
            }

            readData->in->read(readData->buffer, 1024);
            *size = static_cast<size_t>(readData->in->gcount());
            return readData->buffer;
        },
        &readData);

    if (!loadResult.valid()) {
        Log::logError("Failed to load lua bytecode for extension '" + extension.id + "': " + static_cast<sol::error>(loadResult).what());
        return;
    }

    sol::protected_function_result result = static_cast<sol::protected_function>(loadResult)();
    if (!result.valid()) Log::logError("Error while running lua bytecode for extension '" + extension.id + "': " + static_cast<sol::error>(result).what());
}

void registerHandlers(BlockExecutor *blockExecutor) {
    for (auto &extension : extensions)
        registerHandlers(*(extension.second), blockExecutor);
}

void registerHandlers(Extension &extension, BlockExecutor *blockExecutor) {
    const auto getArgs = [](Extension *extension, Block &block, Sprite *sprite) {
        sol::table table = extension->luaState.create_table();
        for (const auto &parsedInput : *block.parsedInputs) {
            Value val = Scratch::getInputValue(block, parsedInput.first, sprite);
            if (val.isBoolean()) {
                table[parsedInput.first] = val.asInt() == 1;
                continue;
            }
            if (val.isDouble() || val.isInteger()) {
                table[parsedInput.first] = val.asInt();
                continue;
            }
            table[parsedInput.first] = val.asString();
        }
        for (const auto &parsedField : *block.parsedFields)
            table[parsedField.first] = parsedField.second.value;
        return table;
    };

    for (auto &extensionBlock : extension.blockTypes) {
        switch (extensionBlock.second) {
        case COMMAND:
            blockExecutor->handlers[extension.id + "_" + extensionBlock.first] = [&](Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
                extension.luaState["blocks"][extensionBlock.first](getArgs(&extension, block, sprite));
                return BlockResult::CONTINUE;
            };
            break;
        case REPORTER:
        case BOOLEAN:
            blockExecutor->valueHandlers[extension.id + "_" + extensionBlock.first] = [&](Block &block, Sprite *sprite) {
                sol::object ret = extension.luaState["blocks"][extensionBlock.first](getArgs(&extension, block, sprite));
                if (ret.is<std::string>()) return Value(ret.as<std::string>());
                if (ret.is<double>()) return Value(ret.as<double>());
                if (ret.is<bool>()) return Value(ret.as<bool>());
                Log::logError("Unknown return type from extension block, this should never happen.");
                return Value();
            };
            break;
        case HAT:
            Log::logWarning("Hat blocks in custom extensions are not supported yet.");
            break;
        case EVENT:
            Log::logWarning("Event blocks in custom extensions are not supported yet.");
            break;
        }
    }
}

} // namespace extensions
