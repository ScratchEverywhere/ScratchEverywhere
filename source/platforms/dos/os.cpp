#include <log.hpp>
#include <os.hpp>
#include <unistd.h>
#include <dir.h>
#include <dos.h>
#include <sys/stat.h>

static char nickname[0x21];

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    return true;
}

void OS::deinit() {
}

std::string OS::getPlatform() {
    return "DOS";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    unsigned int disk;
    _dos_getdrive(&disk);

    if ( ((signed int)disk) <= 0) {
        Log::logCritical("Could not find Current Drive.", false);
        return "";
    }

    disk += 'A' - 1;

    char prefix[] = " :\\\0";
    prefix[0] = disk;

    return prefix;
}

std::string OS::getConfigFolderLocation() {
    const std::string prefix = getFilesystemRootPrefix();
    std::string path = getScratchFolderLocation();

    // stat returns 0 if it successfully finds the path && Check if the S_IFDIR bit is set in st_mode
    /*
    struct stat info;
    if (stat(path, &info) == 0 && (info.st_mode & S_IFDIR) ) {
        // Add Code Later
    } else {
        Log::logCritical("Could not find RoamingData path.", false);
        Log::log("Creating Scratch-Everywhere Directory");
    } 
    */

    return path;
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;

    char* buf = (char *)malloc(MAXPATH);
    if (!(buf && getcwd(buf, MAXPATH))) {
        Log::logCritical("Error When Trying To Figure Out Current Working Directory.", false);
        return "";
    }

    strcat(buf, "\\scratch-everywhere\\");
    // const char* se_path = ;// buf + "\\scratch-everywhere\\"

    return buf;
}

std::string OS::getRomFSLocation() {
    return "";
}

bool OS::isOnline() {
    /*
    // TODO: Add an actual way to check if online
    #if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    return true;
    #endif 
    */
    return false;
}

bool OS::initWifi() {
    return false;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
    if (std::string(nickname) != "") {
        return std::string(nickname);
    }
    return "Player";
}