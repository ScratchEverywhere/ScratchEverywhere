#include <libdragon.h>
#include <rspq_profile.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/gl_integration.h>

#include <filesystem.hpp>
#include <log.hpp>
#include <os.hpp>
#include <timer.hpp>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    /* TODO: Implement */
    debug_init_isviewer();
	debug_init_usblog();
    
    dfs_init(DFS_DEFAULT_LOCATION);
    joypad_init();

    rdpq_init();
    joypad_init();
    

    return true;
}

void OS::deinit() {
}

std::string OS::getPlatform() {
    return "Ultra 64";
}

bool OS::isEnhancedPlatform() {
    return 0; /* TODO: Detect Expansion Pack */
}

static std::string filesystemRoot = "";
std::string OS::getFilesystemRootPrefix() {
    /* TODO: Implement this whole thing */
    return filesystemRoot;
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return getFilesystemRootPrefix() + "";
}

std::string OS::getRomFSLocation() {
    return "rom:/";
}

bool OS::isOnline() {
    return false;
}

bool OS::initWifi() {
    return false;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
    // TODO: Implement
    return "Player";
}
