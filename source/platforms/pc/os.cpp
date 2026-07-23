#include <log.hpp>
#include <os.hpp>

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#include <io.h>
#include <lmcons.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <windows.h>
#else
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__HAIKU__)
#include <haiku.h>
#include <kits/user/User.h>
#endif
#endif
#include <__getbasepath/internal.h>
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
#include <sstream>
#endif

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
    return "PC";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
#if defined(_WIN32) || defined(_WIN64)
    return std::filesystem::path(std::getenv("SystemDrive")).string();
#else
    return "";
#endif
}

std::string OS::getConfigFolderLocation() {
    const std::string prefix = getFilesystemRootPrefix();
    std::string path = getScratchFolderLocation();
#if defined(_WIN32) || defined(_WIN64)
    PWSTR szPath = NULL;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &szPath) == S_OK) {
        path = (std::filesystem::path(szPath) / "scratch-everywhere" / "").string();
        CoTaskMemFree(szPath);
    } else {
        Log::logError("Could not find RoamingData path.");
    }
#elif defined(__APPLE__)
    const char *home = std::getenv("HOME");
    if (home) {
        path = std::string(home) + "/Library/Application Support/scratch-everywhere/";
    }
#elif defined(__HAIKU__)
    BPath bpath;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &bpath) == B_OK) {
        path = (std::filesystem::path(bpath.Path()) / "scratch-everywhere" / "").string();
    }
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
    const char *xdgHome = std::getenv("XDG_CONFIG_HOME");
    if (xdgHome && xdgHome[0] != '\0') {
        path = (std::filesystem::path(xdgHome) / "scratch-everywhere" / "").string();
    } else {
        const char *home = std::getenv("HOME");
        if (!home) {
            struct passwd *pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (home) path = (std::filesystem::path(home) / ".config" / "scratch-everywhere" / "").string();
    }
#endif
    return path;
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;

    const char *basepath = __getbasepath();
    std::string cpp_basepath = basepath ? basepath : "";
#if defined(_WIN32) || defined(_WIN64)
    return cpp_basepath + "scratch-everywhere\\";
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
    const char *path = std::getenv("PATH");
    if (path && path[0] != '\0') {
        std::string str(path);
        std::string buf;
        std::stringstream ss(str);

        std::vector<std::string> tokens;

        while (getline(ss, buf, ':')) {
            tokens.push_back(buf);
        }

        cpp_basepath.pop_back();

        if (std::find(tokens.begin(), tokens.end(), cpp_basepath) != tokens.end()) {
            const char *xdgData = std::getenv("XDG_DATA_HOME");
            if (xdgData && xdgData[0] != '\0') {
                return (std::filesystem::path(xdgData) / "scratch-everywhere" / "").string();
            } else {
                const char *home = std::getenv("HOME");
                if (!home) {
                    struct passwd *pw = getpwuid(getuid());
                    if (pw) home = pw->pw_dir;
                }
                if (home) return (std::filesystem::path(home) / ".local" / "share" / "scratch-everywhere" / "").string();
            }
        } else {
            return cpp_basepath + "/scratch-everywhere/";
        }
    }
#else
    return cpp_basepath + "scratch-everywhere/";
#endif
}

std::string OS::getRomFSLocation() {
    return "";
}

bool OS::isOnline() {
    // TODO: Add an actual way to check if online
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    return true;
#endif
    return false;
}

bool OS::initWifi() {
    return true;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
#if defined(_WIN32) || defined(_WIN64)
    TCHAR username[UNLEN + 1];
    DWORD size = UNLEN + 1;
    if (GetUserName((TCHAR *)username, &size)) return std::string(username);
#elif defined(__HAIKU__)
    BUser user;
    BString username;
    if (user.InitCheck() == B_OK) {
        user.GetUserName(&username);
        return std::string(username.String());
    }
#elif defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if (pw) return std::string(pw->pw_name);
#endif
    return "Player";
}
