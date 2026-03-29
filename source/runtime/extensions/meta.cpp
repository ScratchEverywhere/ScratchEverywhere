#include "meta.hpp"
#include <array>
#include <nonstd/expected.hpp>
#include <string>

static nonstd::expected<std::string, std::string> readNullTerminatedString(std::istream &data) {
    std::string out;
    if (std::getline(data, out, '\0')) return out;
    if (data.eof()) return nonstd::make_unexpected("End of file.");
    return nonstd::make_unexpected("I/O Error.");
}

nonstd::expected<extensions::Extension, std::string> extensions::parseMetadate(std::istream &data) {
    constexpr std::string_view magicString = "SE! EXTENSION";

    Extension out = {};

    // Magic String
    std::array<char, magicString.length()> magicBuffer;
    if (!data.read(magicBuffer.data(), magicString.length())) return nonstd::make_unexpected("Could not read " + std::to_string(magicString.length()) + " bytes for magic string.");
    if (!std::equal(magicBuffer.begin(), magicBuffer.end(), magicString.begin())) return nonstd::make_unexpected("Invalid magic string: " + std::string(magicBuffer.begin(), magicBuffer.end()));

    // Version Stuff
    char formatVersion;
    if (!data.read(&formatVersion, 1)) return nonstd::make_unexpected("Could not read 1 byte for file format version. File too short or I/O error.");
    if (!data.read(reinterpret_cast<char *>(&out.minApiVersion.major), 1)) return nonstd::make_unexpected("Could not read 1 byte for major API version. File too short or I/O error.");
    if (!data.read(reinterpret_cast<char *>(&out.minApiVersion.minor), 1)) return nonstd::make_unexpected("Could not read 1 byte for minor API version. File too short or I/O error.");

    // Core
    if (!data.read(reinterpret_cast<char *>(&out.core), 1)) return nonstd::make_unexpected("Could not read 1 byte for core flag. File too short or I/O error.");

    // Info
    auto id = readNullTerminatedString(data);
    if (!id.has_value()) return nonstd::make_unexpected("Error parsing ID: " + id.error());
    out.id = id.value();

    auto name = readNullTerminatedString(data);
    if (!name.has_value()) return nonstd::make_unexpected("Error parsing name: " + name.error());
    out.name = name.value();

    auto description = readNullTerminatedString(data);
    if (!description.has_value()) return nonstd::make_unexpected("Error parsing ID: " + description.error());
    out.description = description.value();

    // Perms
    char permissionBytes[2];
    if (!data.read(permissionBytes, 2)) return nonstd::make_unexpected("Could not read 2 bytes for permissions. Fille too short or I/O error.");
    for (unsigned int i = 0; i < 10; i++) // Update as more permissions are added
        if ((((permissionBytes[0] << 8) | permissionBytes[1]) >> i) & 1) out.permissions.push_back(static_cast<ExtensionPermission>(i));

    // Platforms
    char platformBytes[2];
    if (!data.read(platformBytes, 2)) return nonstd::make_unexpected("Could not read 2 bytes for supported platforms. Fille too short or I/O error.");
    for (unsigned int i = 0; i < 12; i++) // Update as more platforms are added
        if ((((platformBytes[0] << 8) | platformBytes[1]) >> i) & 1) out.platforms.push_back(static_cast<ExtensionPlatform>(i));

    // Settings
    char numSettings;
    if (!data.read(&numSettings, 1)) return nonstd::make_unexpected("Could not read 1 byte for number of settings. File too short or I/O error.");
    for (int i = 0; i < numSettings; i++) {
        ExtensionSetting setting;
        if (!data.read(reinterpret_cast<char *>(&setting.type), 1)) return nonstd::make_unexpected("Could not read 1 byte for setting type. File too short or I/O error.");
        if (setting.type != ExtensionSettingType::SLIDER && setting.type != ExtensionSettingType::TEXT && setting.type != ExtensionSettingType::TOGGLE)
            return nonstd::make_unexpected("Unknown setting type: '" + std::to_string(static_cast<int>(setting.type)) + "'");

        auto id = readNullTerminatedString(data);
        auto name = readNullTerminatedString(data);
        if (!name.has_value() || !id.has_value()) return nonstd::make_unexpected("Error parsing settings: " + name.error());
        setting.name = name.value();

        switch (setting.type) {
        case ExtensionSettingType::TOGGLE: {
            char toggleDefault;
            if (!data.read(&toggleDefault, 1)) return nonstd::make_unexpected("Could not read 1 byte for default value. File too short or I/O error.");
            setting.defaultValue = toggleDefault != 0;
            break;
        }
        case ExtensionSettingType::TEXT: {
            auto textDefault = readNullTerminatedString(data);
            if (!textDefault.has_value()) return nonstd::make_unexpected("Error parsing settings: " + textDefault.error());
            setting.defaultValue = textDefault.value();

            auto prompt = readNullTerminatedString(data);
            if (!prompt.has_value()) return nonstd::make_unexpected("Error parsing settings: " + prompt.error());
            setting.prompt = prompt.value();
            break;
        }
        case ExtensionSettingType::SLIDER: {
            float sliderDefault;
            if (!data.read(reinterpret_cast<char *>(&sliderDefault), 4)) return nonstd::make_unexpected("Could not read 4 bytes for default value. File too short or I/O error.");
            setting.defaultValue = sliderDefault;
            if (!data.read(reinterpret_cast<char *>(&setting.min), 4)) return nonstd::make_unexpected("Could not read 4 bytes for slider minimum. File too short or I/O error.");
            if (!data.read(reinterpret_cast<char *>(&setting.max), 4)) return nonstd::make_unexpected("Could not read 4 bytes for slider maximum. File too short or I/O error.");
            if (!data.read(reinterpret_cast<char *>(&setting.snap), 4)) return nonstd::make_unexpected("Could not read 4 bytes for slider snap. File too short or I/O error.");
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
    if (!success) return nonstd::make_unexpected(data.eof() ? "Reached end of file while reading block types." : "Unknown error occurred while reading block types.");

    std::vector<std::string> blockIds;
    for (unsigned int i = 0; i < blockTypes.size(); i++) {
        auto id = readNullTerminatedString(data);
        if (!id.has_value()) return nonstd::make_unexpected("Error parsing block types: " + id.error());
        blockIds.push_back(id.value());
    }

    std::transform(blockIds.begin(), blockIds.end(), blockTypes.begin(), std::inserter(out.blockTypes, out.blockTypes.end()), [](const std::string &id, ExtensionBlockType type) { return std::make_pair(id, type); });

    return out;
}
