#include "parser.hpp"
#include "types.hpp"
#include <algorithm>
#include <array>
#include <filesystem.hpp>
#include <fstream>
#include <log.hpp>
#include <memory>
#include <os.hpp>
#include <runtime.hpp>
#include <settings.hpp>
#include <unordered_map>
#if defined(__WIIU__) && defined(ENABLE_CLOUDVARS)
#include <whb/sdcard.h>
#endif

#ifdef ENABLE_CUSTOM_EXTENSIONS
#include <extensions/interface.hpp>
#include <extensions/meta.hpp>
#endif
#ifdef ENABLE_NATIVE_EXTENSIONS
#include <dlfcn.h>
#endif

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

#ifdef ENABLE_CLOUDVARS
#include <mist/mist.hpp>
#include <random>
#include <sstream>

const uint64_t FNV_PRIME_64 = 1099511628211ULL;
const uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037ULL;

std::string Scratch::cloudUsername;
bool Scratch::cloudProject = false;

std::unique_ptr<MistConnection> cloudConnection = nullptr;
#endif

#ifdef ENABLE_CLOUDVARS
void Parser::initMist() {
    OS::initWifi();

    const std::string usernameFilename = OS::getScratchFolderLocation() + "cloud-username.txt";

    std::ifstream fileStream(usernameFilename.c_str());
    if (!fileStream.good()) {
        std::random_device rd;
        std::ostringstream usernameStream;
        usernameStream << "player" << std::setw(7) << std::setfill('0') << rd() % 10000000;
        Scratch::cloudUsername = usernameStream.str();
        std::ofstream usernameFile;
        usernameFile.open(usernameFilename);
        usernameFile << Scratch::cloudUsername;
        usernameFile.close();
    } else {
        fileStream >> Scratch::cloudUsername;
    }
    fileStream.close();

    std::vector<std::string> assetIds;
    for (const auto &sprite : Scratch::sprites) {
        for (const auto &costume : sprite->costumes) {
            assetIds.push_back(costume.id);
        }
        for (const auto &sound : sprite->sounds) {
            assetIds.push_back(sound.id);
        }
    }

    uint64_t assetHash = 0;
    for (const auto &assetId : assetIds) {
        uint64_t hash = FNV_OFFSET_BASIS_64;
        for (char c : assetId) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
            hash *= FNV_PRIME_64;
        }

        assetHash += hash;
    }

    std::ostringstream projectID;
    projectID << "ScratchEverywhere/hash-" << std::hex << std::setw(16) << std::setfill('0') << assetHash;
    cloudConnection = std::make_unique<MistConnection>(projectID.str(), Scratch::cloudUsername, "contact@grady.link");

    cloudConnection->onConnectionStatus([](bool connected, const std::string &message) {
        if (connected) {
            Log::log("Mist++ Connected: " + message);
            return;
        }
        Log::log("Mist++ Disconnected: " + message);
    });

    cloudConnection->onVariableUpdate(BlockExecutor::handleCloudVariableChange);

    Log::log("Connecting to cloud variables with id: " + projectID.str());
#if defined(__PC__) && !(defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__))
    cloudConnection->connect();
#else
    cloudConnection->connect(false);
#endif
}
#endif

std::unordered_map<std::string, std::string> &Parser::getShadowBlocks() {
    static std::unordered_map<std::string, std::string> shadowBlocks;
    return shadowBlocks;
}

void Parser::loadUsernameFromSettings() {
    Scratch::customUsername = "Player";
    Scratch::useCustomUsername = false;

    nlohmann::json j = SettingsManager::getConfigSettings();

    if (j.contains("EnableUsername") && j["EnableUsername"].is_boolean()) {
        Scratch::useCustomUsername = j["EnableUsername"].get<bool>();
    }

    if (j.contains("Username") && j["Username"].is_string()) {
        bool hasNonSpace = false;
        for (char c : j["Username"].get<std::string>()) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                hasNonSpace = true;
            } else if (!std::isspace(static_cast<unsigned char>(c))) {
                break;
            }
        }
        if (hasNonSpace) Scratch::customUsername = j["Username"].get<std::string>();
        else Scratch::customUsername = "Player";
    }
}

bool Parser::logParsing = false;

void Parser::log(const std::string &message) {
    if (Parser::logParsing) {
        Log::log(message);
    }
}

void Parser::loadSprites(const nlohmann::json &json, ProjectFormat format) {
    switch (format) {
    case ProjectFormat::SB3:
        loadSpritesSb3(json);
        return;
    case ProjectFormat::SB2:
        loadSpritesSb2(json);
        return;
    case ProjectFormat::SB1:
        // SB1 is not JSON-shaped - its ProjectLoader builds Scratch::sprites
        // itself and should never call through here.
        Log::logCritical("Parser::loadSprites() called with ProjectFormat::SB1; SB1 projects must be handled by their own ProjectLoader, not Parser.", false);
        return;
    }
}

