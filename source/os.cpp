#include "os.hpp"
#include "render.hpp"
#include "settings.hpp"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <os.hpp>
#include <ostream>
#include <render.hpp>
#include <string>

#include <cerrno>
#include <dirent.h>
#include <sys/stat.h>

#if defined(__3DS__)
#include <malloc.h>
#elif defined(__WIIU__)
#include <nn/act.h>
#elif defined(__SWITCH__)
extern char nickname[0x21];
#elif defined(VITA)
#include <psp2/apputil.h>
#include <psp2/system_param.h>
#elif defined(__PS4__)
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#elif defined(WII)
#include <gccore.h>
#include <ogc/conf.h>
#elif defined(__NDS__)
#include <nds.h>
#endif

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <lmcons.h>
#include <shlwapi.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <pwd.h>
#include <sys/types.h>
#endif

// PS4 implementation of logging
#ifdef __PS4__
char logBuffer[1024];

void Log::log(std::string message, bool printToScreen) {
    if (printToScreen) {
        snprintf(logBuffer, 1023, "<SE!> %s\n", message.c_str());
        sceKernelDebugOutText(0, logBuffer);
    }
}
void Log::logWarning(std::string message, bool printToScreen) {
    if (printToScreen) {
        snprintf(logBuffer, 1023, "<SE!> Warning: %s\n", message.c_str());
        sceKernelDebugOutText(0, logBuffer);
    }
}
void Log::logError(std::string message, bool printToScreen) {
    if (printToScreen) {
        snprintf(logBuffer, 1023, "<SE!> Error: %s\n", message.c_str());
        sceKernelDebugOutText(0, logBuffer);
    }
}
void Log::writeToFile(std::string message) {
}

void Log::deleteLogFile() {
}

#else
void Log::log(std::string message, bool printToScreen) {
    if (printToScreen) std::cout << message << std::endl;
    writeToFile(message);
}

void Log::logWarning(std::string message, bool printToScreen) {
    if (printToScreen)
        std::cout << "\x1b[1;33m" << "Warning: " << message << "\x1b[0m" << std::endl;
    writeToFile("Warning: " + message);
}

void Log::logError(std::string message, bool printToScreen) {
    if (printToScreen)
        std::cerr << "\x1b[1;31m" << "Error: " << message << "\x1b[0m" << std::endl;

    writeToFile("Error: " + message);
}
void Log::writeToFile(std::string message) {
    if (Render::debugMode) {
        std::string filePath = OS::getScratchFolderLocation() + "log.txt";
        std::ofstream logFile;
        logFile.open(filePath, std::ios::app);
        if (logFile.is_open()) {
            logFile << message << std::endl;
            logFile.close();
        } else {
            std::cerr << "Could not open log file: " << filePath << std::endl;
        }
    }
}

void Log::deleteLogFile() {
    std::string filePath = OS::getScratchFolderLocation() + "/log.txt";
    if (std::remove(filePath.c_str()) != 0) {
        Log::logError("Failed to delete log file: " + std::string(std::strerror(errno)));
    }
}

#endif

// Nintendo DS Timer implementation
#ifdef __NDS__
Timer::Timer() {
    start();
}
void Timer::start() {
    startTime = cpuGetTiming();
}
int Timer::getTimeMs() {
    uint64_t currentTime = cpuGetTiming();
    // CPU timing is in units based on the bus clock (33.513982 MHz)
    // Convert to milliseconds: (ticks * 1000) / BUS_CLOCK
    return static_cast<int>((currentTime - startTime) * 1000 / BUS_CLOCK);
}

// Wii's std::chrono support is still pretty bad
#elif defined(WII)

Timer::Timer() {
    start();
}

void Timer::start() {
    startTime = gettick();
}

int Timer::getTimeMs() {
    u64 currentTime = gettick();
    return ticks_to_millisecs(currentTime - startTime);
}

