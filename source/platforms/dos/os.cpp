#include <log.hpp>
#include <os.hpp>
#include <unistd.h>
#include <dir.h>
#include <dos.h>
#include <sys/stat.h>

// https://www.delorie.com/djgpp/doc/incs/
// https://pubs.opengroup.org/onlinepubs/9699919799.2018edition/basedefs/sys_stat.h.html

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

    // const char* config_name = "SECONFIG\\";


    struct find_t ffblk;
    int done;

    // Search for any file/directory matching the pattern
    done = _dos_findfirst("DOS", _A_SUBDIR, &ffblk);
    
    // Read, write, execute/search by others
    const mode_t FOLDER_MODE = S_IRWXO;

    auto directory_handling = [&](const char* buffer) {
        struct stat st;
        if (stat(buffer, &st) == 0) {
            Log::log("Found SECONFIG Directory");
            return buffer;
        }

        // Log::logWarning("Didn't Find SECONFIG Directory");
        // Log::log("Creating SECONFIG Directory");
        mkdir(buffer, FOLDER_MODE);
        return buffer;
    };

    while (!done) {
        // Check if it is actually a directory and not '.' or '..'
        if ((ffblk.attrib & _A_SUBDIR) && strcmp(ffblk.name, ".") != 0 && strcmp(ffblk.name, "..") != 0) {
            path += ffblk.name;
            path += "\\SECONFIG\\\0";

            // Check if Directory Already Exists
            return directory_handling(path.c_str());
        }
        done = _dos_findnext(&ffblk);
    }

    // If Didn't Find Main DOS Directory, then just use ours
    path += "SECONFIG\\\0";
    
    return directory_handling(path.c_str());
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;

    char* buf = (char *)malloc(MAXPATH);
    if (!(buf && getcwd(buf, MAXPATH))) {
        Log::logCritical("Error When Trying To Figure Out Current Working Directory.", false);
        return "";
    }

    
    buf = strcat(buf, "SCH-EVWH");
    std::string prefix;
    prefix = getFilesystemRootPrefix();
    for (int i = 0; i < prefix.length(); i++) {
        buf[i] = prefix[i];
    }

    struct stat st;
    const mode_t FOLDER_MODE = S_IRWXO;
    if (!(stat("SCH-EVWH", &st) == 0)) {
        Log::logWarning("Didn't Find SECONFIG Directory");
        Log::log("Creating SECONFIG Directory");
        mkdir(buf, FOLDER_MODE);
    }

    buf = strcat(buf, "\\\0");


    return buf; 
}

std::string OS::getRomFSLocation() {
    char* buf = (char *)malloc(MAXPATH);
    if (!(buf && getcwd(buf, MAXPATH))) {
        Log::logCritical("Error When Trying To Figure Out Current Working Directory.", false);
        return "";
    }

    char* old_dir = buf;
    buf = strcat(buf, "romfs");
    std::string prefix;
    prefix = getFilesystemRootPrefix();
    for (int i = 0; i < prefix.length(); i++) {
        buf[i] = prefix[i];
        old_dir[i] = prefix[i];
    }

    struct stat st;
    const mode_t FOLDER_MODE = S_IRWXO;
    if (!(stat("romfs", &st) == 0)) {
        Log::logError("Didn't Find ROMFS Directory");
        old_dir = strcat(old_dir, "\\\0");
        return old_dir;
    }

    buf = strcat(buf, "\\\0");

    return buf; 
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