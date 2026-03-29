#pragma once

#include <istream>
#include <map>
#include <nonstd/expected.hpp>
#include <optional>
#include <stdint.h>
#include <string>
#include <variant>
#include <vector>

#define SOL_ALL_SAFETIES_ON 1
#define SOL_LUA_VERSION 501
#ifdef USE_LUAJIT
#define SOL_USE_LUAJIT 1
#endif
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
};

nonstd::expected<Extension, std::string> parseMetadate(std::istream &data);
} // namespace extensions
