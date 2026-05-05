#include <filesystem.hpp>
#include <os.hpp>

#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <lmcons.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <windows.h>
#elif defined(__HAIKU__)
#include <FindDirectory.h>
#include <Path.h>
#else
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>
#endif

nonstd::expected<void, std::string> FileSystem::createDirectory(const std::string &path) {
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
                return nonstd::make_unexpected("Failed to create directory, " + dir + ", " + std::to_string(errno));
            }
        }
    }

    return {};
}

void FileSystem::renameFile(const std::string &originalPath, const std::string &newPath) {
    rename(originalPath.c_str(), newPath.c_str());
}

nonstd::expected<void, std::string> FileSystem::removeDirectory(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return nonstd::make_unexpected("Directory not found, " + path + ", " + std::to_string(errno));
    }

    if (!(st.st_mode & S_IFDIR)) {
        return nonstd::make_unexpected("Not a directory, " + path + ", " + std::to_string(errno));
    }

#ifdef _WIN32
    std::wstring wpath(path.size(), L' ');
    wpath.resize(std::mbstowcs(&wpath[0], path.c_str(), path.size()) + 1);

    SHFILEOPSTRUCTW options = {0};
    options.wFunc = FO_DELETE;
    options.pFrom = wpath.c_str();
    options.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    if (SHFileOperationW(&options) != 0) {
        return nonstd::make_unexpected("Directory removal failed, " + path + ", " + std::to_string(errno));
    }
#else
    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        return nonstd::make_unexpected("Directory open failed, " + path + ", " + std::to_string(errno));
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
                auto potentialError = removeDirectory(fullPath);
                if (!potentialError.has_value()) return nonstd::make_unexpected(potentialError.error());
            } else {
                if (remove(fullPath.c_str()) != 0) {
                    return nonstd::make_unexpected("File removal failed, " + fullPath + ", " + std::to_string(errno));
                }
            }
        }
    }

    closedir(dir);

    if (rmdir(path.c_str()) != 0) {
        return nonstd::make_unexpected("Directory removal failed, " + path + ", " + std::to_string(errno));
    }
#endif

    return {};
}

bool FileSystem::fileExists(const std::string &path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

std::string FileSystem::parentPath(const std::string &path) {
    size_t pos = path.find_last_of("/\\");
    if (std::string::npos != pos)
        return path.substr(0, pos);
    return "";
}