static constexpr std::array<std::string_view, 13> builtInExtensions = {"music", "pen", "videoSensing", "text2speech", "translate", "makeymakey", "microbit", "ev3", "boost", "wedo2", "goDirect", "coreExtensions", "nishiowoDectalk"};

bool Parser::loadExtensions(const nlohmann::json &json) {
    bool hasNativeExts = false;
#if defined(ENABLE_NATIVE_EXTENSIONS) || defined(ENABLE_CUSTOM_EXTENSIONS)
    const std::string folder = OS::getScratchFolderLocation() + "extensions/";

#ifdef __APPLE__
    constexpr const char *libraryExtension = ".dylib";
#else
    constexpr const char *libraryExtension = ".so";
#endif
    if (!json.contains("extensions")) return false;
    for (const std::string &targetID : json["extensions"]) {
        if (std::find(builtInExtensions.begin(), builtInExtensions.end(), targetID) != builtInExtensions.end()) continue;

#ifdef ENABLE_NATIVE_EXTENSIONS
        const std::string &nativePath = folder + targetID + libraryExtension;
        if (FileSystem::fileExists(nativePath)) {
            void *extensionHandle = dlopen(nativePath.c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!extensionHandle) {
                Log::logCritical("Failed to load native extension, '" + targetID + "', dlerror: " + dlerror(), false);
            } else {
                Log::log("Loaded native extension: " + targetID);
                hasNativeExts = true;
            }
            continue;
        }
#endif
#ifdef ENABLE_CUSTOM_EXTENSIONS
        std::unique_ptr<extensions::Extension> loadedExt = nullptr;
        std::ifstream in;

        const auto &tryPath = [&](std::string path) {
            in.open(path, std::ios::binary | std::ios::in);
            auto result = extensions::parseMetadata(in);
            if (result.has_value() && result.value()->id == targetID) {
                loadedExt = std::move(result.value());
            } else {
                if (!result.has_value()) Log::logWarning("Error while loading extension metadata: " + result.error());
                in.close();
                in.clear();
            }
        };

        const std::string romFSPath = OS::getRomFSLocation() + "extensions/" + targetID + ".see";
#ifdef USE_CMAKERC
        bool fromCmrc = false;

        const auto &fs = cmrc::romfs::get_filesystem();

        std::unique_ptr<std::istringstream> romfsStream = nullptr;
        if (fs.exists(romFSPath)) {
            const auto &romfsIn = fs.open(romFSPath);
            romfsStream = std::make_unique<std::istringstream>(std::string(romfsIn.begin(), romfsIn.end()));

            auto result = extensions::parseMetadata(*romfsStream);
            if (result.has_value() && result.value()->id == targetID) {
                loadedExt = std::move(result.value());
                fromCmrc = true;
            } else if (!result.has_value()) Log::logWarning("Error while loading extension metadata: " + result.error());
        }
#else
        if (FileSystem::fileExists(romFSPath)) {
            tryPath(romFSPath);
        }
#endif

        const std::string luaPath = folder + targetID + ".see";
        if (FileSystem::fileExists(luaPath) && !loadedExt) {
            tryPath(luaPath);
        }

        if (!loadedExt) {
            const auto &scanDirectory = [&](std::string path) {
                auto files = FileSystem::listDirectory(path);
                if (files.has_value()) {
                    for (const auto &file : files.value()) {
                        if (file.size() < 4) continue;
                        if (file.compare(file.size() - 4, 4, ".see") != 0) continue;

                        in.open(folder + file, std::ios::binary | std::ios::in);
                        auto result = extensions::parseMetadata(in);

                        if (result.has_value() && result.value()->id == targetID) {
                            loadedExt = std::move(result.value());
                            break;
                        }
                        if (!result.has_value()) Log::logWarning("Error while loading extension metadata: " + result.error());
                        in.close();
                        in.clear();
                    }
                }
            };

#if !defined(USE_CMAKERC) // I'm lazy, someone else can add this in the future.
            scanDirectory(OS::getRomFSLocation() + "extensions");
#endif
            if (!loadedExt) {
                scanDirectory(folder);
            }
        }

        if (loadedExt) {
#ifdef USE_CMAKERC
            if (fromCmrc) {
                extensions::loadLua(loadedExt.get(), *romfsStream);
            } else
#endif
                extensions::loadLua(loadedExt.get(), in);
            Scratch::extensions.push_back(std::move(loadedExt));
            in.close();
            Log::log("Successfully loaded Lua extension: " + targetID);
            continue;
        }

        Log::logError("Failed to find extension: " + targetID);
#endif
    }

#ifdef ENABLE_CUSTOM_EXTENSIONS
    extensions::registerHandlers();
#endif
#endif
    return hasNativeExts;
}
