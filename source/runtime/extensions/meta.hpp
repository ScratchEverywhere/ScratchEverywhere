#pragma once

#include <istream>
#include <map>
#include <memory>
#include <nonstd/expected.hpp>
#include <optional>
#include <stdint.h>
#include <string>
#include <variant>
#include <vector>

#include <sol/sol.hpp>

namespace extensions {
constexpr uint8_t apiMajorVersion = 0;
constexpr uint8_t apiMinorVersion = 0;

enum class ExtensionPermission {
    LOCALFS,
    ROOTFS,
    NETWORK,
    INPUT,
    RENDER,
    UPDATE,
    PLATFORM_SPECIFIC,
    RUNTIME,
    AUDIO,
    EXTENSIONS
};

enum class ExtensionPlatform {
    PLATFORM_N3DS,
    PLATFORM_WIIU,
    PLATFORM_WII,
    PLATFORM_GAMECUBE,
    PLATFORM_SWITCH,
    PLATFORM_PC,
    PLATFORM_VITA,
    PLATFORM_NDS,
    PLATFORM_PS4,
    PLATFORM_PSP,
    PLATFORM_WEBOS,
    PLATFORM_WASM
};

inline std::string permissionToString(extensions::ExtensionPermission perm) {
    switch (perm) {
    case extensions::ExtensionPermission::LOCALFS:
        return "localfs";
    case extensions::ExtensionPermission::ROOTFS:
        return "rootfs";
    case extensions::ExtensionPermission::NETWORK:
        return "network";
    case extensions::ExtensionPermission::INPUT:
        return "input";
    case extensions::ExtensionPermission::RENDER:
        return "render";
    case extensions::ExtensionPermission::UPDATE:
        return "update";
    case extensions::ExtensionPermission::PLATFORM_SPECIFIC:
        return "platform-specific";
    case extensions::ExtensionPermission::RUNTIME:
        return "runtime";
    case extensions::ExtensionPermission::AUDIO:
        return "audio";
    case extensions::ExtensionPermission::EXTENSIONS:
        return "extensions";
    default:
        return "unknown";
    }
}

inline std::string platformToString(extensions::ExtensionPlatform plat) {
    switch (plat) {
    case extensions::ExtensionPlatform::PLATFORM_N3DS:
        return "3ds";
    case extensions::ExtensionPlatform::PLATFORM_WIIU:
        return "wiiu";
    case extensions::ExtensionPlatform::PLATFORM_WII:
        return "wii";
    case extensions::ExtensionPlatform::PLATFORM_GAMECUBE:
        return "gamecube";
    case extensions::ExtensionPlatform::PLATFORM_SWITCH:
        return "switch";
    case extensions::ExtensionPlatform::PLATFORM_PC:
        return "pc";
    case extensions::ExtensionPlatform::PLATFORM_VITA:
        return "vita";
    case extensions::ExtensionPlatform::PLATFORM_NDS:
        return "nds";
    case extensions::ExtensionPlatform::PLATFORM_PS4:
        return "ps4";
    case extensions::ExtensionPlatform::PLATFORM_PSP:
        return "psp";
    case extensions::ExtensionPlatform::PLATFORM_WEBOS:
        return "webos";
    case extensions::ExtensionPlatform::PLATFORM_WASM:
        return "wasm";
    default:
        return "unknown";
    }
}

enum class ExtensionBlockType {
    COMMAND = 0x1,
    HAT = 0x2,
    EVENT = 0x3,
    REPORTER = 0x4,
    BOOLEAN = 0x5
};

enum class ExtensionSettingType {
    TOGGLE = 0x12,
    TEXT = 0x63,
    SLIDER = 0x6e
};

struct ExtensionSetting {
    std::string name;
    ExtensionSettingType type;
    std::variant<std::string, bool, float> defaultValue;
    std::optional<float> min;
    std::optional<float> max;
    std::optional<float> snap;
    std::optional<std::string> prompt;
};

struct Extension {
    bool core;
    std::string id;
    std::string name;
    std::string description;
    std::vector<ExtensionPermission> permissions;
    std::vector<ExtensionPlatform> platforms;
    struct {
        uint8_t major;
        uint8_t minor;
    } minApiVersion;
    std::map<std::string, ExtensionBlockType> blockTypes;
    std::map<std::string, ExtensionSetting> settings;

    sol::state luaState;

    inline bool hasPermission(const ExtensionPermission permission) {
        return std::find(permissions.begin(), permissions.end(), permission) != permissions.end();
    }
};

nonstd::expected<std::unique_ptr<Extension>, std::string> parseMetadata(std::istream &data);
} // namespace extensions