// std::chrono on PS4 updates slowly
#elif defined(__PS4__)

Timer::Timer() {
    start();
}

void Timer::start() {
    startTime = sceKernelReadTsc() * 1000;
}

int Timer::getTimeMs() {
    uint64_t currentTime = sceKernelReadTsc() * 1000;
    return static_cast<int>((currentTime - startTime) / sceKernelGetTscFrequency());
}

// everyone else...
#else
Timer::Timer() {
    start();
}

void Timer::start() {
    startTime = std::chrono::high_resolution_clock::now();
}

int Timer::getTimeMs() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);

    return static_cast<int>(duration.count());
}

#endif

bool Timer::hasElapsed(int milliseconds) {
    return getTimeMs() >= milliseconds;
}

bool Timer::hasElapsedAndRestart(int milliseconds) {
    if (hasElapsed(milliseconds)) {
        start();
        return true;
    }
    return false;
}

bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
namespace OS {
bool toExit = false;
}

std::string OS::getScratchFolderLocation() {
    if (!loadedSettings) {
        loadedSettings = true;

        nlohmann::json json = SettingsManager::getConfigSettings();

        if (json.contains("ProjectsPath") && json["ProjectsPath"].is_string() && json.contains("UseProjectsPath") && json["UseProjectsPath"].is_boolean() && json["UseProjectsPath"] == true) customProjectsPath = new std::string(json["ProjectsPath"].get<std::string>());
    }

    if (customProjectsPath != nullptr) return *customProjectsPath;

    const std::string prefix = getFilesystemRootPrefix();
#ifdef __WIIU__
    return prefix + "/wiiu/scratch-wiiu/";
#elif defined(__SWITCH__)
    return "/switch/scratch-nx/";
#elif defined(WII)
    return "/apps/scratch-wii/";
#elif defined(GAMECUBE)
    return "/scratch-gamecube/";
#elif defined(VITA)
    return prefix + "data/scratch-vita/";
#elif defined(__PS4__)
    return "/data/scratch-ps4/";
#elif defined(__3DS__)
    return prefix + "/3ds/scratch-everywhere/";
#elif defined(__EMSCRIPTEN__)
    return "/scratch-everywhere/";
#elif defined(__NDS__)
    return prefix + "/scratch-ds/";
#elif defined(WEBOS)
    return prefix + "projects/";
#else
    return "scratch-everywhere/";
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
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
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

bool OS::initWifi() {
#ifdef __3DS__
    u32 wifiEnabled = false;
    ACU_GetWifiStatus(&wifiEnabled);
    if (wifiEnabled == AC_AP_TYPE_NONE) {
        int ret;
        uint32_t SOC_ALIGN = 0x1000;
        uint32_t SOC_BUFFERSIZE = 0x100000;
        uint32_t *SOC_buffer = NULL;

        SOC_buffer = (uint32_t *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
        if (SOC_buffer == NULL) {
            Log::logError("memalign: failed to allocate");
            return false;
        } else if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
            std::ostringstream err;
            err << "socInit: 0x" << std::hex << std::setw(8) << std::setfill('0') << ret;
            Log::logError(err.str());
            return false;
        }
    }
#endif
    return true;
}

void OS::deInitWifi() {
#ifdef __3DS__
    u32 wifiEnabled = false;
    ACU_GetWifiStatus(&wifiEnabled);
    if (wifiEnabled != AC_AP_TYPE_NONE)
        socExit();
#endif
}

std::string OS::getUsername() {
#ifdef __WIIU__
    int16_t miiName[256];
    nn::act::GetMiiName(miiName);
    return std::string(miiName, miiName + sizeof(miiName) / sizeof(miiName[0]));
#elif defined(__SWITCH__)
    if (std::string(nickname) != "") return std::string(nickname);
#elif defined(VITA)
    static SceChar8 username[SCE_SYSTEM_PARAM_USERNAME_MAXSIZE];
    sceAppUtilSystemParamGetString(
        SCE_SYSTEM_PARAM_ID_USERNAME,
        username,
        sizeof(username));
    return std::string(reinterpret_cast<char *>(username));
#elif defined(WII)

    CONF_Init();
    u8 nickname[24];
    if (CONF_GetNickName(nickname) != 0) {
        return std::string(reinterpret_cast<char *>(nickname));
    }
#elif defined(__PS4__)
    char username[32];
    int userId;
    sceUserServiceGetInitialUser(&userId);
    if (sceUserServiceGetUserName(userId, username, 31) == 0) {
        return std::string(reinterpret_cast<char *>(username));
    }
#elif defined(__3DS__)
    std::array<u16, 0x1C / sizeof(u16)> block{};

    cfguInit();
    CFGU_GetConfigInfoBlk2(0x1C, 0xA0000, reinterpret_cast<u8 *>(block.data()));
    cfguExit();

    std::string username(0x14, '\0');
    const ssize_t length = utf16_to_utf8(reinterpret_cast<u8 *>(username.data()), block.data(), username.size());

    if (length > 0) {
        username.resize(static_cast<size_t>(length));
        return username;
    }
#elif defined(_WIN32) || defined(_WIN64)
    TCHAR username[UNLEN + 1];
    DWORD size = UNLEN + 1;
    if (GetUserName((TCHAR *)username, &size)) return std::string(username);
#elif defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if (pw) return std::string(pw->pw_name);
#endif
    return "Player";
}

void OS::createDirectory(const std::string &path) {
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');

    size_t pos = 0;
    while ((pos = p.find('/', pos)) != std::string::npos) {
        std::string dir = p.substr(0, pos++);
        if (dir.empty()) continue;
        if (dir == OS::getFilesystemRootPrefix()) continue; // Fixes DS but hopefully doesn't negatively affect other platforms????

        struct stat st;
        if (stat(dir.c_str(), &st) != 0) {
#ifdef _WIN32
            if (_mkdir(dir.c_str()) != 0 && errno != EEXIST) {
#else
            if (mkdir(dir.c_str(), 0777) != 0 && errno != EEXIST) {
#endif
                throw OS::DirectoryCreationFailed(dir, errno);
            }
        }
    }
}

void OS::renameFile(const std::string &originalPath, const std::string &newPath) {
    rename(originalPath.c_str(), newPath.c_str());
}

void OS::removeDirectory(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        throw OS::DirectoryNotFound(path, errno);
    }

    if (!(st.st_mode & S_IFDIR)) {
        throw OS::NotADirectory(path, errno);
    }

#ifdef _WIN32
    std::wstring wpath(path.size(), L' ');
    wpath.resize(std::mbstowcs(&wpath[0], path.c_str(), path.size()) + 1);

    SHFILEOPSTRUCTW options = {0};
    options.wFunc = FO_DELETE;
    options.pFrom = wpath.c_str();
    options.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    if (SHFileOperationW(&options) != 0) {
        throw OS::DirectoryRemovalFailed(path, errno);
    }
#else
    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        throw OS::DirectoryOpenFailed(path, errno);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string fullPath = path + "/" + entry->d_name;

        struct stat entrySt;
        if (stat(fullPath.c_str(), &entrySt) == 0) {
            if (S_ISDIR(entrySt.st_mode)) {
                removeDirectory(fullPath);
            } else {
                if (remove(fullPath.c_str()) != 0) {
                    throw OS::FileRemovalFailed(fullPath, errno);
                }
            }
        }
    }

    closedir(dir);

    if (rmdir(path.c_str()) != 0) {
        throw OS::DirectoryRemovalFailed(path, errno);
    }
#endif
}

bool OS::fileExists(const std::string &path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

std::string OS::parentPath(const std::string &path) {
    size_t pos = path.find_last_of("/\\");
    if (std::string::npos != pos)
        return path.substr(0, pos);
    return "";
}
