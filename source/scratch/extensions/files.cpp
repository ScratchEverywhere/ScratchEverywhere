#include "files.hpp"
#include "../input.hpp"
#include "../os.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

#ifdef __WIIU__
#include <whb/sdcard.h>
#endif

namespace extensions::files {
static inline std::string getPrefix(bool rootfs, std::string extensionId) {
    std::string prefix = "";
    if (rootfs) {
#ifdef __WIIU__
        prefix = std::string(WHBGetSdCardMountPath());
#elif defined(GAMECUBE)
        prefix = "";
#elif defined(__3DS__)
        prefix = "sdmc:/";
#elif defined(VITA)
        prefix = "ux0:data/";
#elif defined(__PC__)
        prefix = "/"; // TODO: Windows handling, why does it need to exist.
#endif
    } else {
        prefix = OS::getScratchFolderLocation() + "/extension-data/" + extensionId + "/";
        std::filesystem::create_directories(prefix);
    }

    return prefix;
}

std::function<std::string(const std::string path)> read(bool rootfs, std::string extensionId) {
    const std::string prefix = getPrefix(rootfs, extensionId);
    return [=](const std::string path) -> std::string {
        if (path[0] == '.' && path[1] == '.' && path[2] == '/') {
            Log::logWarning("An extension tried to access an invalid path.");
            return "";
        }
        std::ifstream f(prefix + (Input::isAbsolutePath(path) ? path.substr(1) : path));
        return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    };
}

std::function<void(const std::string path, const std::string content)> write(bool rootfs, std::string extensionId) {
    const std::string prefix = getPrefix(rootfs, extensionId);
    return [=](const std::string path, const std::string content) {
        if (path[0] == '.' && path[1] == '.' && path[2] == '/') {
            Log::logWarning("An extension tried to access an invalid path.");
            return;
        }
        std::ofstream(prefix + (Input::isAbsolutePath(path) ? path.substr(1) : path)) << content; // TODO: Error handling
    };
}

std::function<void(const std::string path, const std::string content)> append(bool rootfs, std::string extensionId) {
    const std::string prefix = getPrefix(rootfs, extensionId);
    return [=](const std::string path, const std::string content) {
        if (path[0] == '.' && path[1] == '.' && path[2] == '/') {
            Log::logWarning("An extension tried to access an invalid path.");
            return;
        }
        std::ofstream(prefix + (Input::isAbsolutePath(path) ? path.substr(1) : path), std::ios::app) << content; // TODO: Error handling
    };
}

std::function<void(const std::string path)> mkdir(bool rootfs, std::string extensionId) {
    const std::string prefix = getPrefix(rootfs, extensionId);
    return [=](const std::string path) {
        if (path[0] == '.' && path[1] == '.' && path[2] == '/') {
            Log::logWarning("An extension tried to access an invalid path.");
            return;
        }
        std::filesystem::create_directory(prefix + (Input::isAbsolutePath(path) ? path.substr(1) : path)); // TODO: Error handling
    };
}

std::function<std::vector<std::string>(const std::string path)> ls(bool rootfs, std::string extensionId) {
    const std::string prefix = getPrefix(rootfs, extensionId);
    return [=](const std::string path) -> std::vector<std::string> {
        if (path[0] == '.' && path[1] == '.' && path[2] == '/') {
            Log::logWarning("An extension tried to access an invalid path.");
            return {};
        }
        std::vector<std::string> ret;
        std::transform(std::filesystem::directory_iterator(prefix + (Input::isAbsolutePath(path) ? path.substr(1) : path)), std::filesystem::directory_iterator{}, std::back_inserter(ret), [](const std::filesystem::directory_entry &entry) { return entry.path().filename().string(); });
        return ret;
    };
}
} // namespace extensions::files
