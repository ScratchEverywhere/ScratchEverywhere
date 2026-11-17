#include <filesystem.hpp>
#include <log.hpp>
#include <os.hpp>
#include <platforms/xbox360/diagnostics.hpp>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;

bool init() {
    toExit = false;
    Xbox360Diagnostics::open();
    const auto directory = FileSystem::createDirectory(getConfigFolderLocation());
    if (!directory) Log::logWarning("Configuration directory unavailable: " + directory.error());
    return true;
}
void deinit() {
    delete customProjectsPath;
    customProjectsPath = nullptr;
    loadedSettings = false;
    Log::log("Xbox 360 platform shutdown complete");
    Xbox360Diagnostics::close();
}
std::string getFilesystemRootPrefix() { return "game:"; }
std::string getRomFSLocation() { return "game:/romfs/"; }
std::string getConfigFolderLocation() { return "game:/scratch-everywhere-xbox360/"; }
std::string getScratchFolderLocation() { return getConfigFolderLocation(); }
std::string getPlatform() { return "Xbox 360"; }
bool isEnhancedPlatform() { return false; }
bool isOnline() { return false; }
bool initWifi() {
    Log::logWarning("Xbox 360 networking is not implemented");
    return false;
}
void deInitWifi() {}
std::string getUsername() { return "Player"; }
} // namespace OS
