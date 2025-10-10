#include <cstdint>
#include <expected>
#include <istream>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace extensions {

// Format version isn't here because I plan to support all file formats throughout all versions of SE!
const uint8_t apiMajorVersion = 0;
const uint8_t apiMinorVersion = 0;

enum ExtensionPermission {
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

enum ExtensionPlatform {
    N3DS,
    WIIU,
    WII,
    GAMECUBE,
    SWITCH,
    PC,
    VITA
};

enum ExtensionBlockType {
    COMMAND = 0x1,
    HAT = 0x2,
    EVENT = 0x3,
    REPORTER = 0x4,
    BOOLEAN = 0x5
};

enum ExtensionSettingType {
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

struct ExtensionFile {
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

    // TODO: Add the lua stuff here.
};

std::expected<ExtensionFile, std::string> parse(std::istream &data);

} // namespace extensions
